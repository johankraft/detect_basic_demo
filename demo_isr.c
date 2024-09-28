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
#include "demo_isr.h"

TraceISRHandle_t xISRHandle1 = 0;
TraceISRHandle_t xISRHandle2 = 0;


/* Example of tracing an ISR handler. */
void ExampleISRHandler1(void)
{
	xTraceISRBegin(xISRHandle1);

	/* ISR code... */
	DemoSimulateExecutionTime(2000);

	xTraceISREnd(0);
}


/* Tracing a second ISR handler. */
void ExampleISRHandler2(void)
{
	xTraceISRBegin(xISRHandle2);

	/* ISR code... */
	DemoSimulateExecutionTime(2500);

	xTraceISREnd(0);
}


/* This is called by the FreeRTOS Tick Hook to simulate an example interrupt.*/
void vApplicationTickHook( void )
{
	static int tickCounter = 0;

	// If this part of the demo has not (yet) been enabled...
	if (xISRHandle1 == NULL)
		return;

	if (tickCounter % 10 == 0)
	{
		ExampleISRHandler1();
	}

	if (tickCounter % 6 == 0)
	{
		ExampleISRHandler2();
	}

	tickCounter++;
}

void DemoISRInit(void)
{
	// Second arguments (1 and 2) = Interrupt priority level, Only used for sorting the Tracealyzer columns.
	xTraceISRRegister("ISR1", 1, &xISRHandle1);
	xTraceISRRegister("ISR2", 2, &xISRHandle2);
}
