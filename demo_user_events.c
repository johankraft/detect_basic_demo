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

extern void DemoSimulateExecutionTime(int n);

void DemoUserEvents(void* argument)
{
	TraceStringHandle_t log_channel;
	TraceStringHandle_t counter_channel;
	int counter = 0;

	xTraceStringRegister("Counter", &counter_channel);
	xTraceStringRegister("Log", &log_channel);

    for(;;)
    {

    	if (counter % 4 == 0)
    	{
        	// The simplest API function for logging. Similar to "puts".
    		xTracePrint(log_channel, "XYZ happened.");
    	}

    	DemoSimulateExecutionTime(1000);

    	// xTracePrintF allows for data arguments, similar to printf.
    	xTracePrintF(counter_channel, "Value: %d", counter);

    	counter = (counter+1) % 10;

    	DemoSimulateExecutionTime(1000);

    	vTaskDelay(5);
    }
}


void DemoUserEventsInit(void)
{
	if( xTaskCreate( DemoUserEvents, "Demo-UserEvents", 128, NULL, 3, NULL ) != pdPASS )
	{
		configPRINT_STRING(("Failed creating Demo-UserEvents."));
	}
}
