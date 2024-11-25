/*******************************************************************************
 * demo_isr.c
 *
 * Demonstrates interrupt tracing for Tracealyzer using the trcISR API in
 * TraceRecorder.
 *
 * Note that this adds some overhead to the ISR, typically a few microseconds
 * per event. This may cause issues with highly time-critical ISRs, where every
 * microsecond matter. The CPU usage will also increase, especially if tracing
 * very frequent ISRs.
 *
 * See full documentation in TraceRecorder/include/trcISR.h.
 */

#include "main.h"
#include "stdint.h"
#include "stdarg.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "iot_uart.h"

/* Percepio includes */
#include "trcRecorder.h"

/* Intentionally not initialized, value is kept between warm restarts. */
static volatile __attribute__((section (".noinit"))) unsigned int last_test_case;
static volatile __attribute__((section (".noinit"))) unsigned int last_test_case_control;

/*** Test case: Hard fault exception  ****************************************/

static int MakeFaultExceptionByIllegalRead(void)
{
   int r;
   volatile unsigned int* p;

   // Creating an invalid pointer)
   p = (unsigned int*)0x00100000;
   r = *p;

   return r;
}

// Just to make the displayed call stack a bit more interesting...
int dosomething(int n)
{
	if (n != -42)
	{
		return MakeFaultExceptionByIllegalRead();
	}
	return 0;
}


/*** Test case: Buffer overflow, causes  __stack_chk_fail to be called. ******/

#define MAX_MESSAGE_SIZE 12

void getNetworkMessage(char* data)
{
	sprintf(data, "Incoming data");
}

void processNetworkMessage(char* data)
{
	configPRINT_STRING("  Simulated network message: \"");
	configPRINT_STRING(data);
	configPRINT_STRING("\"\n");
}

void prvCheckForNetworkMessages(void)
{
	char message[MAX_MESSAGE_SIZE];
	getNetworkMessage(message);
	processNetworkMessage(message);
}

void testBufferOverrun(void)
{
	prvCheckForNetworkMessages();
}

/*** Test case: Failed assert statement triggers an Alert ********************/

void testAssertFailed(char* str)
{
	configASSERT( str != NULL );

	configPRINTF(("Input: %s", str));
}

/*****************************************************************************/

void DemoAlert(void)
{
	int test_case = 0;

	if ((last_test_case < 3) && (last_test_case_control == last_test_case * 16))
	{
		// After warm restart, we remember the previous case.
		test_case = (last_test_case+1)%3;
		last_test_case = test_case;
		last_test_case_control = last_test_case * 16;
	}
	else
	{
		// First button press after cold start, reset variables.
		test_case = 0;
		last_test_case = 0;
		last_test_case_control = 0;
	}

	switch(test_case % 3)
	{
		case 0:
			configPRINTF(( "Test case: Assert failed\n"));
			testAssertFailed(NULL);
			break;

		case 1:
			configPRINTF(( "Test case: Hardware fault exception\n"));
			dosomething(3);
			break;

		case 2:
			configPRINTF(( "Test case: Buffer overrun\n"));
			testBufferOverrun();
			break;
	}
}


void DemoAlertInit(void)
{
	configPRINT_STRING( "\n--- Percepio Detect Demo ---\n");
	configPRINTF(("DFM_CFG_FIRMWARE_VERSION: %s\n\n", DFM_CFG_FIRMWARE_VERSION));
	configPRINT_STRING("Press the blue button to trigger an error and output an Alert.\n");
	configPRINT_STRING("Log this output to a log file and pass it to Percepio Receiver.\n");

	if (last_test_case < 3)
	{
		if (last_test_case_control == last_test_case * 16)
		{
			configPRINTF(("Last test case: %d\n", last_test_case));
		}
	}

}
