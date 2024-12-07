/*******************************************************************************
 * demo_user_events.c
 *
 * Demonstrates user event logging for Tracealyzer using the trcPrint API in
 * TraceRecorder.
 *
 * See full documentation in TraceRecorder/include/trcPrint.h.
 */

#include "main.h"
#include "stdint.h"
#include "stdarg.h"

/* FreeRTOS includes. */
//#include "FreeRTOS.h"
//#include "task.h"
//#include "iot_uart.h"

/* Percepio includes */
#include "trcRecorder.h"

extern void DemoSimulateExecutionTime(int n);

TraceStringHandle_t log_channel;
TraceStringHandle_t counter_channel;

void DemoUserEventsUpdate(int tickcounter)
{
	if (tickcounter % 4 == 0)
	{
		// The simplest API function for logging. Similar to "puts".
		xTracePrint(log_channel, "XYZ happened.");
	}

	// xTracePrintF allows for data arguments, similar to printf.
	xTracePrintF(counter_channel, "Value: %d", tickcounter % 16);
}

/*
void DemoUserEvents(void* argument)
{
    for(;;)
    {
    	DemoUserEventsUpdate( xTaskGetTickCount() );
    	vTaskDelay(5);
    }
}
*/

void DemoUserEventsInit(void)
{
	xTraceStringRegister("Counter", &counter_channel);
	xTraceStringRegister("Log", &log_channel);

	/*
	if( xTaskCreate( DemoUserEvents, "Demo-UserEvents", 128, NULL, 3, NULL ) != pdPASS )
	{
		configPRINT_STRING(("Failed creating Demo-UserEvents."));
	}
	*/
}
