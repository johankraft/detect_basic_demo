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
#include "FreeRTOS.h"
#include "task.h"
#include "iot_uart.h"

/* Percepio includes */
#include "trcRecorder.h"

void DemoUserEvents(void* argument)
{
	TraceStringHandle_t log_channel;
	TraceStringHandle_t counter_channel;
	int counter = 0;

	xTraceStringRegister("Counter", &counter_channel);
	xTraceStringRegister("Log", &log_channel);

    for(;;)
    {
    	// The simplest API function for logging. Similar to "puts".
    	xTracePrint(log_channel, "Something happened.");

    	vTaskDelay(5);

    	// The "F" variant allows for data arguments, similar to printf.
    	xTracePrintF(counter_channel, "Value: %d", counter++);

    	vTaskDelay(5);
    }
}


void DemoUserEventsInit(void)
{
	if( xTaskCreate( DemoUserEvents, "DemoUserEvents", 1000, NULL, 5, NULL ) != pdPASS )
	{
		configPRINT_STRING(("Failed creating DemoUserEvents."));
	}
}
