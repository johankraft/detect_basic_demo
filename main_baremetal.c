
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
 *
 * TODO:
 * - Test loading alerts into Detect server.
 * - Move to a separate dfm file (clean up).
 * - Unit tests for Stopwatch API (also verify overflows).
 * - Improve the demo application.
 * - Allow for using DFM_TRAP without restarting.
 */

extern char cDfmPrintBuffer[128];

typedef struct {
	uint32_t start_time;
	uint32_t expected_duration;
	uint32_t high_watermark;
	char* name;
} dfmStopwatch_t;

#define MAX_STOPWATCHES 4

dfmStopwatch_t stopwatches[MAX_STOPWATCHES];
int32_t stopwatch_count = 0;

dfmStopwatch_t* xDfmStopwatchCreate(char* name, uint32_t expected_max);
void vDfmStopwatchBegin(dfmStopwatch_t* sw);
void vDfmStopwatchEnd(dfmStopwatch_t* sw);

dfmStopwatch_t* xDfmStopwatchCreate(char* name, uint32_t expected_max)
{
	TRACE_ALLOC_CRITICAL_SECTION();
	dfmStopwatch_t* sw = NULL;

	TRACE_ENTER_CRITICAL_SECTION();

	if (stopwatch_count < MAX_STOPWATCHES)
	{
		sw = &stopwatches[stopwatch_count];

		sw->expected_duration = expected_max;
		sw->name = name;
		sw->high_watermark = 0;
		sw->start_time = 0;

		stopwatch_count++;
	}
	TRACE_EXIT_CRITICAL_SECTION();

	return sw;
}

void vDfmStopwatchBegin(dfmStopwatch_t* sw)
{
	if (sw != NULL)
	{
		sw->start_time = TRC_HWTC_COUNT;
	}
}

extern void prvAddTracePayload(void);

void vDfmStopwatchEnd(dfmStopwatch_t* sw)
{
	uint32_t end_time = TRC_HWTC_COUNT;

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
				static DfmAlertHandle_t xAlertHandle;

				snprintf(cDfmPrintBuffer, sizeof(cDfmPrintBuffer), "Stopwatch %s, new high: %u", sw->name, (unsigned int)sw->high_watermark);
				DFM_DEBUG_PRINT(cDfmPrintBuffer);

				if (xDfmAlertBegin(DFM_TYPE_OVERLOAD, cDfmPrintBuffer, &xAlertHandle) == DFM_SUCCESS)
				{
					void* pvBuffer = (void*)0;
					uint32_t ulBufferSize = 0;

					// xDfmAlertAddSymptom(xAlertHandle,

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

					/* Assumes "cloud port" is a UART or similar, that is always available. */
					if (xDfmAlertEnd(xAlertHandle) != DFM_SUCCESS)
					{
						DFM_DEBUG_PRINT("DFM: xDfmAlertEnd failed.\n");
					}
					xTraceResume();
				}
			}
		}
	}
}


//#define INCLUDE_STOPWATCH
//#define STOPWATCH_OVERHEAD_MEASUREMENT

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


