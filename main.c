/*
 * FreeRTOS V1.4.7
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

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

static void SystemClock_Config( void );
static void Console_UART_Init( void );
static void prvMiscInitialization( void );

int main( void );

static void DemoUpdate(int tickcounter);
static void DemoInit(void);

IotUARTHandle_t xConsoleUart;


/******************************************************************************
 * DEMO_TYPE
 *
 * Sets if the demo should run on FreeRTOS or using a "bare-metal" superloop.
 *
 * Valid options:
 * - DEMO_BAREMETAL: Superloop only, FreeRTOS not used.
 * - DEMO_FREERTOS: Using FreeRTOS to run the application.
 *
 *****************************************************************************/
#define DEMO_TYPE DEMO_BAREMETAL


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


#if (DEMO_TYPE == DEMO_BAREMETAL)

volatile int ButtonPressed = 0;

void main_superloop(void)
{
    int counter = 0;

    // Set BasePri 0
    // Enable all interrupts
    __set_BASEPRI(0);
    __enable_irq();

    while (1)
    {
    	DemoUpdate(counter);
    	counter++;

        xTraceTaskSwitch( (void*)TASK_IDLE, 0);
    	HAL_Delay(1);
    	xTraceTaskSwitch( (void*)TASK_MAIN, 0);

    }
}

#define RUN_DEMO() main_superloop();


// ISR for Button press, wakes up the ButtonTask using a global variable.
void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{
    if (GPIO_Pin == USER_BUTTON_PIN )
    {
    	ButtonPressed = 1;
    }
}


#elif (DEMO_TYPE == DEMO_FREERTOS)

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

static void DemoAlertTask(void* argument);
static void prvInitializeHeap( void );

#define RUN_DEMO() vTaskStartScheduler();

void DemoTaskFreeRTOS(void* argument);

TaskHandle_t DemoAlertTaskHandle = NULL;

// ISR for Button press, wakes up the ButtonTask using FreeRTOS task notify call.
void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (GPIO_Pin == USER_BUTTON_PIN )
    {
    	vTaskNotifyGiveFromISR( DemoAlertTaskHandle, &xHigherPriorityTaskWoken );
    }
}

void DemoAlertTask(void* argument)
{
	for(;;)
    {
    	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  /* Block indefinitely. */
    	DemoAlert();
    }
}

#endif


/*-----------------------------------------------------------*/

// Called on every "tick" from main_superloop or FreeRTOS tick hook, depending on DEMO_TYPE.
void DemoUpdate(int counter)
{
	DemoISRUpdate(counter);
   	DemoStatesUpdate(counter);
   	DemoUserEventsUpdate(counter);

#if (DEMO_TYPE == DEMO_BAREMETAL)
   	if (ButtonPressed == 1)
   	{
   		ButtonPressed = 0;
   	    DemoAlert();
   	}
#endif

}


// Called once during startup, from main.
void DemoInit(void)
{
	DemoAlertInit();
    DemoISRInit();
    DemoStatesInit();
    DemoUserEventsInit();

#if (DEMO_TYPE == DEMO_FREERTOS)

	if( xTaskCreate( DemoTaskFreeRTOS, "DemoTask", 128, NULL, 2, NULL ) != pdPASS )
	{
	   configPRINT_STRING(("Failed creating DemoTask."));
	}

	if (xTaskCreate(DemoAlertTask,  "DemoAlertTask", 1024, NULL, tskIDLE_PRIORITY, &DemoAlertTaskHandle ) != pdPASS)
	{
		configPRINT_STRING(("Failed creating ButtonTask."));
	}

#endif
}

/**
 * @brief Application runtime entry point.
 */
int main( void )
{
	/* Perform any hardware initialization that does not require the RTOS to be
     * running.  */
    prvMiscInitialization();

    /* Initialize Percepio TraceRecorder (stores events to ring buffer) */
    xTraceEnable(TRC_START);

    /* Just to set a better name for the main thread...*/
    REGISTER_TASK(TASK_MAIN, "main-thread");

    /* Used for tracing the HAL_Delay call in the bare-metal superloop. */
    REGISTER_TASK(TASK_IDLE, "IDLE");

    /* Trace TASK_MAIN as the executing task... */
    xTraceTaskSwitch((void*)TASK_MAIN, 0);

    /* Initialize the Percepio DFM library for creating Alerts. */
    if (xDfmInitializeForLocalUse() == DFM_FAIL)
    {
    	configPRINTF(("Failed to initialize DFM\r\n"));
    }

    DemoInit();

    // Calls main_superloop or starts FreeRTOS, depending on DEMO_TYPE.
    RUN_DEMO();

    return 0;
}
/*-----------------------------------------------------------*/

void vSTM32L475putc( void * pv,
                     char ch )
{
    int32_t status;

    do {
        status = iot_uart_write_sync( xConsoleUart, ( uint8_t * ) &ch, 1 );
    } while( status == IOT_UART_BUSY );
}

void vSTM32L475getc( void * pv,
                     char * ch )
{
    int32_t status;
	int done = 0;

    do {
        status = iot_uart_read_sync( xConsoleUart, ( uint8_t * ) ch, 1 );
        switch (status) {
        case IOT_UART_SUCCESS:
        	done = 1;
        	break;
        case IOT_UART_INVALID_VALUE:
        	configPRINTF(("vSTM32L475getc(): IOT_UART_INVALID_VALUE"));
        	done = 1;
        	break;
        case IOT_UART_READ_FAILED:
        	configPRINTF(("vSTM32L475getc(): IOT_UART_READ_FAILED"));
        	done = 1;
        	break;
        case IOT_UART_BUSY:
        	/* keep looping */
        	break;
        case IOT_UART_FUNCTION_NOT_SUPPORTED:
        	configPRINTF(("vSTM32L475getc(): IOT_UART_FUNCTION_NOT_SUPPORTED"));
        	done = 1;
        	break;
        case IOT_UART_NOTHING_TO_CANCEL:
        case IOT_UART_WRITE_FAILED:
        default:
        	configPRINTF(("vSTM32L475getc(): unexpected status"));
        	done = 1;
        	break;
        }
    } while (!done);
}

static void prvMiscInitialization( void )
{
    HAL_Init();

    SystemClock_Config();

 #if (DEMO_TYPE == DEMO_FREERTOS)
    prvInitializeHeap();
 #endif

    // Enable fault on divide-by-zero and unaligned access
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk | SCB_CCR_UNALIGN_TRP_Msk;

    // enable UsageFault, BusFault and MPU Fault
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk |  SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk;

    BSP_LED_Init( LED_GREEN );
    BSP_PB_Init( BUTTON_USER, BUTTON_MODE_EXTI );

    Console_UART_Init();
}


static void Console_UART_Init( void )
{
    int32_t status = IOT_UART_SUCCESS;

     xConsoleUart = iot_uart_open( 0 );
     configASSERT( xConsoleUart != NULL );

     IotUARTConfig_t xConfig =
     {
         .ulBaudrate    = 115200,
         .xParity      = UART_PARITY_NONE,
         .ucWordlength  = UART_WORDLENGTH_8B,
         .xStopbits    = UART_STOPBITS_1,
         .ucFlowControl = UART_HWCONTROL_NONE
     };

     status = iot_uart_ioctl( xConsoleUart, eUartSetConfig, &xConfig );
     configASSERT( status == IOT_UART_SUCCESS );
}

void Error_Handler( void )
{
    while( 1 )
    {
        BSP_LED_Toggle( LED_GREEN );
        HAL_Delay( 200 );
    }
}

void vMainUARTPrintString( char * pcString )
{
    iot_uart_write_sync( xConsoleUart, ( uint8_t * ) pcString, strlen( pcString ) );
}

#if (DEMO_TYPE == DEMO_FREERTOS)

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
	DFM_TRAP(DFM_TYPE_MALLOC_FAILED, "Could not allocate heap memory");
}

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                     char * pcTaskName )
{
	configPRINT_STRING( ( "ERROR: stack overflow\r\n" ) );

	/* Create an Alert and restart here... */
	DFM_TRAP(DFM_TYPE_STACK_OVERFLOW, "Stack overflow");

	/* Unused Parameters */
	( void ) xTask;
	( void ) pcTaskName;

	/* Loop forever - but not reached*/
	for( ; ; )
	{
	}
}

#endif /* #if (DEMO_TYPE == DEMO_FREERTOS) */

void HAL_TIM_PeriodElapsedCallback( TIM_HandleTypeDef * htim )
{
    if( htim->Instance == TIM6 )
    {
        HAL_IncTick();
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



static void SystemClock_Config( void )
{
    RCC_OscInitTypeDef xRCC_OscInitStruct;
    RCC_ClkInitTypeDef xRCC_ClkInitStruct;
    RCC_PeriphCLKInitTypeDef xPeriphClkInit;

    xRCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
    xRCC_OscInitStruct.LSEState = RCC_LSE_ON;
    xRCC_OscInitStruct.MSIState = RCC_MSI_ON;
    xRCC_OscInitStruct.MSICalibrationValue = 0;
    xRCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
    xRCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    xRCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    xRCC_OscInitStruct.PLL.PLLM = 6;
    xRCC_OscInitStruct.PLL.PLLN = 20;
    xRCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    xRCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    xRCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if( HAL_RCC_OscConfig( &xRCC_OscInitStruct ) != HAL_OK )
    {
        Error_Handler();
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     * clocks dividers. */
    xRCC_ClkInitStruct.ClockType = ( RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 );
    xRCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    xRCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    xRCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    xRCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if( HAL_RCC_ClockConfig( &xRCC_ClkInitStruct, FLASH_LATENCY_4 ) != HAL_OK )
    {
        Error_Handler();
    }

    xPeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC
                                          | RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART3 | RCC_PERIPHCLK_I2C2
                                          | RCC_PERIPHCLK_RNG;
    xPeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    xPeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    xPeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
    xPeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    xPeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_MSI;

    if( HAL_RCCEx_PeriphCLKConfig( &xPeriphClkInit ) != HAL_OK )
    {
        Error_Handler();
    }

    __HAL_RCC_PWR_CLK_ENABLE();

    if( HAL_PWREx_ControlVoltageScaling( PWR_REGULATOR_VOLTAGE_SCALE1 ) != HAL_OK )
    {
        Error_Handler();
    }

    /* Enable MSI PLL mode. */
    HAL_RCCEx_EnableMSIPLLMode();
}

void DemoSimulateExecutionTime(int n)
{
	for (volatile int dummy = 0; dummy < n; dummy++);
}


#if (DEMO_TYPE == DEMO_FREERTOS)
void DemoTaskFreeRTOS(void* argument)
{
    for(;;)
    {
    	DemoUpdate( xTaskGetTickCount() );
    	vTaskDelay(1);
    }
}
#endif

void vApplicationTickHook(void)
{
}

