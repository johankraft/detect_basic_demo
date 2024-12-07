
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

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

static void prvInitializeHeap( void );
static void DemoAlertTask(void* argument);
static void DemoTaskFreeRTOS(void* argument);

TaskHandle_t DemoAlertTaskHandle = NULL;

// Called on every "tick"
void DemoUpdate(int counter)
{
	DemoISRUpdate(counter);
   	DemoStatesUpdate(counter);
   	DemoUserEventsUpdate(counter);
}

// Called once during startup
void DemoInit(void)
{
	DemoAlertInit();
    DemoISRInit();
    DemoStatesInit();
    DemoUserEventsInit();

	if( xTaskCreate( DemoTaskFreeRTOS, "DemoTask", 128, NULL, 2, NULL ) != pdPASS )
	{
	   configPRINT_STRING(("Failed creating DemoTask."));
	}

	if (xTaskCreate(DemoAlertTask,  "DemoAlertTask", 1024, NULL, tskIDLE_PRIORITY, &DemoAlertTaskHandle ) != pdPASS)
	{
		configPRINT_STRING(("Failed creating ButtonTask."));
	}
}


/**
 * @brief Application runtime entry point.
 */
int main_freertos( void )
{
	prvInitializeHeap();

    DemoInit();

    // Starts FreeRTOS
    vTaskStartScheduler();

    return 0;
}

void DemoAlertTask(void* argument)
{
	for(;;)
    {
    	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  /* Block indefinitely. */
    	DemoAlert();
    }
}

void DemoTaskFreeRTOS(void* argument)
{
    for(;;)
    {
    	DemoUpdate( xTaskGetTickCount() );
    	vTaskDelay(1);
    }
}

/* The FreeRTOS Heap */
uint8_t ucHeap1[ configTOTAL_HEAP_SIZE ];

static void prvInitializeHeap( void )
{
    HeapRegion_t xHeapRegions[] =
    {
        { ( unsigned char * ) ucHeap1, sizeof( ucHeap1 ) },
        { NULL,                        0                 }
    };

    vPortDefineHeapRegions( xHeapRegions );
}

void vApplicationMallocFailedHook(void)
{
	/* Create an Alert and restart here... */
	DFM_TRAP(DFM_TYPE_MALLOC_FAILED, "Could not allocate heap memory", 1);
}

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                     char * pcTaskName )
{
	configPRINT_STRING( ( "ERROR: stack overflow\r\n" ) );

	/* Create an Alert and restart here... */
	// DFM_TRAP(DFM_TYPE_STACK_OVERFLOW, "Stack overflow");

	/* Unused Parameters */
	( void ) xTask;
	( void ) pcTaskName;

	/* Loop forever - but not reached*/
	for( ; ; )
	{
	}
}

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
 * function then they must be declared static - otherwise they will be allocated on
 * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetTimerTaskMemory() to provide the memory that is
 * used by the RTOS daemon/time task. */
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
 * function then they must be declared static - otherwise they will be allocated on
 * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void DemoOnButtonPressedFreeRTOS(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR( DemoAlertTaskHandle, &xHigherPriorityTaskWoken );
}

