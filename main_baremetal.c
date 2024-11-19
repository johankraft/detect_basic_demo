
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
 *
 * TODO:
 * - Load alerts into Detect server.
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

DfmResult_t xDfmStopwatchCreate(char* name, int expected_max, int* stopwatch_id);
DfmResult_t xDfmStopwatchBegin(int stopwatch_id);
DfmResult_t xDfmStopwatchEnd(int stopwatch_id);

DfmResult_t xDfmStopwatchCreate(char* name, int expected_max, int* stopwatch_id)
{
	TRACE_ALLOC_CRITICAL_SECTION();
	DfmResult_t result = DFM_FAIL;
	*stopwatch_id = -1;

	TRACE_ENTER_CRITICAL_SECTION();

	if (stopwatch_count < MAX_STOPWATCHES)
	{
		*stopwatch_id = stopwatch_count;

		stopwatches[stopwatch_count].expected_duration = expected_max;
		stopwatches[stopwatch_count].name = name;
		stopwatches[stopwatch_count].high_watermark = 0;
		stopwatches[stopwatch_count].start_time = 0;

		stopwatch_count++;

		result = DFM_SUCCESS;
	}
	TRACE_EXIT_CRITICAL_SECTION();

	return result;
}

DfmResult_t xDfmStopwatchBegin(int stopwatch_id)
{
	if ((stopwatch_id >= 0) && (stopwatch_id < MAX_STOPWATCHES))
	{
		stopwatches[stopwatch_id].start_time = TRC_HWTC_COUNT;
		return DFM_SUCCESS;
	}
	else
	{
		return DFM_FAIL;
	}
}

extern void prvAddTracePayload(void);

DfmResult_t xDfmStopwatchEnd(int stopwatch_id)
{
	static DfmAlertHandle_t xAlertHandle;
	DfmResult_t result = DFM_FAIL;

	if ((stopwatch_id >= 0) && (stopwatch_id < MAX_STOPWATCHES))
	{
		uint32_t end_time = TRC_HWTC_COUNT;

		/* Overflow in HWTC_COUNT is OK. Say that start_time is 0xFFFFFFFF
		 * and end_time is 1, then we get (1 - 0xFFFFFFFF) = 2.
		 * This since 0xFFFFFFFF equals -1 (signed) and 1 - (-1)) = 2 */
		uint32_t duration = end_time - stopwatches[stopwatch_id].start_time;

		/* Alert if new highest value is found, assuming it is above expected_duration (provides a lower threshold) */
		if ( (duration > stopwatches[stopwatch_id].expected_duration) && (duration > stopwatches[stopwatch_id].high_watermark) )
		{
			stopwatches[stopwatch_id].high_watermark = duration;

			snprintf(cDfmPrintBuffer, sizeof(cDfmPrintBuffer), "Stopwatch %s, new high: %u", stopwatches[stopwatch_id].name, (unsigned int)stopwatches[stopwatch_id].high_watermark);
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
				if (xDfmAlertEnd(xAlertHandle) == DFM_SUCCESS)
				{
					DFM_DEBUG_PRINT("DFM: xDfmAlertEnd OK.\n");
					result = DFM_SUCCESS;
				}
				else
				{
					DFM_DEBUG_PRINT("DFM: xDfmAlertEnd failed.\n");
					result = DFM_FAIL;
				}
			}
		}
	}

	xTraceResume();

	return result;
}

//#define TEST_STOPWATCH

#ifdef TEST_STOPWATCH

void main_superloop(void)
{
    int counter = 0;

    while (1)
    {
    	int my_stopwatch;
    	xDfmStopwatchCreate("MyStopwatch1", 10000, &my_stopwatch);

    	xDfmStopwatchBegin(my_stopwatch);
    	DemoUpdate(counter);
    	xDfmStopwatchEnd(my_stopwatch);

    	counter++;

        xTraceTaskSwitch( (void*)TASK_IDLE, 0);
    	HAL_Delay(1);
    	xTraceTaskSwitch( (void*)TASK_MAIN, 0);

    }
}

#else
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


