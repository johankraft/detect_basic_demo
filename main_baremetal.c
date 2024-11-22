
#include "dfmStoragePort.h"
#include "main.h"
#include "stdint.h"
#include "stdarg.h"

#include "iot_uart.h"

#include "trcRecorder.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"

#include "demo_alert.h"
#include "demo_states.h"
#include "demo_user_events.h"
#include "demo_isr.h"

volatile int ButtonPressed = 0;


/******************************************************************************
 * REGISTER_TASK
 *
 * The TraceRecorder functions for RTOS task tracing is applicable also for
 * bare-metal systems, for example to show "idle time" in the trace views.
 *
 * This macro is used to manually register two tasks, "IDLE" and "main-thread".
 * where IDLE represents the HAL_Delay() call in main_superloop(). The macro
 * stores a Task ID together with a display name for Tracealyzer. The Task IDs
 * are passed as arguments to xTraceTaskSwitch() for tracing when entering and
 * leaving the HAL_Delay call.
 *
 * This is only needed for bare-metal systems. When using a supported RTOS
 * such as FreeRTOS, the tasks are registered and traced automatically.
 *****************************************************************************/
#define REGISTER_TASK(ID, name) xTraceTaskRegisterWithoutHandle((void*)ID, name, 0)

#define TASK_IDLE 100
#define TASK_MAIN 101

static void DemoUpdate(int counter);
static void DemoInit(void);

#if (TRC_HWTC_TYPE != TRC_FREE_RUNNING_32BIT_INCR)
#error Only hardware ports with TRC_HWTC_TYPE == TRC_FREE_RUNNING_32BIT_INCR are supported.
#endif

/*****
 * Changes:
 * - Added initial Stopwatch API. See serial output from alerts.
 * - Added xTracePause() and xTraceResume() to avoid reseting the trace.
 * - Optimized stopwatch API. Overhead now just 31-108 clock cycles on Arm.
 * - Solved hard fault issue in DFM caused by GCC bug on -O3.
 * - Demo app now remembers last test case, so it cycles through the alerts. Didn't require linker file changes (attribute noinit). Can be used to implement retained memory for any GCC target.
 * - Created a unit test framework and added 7 tests. Probably not complete, but seems decent for now. Consider adding a few more tests.
 * - Added new alert type and symptoms (server must be updated, see dfmCodes.h)
 * - Added print and clear functions in the API.
 * - Added id field, where 0 is invalid. Allows for checking if a stopwatch has been initialized.
 * - Fixed warnings and removed old invalid include paths from project.
 *
 * TODO:
 * - Test loading alerts into Detect server.
 * - Move to a separate dfm file (clean up).
 * - Consider adding a few more test cases.
 * - Improve the demo application.
 * - Allow for using DFM_TRAP without restarting.
 */

extern char cDfmPrintBuffer[128];

typedef struct {
	uint32_t start_time;
	uint32_t expected_duration;
	uint32_t high_watermark;
	uint32_t id; /* Starts with 1, 0 is invalid */
	const char* name;
} dfmStopwatch_t;


//#define INCLUDE_STOPWATCH
//#define STOPWATCH_OVERHEAD_MEASUREMENT
//#define INCLUDE_STOPWATCH_UNIT_TEST

#define DFM_CFG_MAX_STOPWATCHES 4

dfmStopwatch_t stopwatches[DFM_CFG_MAX_STOPWATCHES];
int32_t stopwatch_count = 0;

/* PUBLIC API */

dfmStopwatch_t* xDfmStopwatchCreate(const char* name, uint32_t expected_max);

void vDfmStopwatchBegin(dfmStopwatch_t* sw);

void vDfmStopwatchEnd(dfmStopwatch_t* sw);

void vDfmStopwatchClearAll(void);

void vDfmStopwatchPrintAll(void);

/* Private functions */
void prvStopwatchPrint(dfmStopwatch_t* sw, char* testresult);
void prvDfmPrintHeader(void);
void prvDfmStopwatchAlert(char* msg, int high_watermark, int stopwatch_index);

dfmStopwatch_t* xDfmStopwatchCreate(const char* name, uint32_t expected_max)
{
	TRACE_ALLOC_CRITICAL_SECTION();
	dfmStopwatch_t* sw = NULL;

	TRACE_ENTER_CRITICAL_SECTION();

	if (stopwatch_count < DFM_CFG_MAX_STOPWATCHES)
	{
		sw = &stopwatches[stopwatch_count];

		sw->expected_duration = expected_max;
		sw->name = name;
		sw->high_watermark = 0;
		sw->start_time = 0;
		sw->id = stopwatch_count + 1; /* starts with 1, id 0 denotes invalid/uninitialized. */

		stopwatch_count++;
	}
	TRACE_EXIT_CRITICAL_SECTION();

	return sw;
}

#ifdef INCLUDE_STOPWATCH_UNIT_TEST
static volatile uint32_t hwtc_count_simulated = 0;
#define DFM_CFG_HWTC_COUNT hwtc_count_simulated
#else
#define DFM_CFG_HWTC_COUNT TRC_HWTC_COUNT
#endif


void vDfmStopwatchBegin(dfmStopwatch_t* sw)
{
	if (sw != NULL)
	{
		sw->start_time = DFM_CFG_HWTC_COUNT;
	}
}

extern void prvAddTracePayload(void);

void vDfmStopwatchEnd(dfmStopwatch_t* sw)
{
	uint32_t end_time = DFM_CFG_HWTC_COUNT;

	if (sw != NULL)
	{
		/* Overflow in HWTC_COUNT is OK. Say that start_time is 0xFFFFFFFF
		 * and end_time is 1, then we get (1 - 0xFFFFFFFF) = 2.
		 * This since 0xFFFFFFFF equals -1 (signed) and 1 - (-1)) = 2 */
		uint32_t duration = end_time - sw->start_time;

		/* Alert if new highest value is found, assuming it is above expected_duration (provides a lower threshold) */
		if (duration > sw->high_watermark)
		{
			sw->high_watermark = duration;

			if (duration > sw->expected_duration)
			{
				snprintf(cDfmPrintBuffer, sizeof(cDfmPrintBuffer), "Stopwatch %s, new high: %u", sw->name, (unsigned int)sw->high_watermark);
				DFM_DEBUG_PRINT(cDfmPrintBuffer);

#if !defined(INCLUDE_STOPWATCH_UNIT_TEST)
				prvDfmStopwatchAlert(cDfmPrintBuffer, sw->high_watermark, sw->id);
#endif
			}
		}
	}
}

void prvStopwatchPrint(dfmStopwatch_t* sw, char* testresult)
{
	if (sw != NULL)
	{
		if (sw->name == NULL)
		{
			sw->name = "NULL";
		}
		printf("%12u, %-15s %10u, %12u, %14u %s\n", (unsigned int)sw->id, sw->name, (unsigned int)sw->start_time, (unsigned int)sw->expected_duration, (unsigned int)sw->high_watermark, testresult);
	}
	else
	{
		printf("Stopwatch is NULL (ERROR!)\n");
	}
}

void prvDfmPrintHeader(void)
{
	printf("Stopwatch ID, Name,           Last Start, Expected Max, High Watermark\n");
}

void vDfmStopwatchPrintAll(void)
{
	prvDfmPrintHeader();
	for (int i = 0; i < DFM_CFG_MAX_STOPWATCHES; i++)
	{
		if (stopwatches[i].id != 0)
		{
			prvStopwatchPrint(&stopwatches[i], "");
		}else{
			printf("Index %d not used.\n", i);
		}
	}
}


void vDfmStopwatchClearAll(void)
{
	for (int i = 0; i < DFM_CFG_MAX_STOPWATCHES; i++)
	{
		stopwatches[i].expected_duration = 0;
		stopwatches[i].name = NULL;
		stopwatches[i].high_watermark = 0;
		stopwatches[i].start_time = 0;
		stopwatches[i].id = 0;
	}
	stopwatch_count = 0;
}



void prvDfmStopwatchAlert(char* msg, int high_watermark, int stopwatch_index)
{
	static DfmAlertHandle_t xAlertHandle;
	if (xDfmAlertBegin(DFM_TYPE_OVERLOAD, msg, &xAlertHandle) == DFM_SUCCESS)
	{
		void* pvBuffer = (void*)0;
		uint32_t ulBufferSize = 0;
		static TraceStringHandle_t TzUserEventChannel = NULL;

		if (TzUserEventChannel == 0)
		{
			xTraceStringRegister("ALERT", &TzUserEventChannel);
		}

		xTracePrint(TzUserEventChannel, cDfmPrintBuffer);

		/* Pausing the tracing while outputting the data. Note that xDfmAlertAddPayload doesn't copy the trace data, only the pointer.
		 * So we should not allow new events to be written to the trace buffer before xDfmAlertEnd (where it is stored) */
		xTracePause();

		xTraceGetEventBuffer(&pvBuffer, &ulBufferSize);
		xDfmAlertAddPayload(xAlertHandle, pvBuffer, ulBufferSize, "dfm_trace.psfs");

		xDfmAlertAddSymptom(xAlertHandle, DFM_SYMPTOM_HIGH_WATERMARK, high_watermark);
		xDfmAlertAddSymptom(xAlertHandle, DFM_SYMPTOM_STOPWATCH_ID, stopwatch_index);

		/* Assumes "cloud port" is a UART or similar, that is always available. */
		if (xDfmAlertEnd(xAlertHandle) != DFM_SUCCESS)
		{
			DFM_DEBUG_PRINT("DFM: xDfmAlertEnd failed.\n");
		}

		xTraceResume();

	}
}

#ifdef INCLUDE_STOPWATCH

void main_superloop(void)
{
    int counter = 0;
    dfmStopwatch_t* my_stopwatch = xDfmStopwatchCreate("MyStopwatch1", 10);

    if (my_stopwatch == NULL)
    {
    	printf("ERROR, my_stopwatch == NULL\n");
    	return;
    }

    while (1)
    {
    	vDfmStopwatchBegin(my_stopwatch);
    	DemoUpdate(counter);
    	vDfmStopwatchEnd(my_stopwatch);

    	counter++;

        xTraceTaskSwitch( (void*)TASK_IDLE, 0);
    	HAL_Delay(1);
    	xTraceTaskSwitch( (void*)TASK_MAIN, 0);

    }
}

#elif defined(INCLUDE_STOPWATCH_UNIT_TEST)

void prvStopwatchSetSimulatedTime(uint32_t time);
void prvStopwatchClear(dfmStopwatch_t* sw);


DfmResult_t prvPrintAndCheckStopwatch(dfmStopwatch_t* sw, uint32_t expected_hwm, uint32_t expected_st, uint32_t expected_exp_dur, const char* expected_name, uint32_t expected_id)
{
	DfmResult_t result = DFM_SUCCESS;
	char* status = "(OK)";

	if (sw->start_time != expected_st)
	{
		status = "ERROR (start_time)";
		result = DFM_FAIL;
	}

	if (sw->expected_duration != expected_exp_dur)
	{
		status = "ERROR (expected_duration)";
		result = DFM_FAIL;
	}

	if (sw->high_watermark != expected_hwm)
	{
		status = "ERROR (high_watermark)";
		result = DFM_FAIL;
	}

	if (sw->name != expected_name)
	{
		status = "ERROR (name)";
		result = DFM_FAIL;
	}

	if (sw->id != expected_id)
	{
		status = "ERROR (id)";
		result = DFM_FAIL;
	}

	prvStopwatchPrint(sw, status);

	return result;
}

void main_superloop(void)
{
	int err = 0;

	char swnames[DFM_CFG_MAX_STOPWATCHES][10] = {"SW1", "SW2", "SW2", "SW4"} ;

	printf("\nTest 1: xDfmStopwatchCreate().\n");
    for (int i = 0; i < DFM_CFG_MAX_STOPWATCHES + 1; i++) // Tries creating one too many stopwatches.
    {
    	dfmStopwatch_t* sw = NULL;

    	sw = xDfmStopwatchCreate( swnames[i], 1000 + (i * 100));

    	printf(" Call %d: ", i);

    	if (sw == NULL)
    	{
    		printf("NULL ");
    		if (i >= DFM_CFG_MAX_STOPWATCHES)
    		{
    			printf("(OK)\n");
    		}
    		else
    		{
    			printf("ERROR!\n");
    			err = 1;
    		}
    	}
    	else
    	{
    		printf("Not NULL (OK)\n");
    	}
    }

    prvDfmPrintHeader();
    for (int i = 0; i < DFM_CFG_MAX_STOPWATCHES; i++) // List and verify all stopwatches
    {
    	if (prvPrintAndCheckStopwatch(&stopwatches[i], 0, 0, 1000 + (100*i), swnames[i], i+1) == DFM_FAIL)
    	{
    		err = 1;
    	}
    }

    if (err == 0)
    {
    	dfmStopwatch_t* sw;
    	vDfmStopwatchClearAll();

    	// Test: Zero time elapsed
    	printf("\nTest 2: vDfmStopwatchBegin/End - Zero time elapsed.\n");
    	prvDfmPrintHeader();
    	sw = xDfmStopwatchCreate(swnames[0], 1000);
    	prvStopwatchSetSimulatedTime(0);
    	vDfmStopwatchBegin(sw);
    	vDfmStopwatchEnd(sw);
    	prvPrintAndCheckStopwatch(sw, 0, 0, 1000, swnames[0], 1);

    	printf("\nTest 3: vDfmStopwatchClearAll();\n");
    	vDfmStopwatchClearAll();
    	prvDfmPrintHeader();
    	for (int i = 0; i < DFM_CFG_MAX_STOPWATCHES; i++)
    	{
    		prvPrintAndCheckStopwatch(&stopwatches[i], 0, 0, 0, NULL, 0);
    	}

    	printf("\nTest 4: Begin/End - Overflow at t=0xFFFFFFFF.\n");
    	prvDfmPrintHeader();
    	sw = xDfmStopwatchCreate(swnames[0], 2);
    	prvStopwatchSetSimulatedTime(0xFFFFFFFF);
		vDfmStopwatchBegin(sw);
		prvStopwatchSetSimulatedTime(1);
		vDfmStopwatchEnd(sw);
		prvPrintAndCheckStopwatch(sw, 2, 0xFFFFFFFF, 2, swnames[0], 1);
		vDfmStopwatchClearAll();

    	printf("\nTest 5: Begin/End from 200 to 300.\n");
    	prvDfmPrintHeader();
    	sw = xDfmStopwatchCreate(swnames[0], 1000);
    	prvStopwatchSetSimulatedTime(200);
    	vDfmStopwatchBegin(sw);
    	prvStopwatchSetSimulatedTime(300);
    	vDfmStopwatchEnd(sw);
    	prvPrintAndCheckStopwatch(sw, 100, 200, 1000, swnames[0], 1);
    	vDfmStopwatchClearAll();

		printf("\nTest 6: Begin/End with 0xFFFFFFFF duration\n");
		prvDfmPrintHeader();
		sw = xDfmStopwatchCreate(swnames[0], 0xFFFFFFFF);
		prvStopwatchSetSimulatedTime(0);
		vDfmStopwatchBegin(sw);
		prvStopwatchSetSimulatedTime(0xFFFFFFFF);
		vDfmStopwatchEnd(sw);
		prvPrintAndCheckStopwatch(sw, 0xFFFFFFFF, 0, 0xFFFFFFFF, swnames[0], 1);
		vDfmStopwatchClearAll();

		printf("\nTest 7: Using all stopwatches at the same time.\n");

		{
			dfmStopwatch_t* sw0;
			dfmStopwatch_t* sw1;
			dfmStopwatch_t* sw2;
			dfmStopwatch_t* sw3;

			sw0 = xDfmStopwatchCreate(swnames[0], 100);
			sw1 = xDfmStopwatchCreate(swnames[1], 200);
			sw2 = xDfmStopwatchCreate(swnames[2], 300);
			sw3 = xDfmStopwatchCreate(swnames[3], 400);

			prvStopwatchSetSimulatedTime(10);

			vDfmStopwatchBegin(sw0);
			vDfmStopwatchBegin(sw1);
			vDfmStopwatchBegin(sw2);
			vDfmStopwatchBegin(sw3);

			prvStopwatchSetSimulatedTime(110);
			vDfmStopwatchEnd(sw0);

			prvStopwatchSetSimulatedTime(210);
			vDfmStopwatchEnd(sw1);

			prvStopwatchSetSimulatedTime(310);
			vDfmStopwatchEnd(sw2);

			prvStopwatchSetSimulatedTime(410);
			vDfmStopwatchEnd(sw3);

			prvDfmPrintHeader();
			prvPrintAndCheckStopwatch(sw0, 100, 10, 100, swnames[0], 1);
			prvPrintAndCheckStopwatch(sw1, 200, 10, 200, swnames[1], 2);
			prvPrintAndCheckStopwatch(sw2, 300, 10, 300, swnames[2], 3);
			prvPrintAndCheckStopwatch(sw3, 400, 10, 400, swnames[3], 4);

			vDfmStopwatchClearAll();
		}
    }

    for (;;);
}

// Only intended for unit testing
void prvStopwatchSetSimulatedTime(uint32_t time)
{
	hwtc_count_simulated = time;
}




#elif defined(STOPWATCH_OVERHEAD_MEASUREMENT)

/******************************************************************************
 * Profiling results on 80 MHz Arm Cortex-M4 (STM32L475)
 *
 * Clock cycles needed for one measurement (xDfmStopwatchBegin() +
 * xDfmStopwatchEnd()) in the normal case (no alert or new high watermark).
 * - No Optimizations (-O0): 108 cycles
 * - Basic Optimizations (-O1): 65 cycles
 * - Full Optimizations (-O3): 31 cycles
 *
 * This means, with 1000 measurements per second on this 80 MHz device, the
 * execution time overhead would be 0,04%-0.14%.
 *****************************************************************************/

void main_superloop(void)
{
    int counter = 0;
    dfmStopwatch_t* my_stopwatch;

    uint32_t starttime1;
    uint32_t endtime1;
    uint32_t max1 = 0;
    uint32_t starttime2;
    uint32_t endtime2;
    uint32_t max2 = 0;
    uint32_t dur;

    my_stopwatch = xDfmStopwatchCreate("MyStopwatch1", 10);
    if (my_stopwatch == NULL)
    {
    	printf("ERROR, my_stopwatch == NULL\n");
    	return;
    }

    while (1)
    {


    	starttime1 = TRC_HWTC_COUNT;
    	vDfmStopwatchBegin(my_stopwatch);
    	endtime1 = TRC_HWTC_COUNT;

    	DemoUpdate(counter);

    	starttime2 = TRC_HWTC_COUNT;
    	vDfmStopwatchEnd(my_stopwatch);
    	endtime2 = TRC_HWTC_COUNT;

    	dur = endtime1 - starttime1;
    	printf("Begin: %u", (unsigned int)dur);

    	if (dur > max1)
    	{
    		printf(" - new high watermark.\n");
    		max1 = dur;
    	}
    	else
    	{
    		printf("\n");
    	}

    	dur = endtime2 - starttime2;
    	printf("End: %u", (unsigned int)dur);
    	if (dur > max2)
    	{
    		printf(" - new high watermark.\n");
    		max2 = dur;
    	}
    	else
 	    {
    		printf("\n");
    	}

    	counter++;

        xTraceTaskSwitch( (void*)TASK_IDLE, 0);
    	HAL_Delay(1);
    	xTraceTaskSwitch( (void*)TASK_MAIN, 0);

    }
}


#else

// No stopwatch
void main_superloop(void)
{
    int counter = 0;

    while (1)
    {
    	DemoUpdate(counter);

    	counter++;

        xTraceTaskSwitch( (void*)TASK_IDLE, 0);
    	HAL_Delay(1);
    	xTraceTaskSwitch( (void*)TASK_MAIN, 0);

    }
}

#endif

static void DemoUpdate(int counter)
{
	DemoISRUpdate(counter);
   	DemoStatesUpdate(counter);
   	DemoUserEventsUpdate(counter);

   	if (ButtonPressed == 1)
   	{
   		ButtonPressed = 0;
   	    DemoAlert();
   	}
}

static void DemoInit(void)
{
	DemoAlertInit();
    DemoISRInit();
    DemoStatesInit();
    DemoUserEventsInit();
}


int main_baremetal( void )
{
    /* Just to set a better name for the main thread...*/
    REGISTER_TASK(TASK_MAIN, "main-thread");

    /* Used for tracing the HAL_Delay call in the bare-metal superloop. */
    REGISTER_TASK(TASK_IDLE, "IDLE");

    /* Trace TASK_MAIN as the executing task... */
    xTraceTaskSwitch((void*)TASK_MAIN, 0);

    DemoInit();

    __set_BASEPRI(0);
    __enable_irq();

    main_superloop();

    return 0;
}

void DemoOnButtonPressedBareMetal(void)
{
	ButtonPressed = 1;
}


