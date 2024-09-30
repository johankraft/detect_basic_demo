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
	switch(TRC_HWTC_COUNT % 3)
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
    configPRINTF(("\nFirmware Revision: %s\n", DFM_CFG_FIRMWARE_VERSION));

	configPRINT_STRING( "\n\nAlert Demo - Press blue button to trigger an error that is captured by DevAlert.\n\n" );
}
