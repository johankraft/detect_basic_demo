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

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "iot_uart.h"

#include "trcRecorder.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"

#define mainLOGGING_TASK_PRIORITY                         ( configMAX_PRIORITIES - 1 )
#define mainLOGGING_TASK_STACK_SIZE                       ( 2000 )
#define mainLOGGING_MESSAGE_QUEUE_LENGTH                  ( 15 )

/* Minimum required WiFi firmware version. */
#define mainREQUIRED_WIFI_FIRMWARE_WICED_MAJOR_VERSION    ( 3 )
#define mainREQUIRED_WIFI_FIRMWARE_WICED_MINOR_VERSION    ( 5 )
#define mainREQUIRED_WIFI_FIRMWARE_WICED_PATCH_VERSION    ( 2 )
#define mainREQUIRED_WIFI_FIRMWARE_INVENTEK_VERSION       ( 5 )
#define mainREQUIRED_WIFI_FIRMWARE_DESCRIPTOR_STRING      ( "STM" )

/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook( void );

/**********************
* Global Variables
**********************/
IotUARTHandle_t xConsoleUart;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config( void );
static void Console_UART_Init( void );

/**
 * @brief Initializes the STM32L475 IoT node board.
 *
 * Initialization of clock, LEDs, RNG, RTC, and WIFI module.
 */
static void prvMiscInitialization( void );

/**
 * @brief Initializes the FreeRTOS heap.
 *
 * Heap_5 is being used because the RAM is not contiguous, therefore the heap
 * needs to be initialized.  See http://www.freertos.org/a00111.html
 */
static void prvInitializeHeap( void );

/*-----------------------------------------------------------*/

void ButtonTask(void* argument);

char* stackoverflowpadding = NULL;

typedef struct task_arg {
    uint32_t type;
    const char* message;
} task_arg_t;


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


TaskHandle_t xBhjHandle = NULL;

char* ptr = NULL;

void ButtonTask(void* argument)
{
	configPRINT_STRING( "\n\nDemo ready - Press blue button to trigger an error that is captured by DevAlert.\n\n" );
    for(;;)
    {
    	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  /* Block indefinitely. */

    	/* Pick a random test case... TRC_HWTC_COUNT gives the current clock cycle count. */

    	switch(TRC_HWTC_COUNT % 4)
        {
        		case 0:

        			configPRINTF(( "Test case: Assert failed\n"));

        			vTaskDelay(1000);

        			testAssertFailed(ptr);

        			break;

        		case 1:

        			configPRINTF(( "Test case: Malloc failed\n"));

        			vTaskDelay(1000);

        			pvPortMalloc(1000000); // This much heap memory isn't available, should fail

        			break;

        		case 2:

        			configPRINTF(( "Test case: Hardware fault exception\n"));

        			vTaskDelay(1000);

        			dosomething(3);

        			break;

        		case 3:

        			configPRINTF(( "Test case: Buffer overrun\n"));

        			vTaskDelay(1000);

        			testBufferOverrun();
        			break;

        }
    }
}


/*** Adding a Task to show some TraceRecorder features ***********************/

#define nMSG 5
#define nSTATE 3

const char * messages[nMSG] = {	"SetMode A",
								"SetOptionsFlags X,Y",
								"Certificate checksum 123456789ABCDEF",
								"SetRemoteIP 127.0.0.1",
								"SetRemotePort 8888"};

const char * states[nSTATE] = {	"State A",
						   	    "State B",
							    "State C"};

void prvRXTask(void* argument)
{
	volatile int dummy;

	int counter = 0;
	int state = 0;
	int len = 0;

	TraceStringHandle_t command_chn;
	TraceStateMachineHandle_t myfsm;
	TraceStateMachineStateHandle_t myfsm_state[5];

	xTraceStringRegister("Command Log", &command_chn);

	/* Trace a state machine (states can span between tasks) */
	xTraceStateMachineCreate("RX State", &myfsm);

	for (int i = 0; i < nSTATE; i++)
	{
		// Register each state name, one for each message
		xTraceStateMachineStateCreate(myfsm, states[i], &myfsm_state[i]);
	}

    for(;;)
    {

    	len = strlen(messages[counter]);

    	// Logging the current "message" to the trace buffer.
    	xTracePrintF(command_chn, messages[counter]);

    	counter = (counter+1) % nMSG;
    	state = (state+1) % nSTATE;

    	// Make the task run a bit longer...
   		for (dummy = 0; dummy < 3000; dummy++);

   		// Log a state transition
    	xTraceStateMachineSetState(myfsm, myfsm_state[state]);

    	// Make the task run a bit longer...
    	for (dummy = 0; dummy < (1000 * len); dummy++);

    	vTaskDelay(50);
    }

}

/**
 * @brief Application runtime entry point.
 */
int main( void )
{
	/* Perform any hardware initialization that does not require the RTOS to be
     * running.  */
    prvMiscInitialization();

    /* Uses the timestamp source (cycle counter) to initialize pseudo-random number generation. */
    // srand(TRC_HWTC_COUNT);

    BSP_LED_Off( LED_GREEN );

    if( xTaskCreate( prvRXTask, "RX", 1000, NULL, 6, NULL ) != pdPASS )
    {
    	configPRINT_STRING(("Failed creating task."));
    }

    /* Start the scheduler.  Initialization that requires the OS to be running,
     * including the WiFi initialization, is performed in the RTOS daemon task
     * startup hook. */
    vTaskStartScheduler();

    return 0;
}
/*-----------------------------------------------------------*/

typedef struct {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
    uint8_t data[];
} ElfNoteSection_t;

extern const ElfNoteSection_t build_id_elf_note;

/******************************************************************************
 * vDfmSetGCCBuildID - Writes the GCC Build ID to dest.
 * Parameter size is the maximum length (at least 42)
 *
 * * For this to work:
 *   - Add  .gnu_build_id section in linker script (see .ld file in root folder)
 *   - Add  -Wl,--build-id in linker flags
 *
 * Can be read from elf file by "arm-none-eabi-readelf -n aws_demos.elf"
 * to archive the elf file for automated lookup using the Build ID.
 *****************************************************************************/

int vDfmSetGCCBuildID(char* dest, int size)
{
	if (size < 42) return DFM_FAIL;

	const uint8_t *build_id_data = &build_id_elf_note.data[build_id_elf_note.namesz];

    // The GNU Build ID is 20 bytes.
    for (int i = 0; i < 20; i++)
    {
    	char byte_str[4] = "";
        sprintf(byte_str, "%02x", build_id_data[i]);
        strncat(dest, byte_str, 2);
    }

    return DFM_SUCCESS;
}

void vApplicationDaemonTaskStartupHook( void )
{
    configPRINTF( ( "\n\n\n\n--- Percepio Detect Basic Demo ---\n" ) );

    // Can be read from elf file by "arm-none-eabi-readelf -n aws_demos.elf"
    configPRINTF(("Firmware Revision: %s\n", DFM_CFG_FIRMWARE_VERSION));

    /* Basic demo initialization for serial port upload */
	xTaskCreate(ButtonTask,       /* Function that implements the task. */
			   "DemoTask1",          /* Text name for the task. */
			   1024,           /* Stack size in words, not bytes. */
			   NULL,    		/* Parameter passed into the task. */
			   tskIDLE_PRIORITY,/* Priority at which the task is created. */
			   &xBhjHandle );      /* Used to pass out the created task's handle. */

	BSP_LED_On( LED_GREEN );

}

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

    prvInitializeHeap();

	// Enable usage fault and bus fault exceptions.
	SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk;

    BSP_LED_Init( LED_GREEN );
    BSP_PB_Init( BUTTON_USER, BUTTON_MODE_EXTI );

    Console_UART_Init();

    /* Initialize Percepio TraceRecorder (stores events to ring buffer) */
    xTraceEnable(TRC_START);

    /* Initialize DFM (Percepio Detect library) */
    if (xDfmInitializeForLocalUse() == DFM_FAIL)
    {
    	configPRINTF(("Failed to initialize DFM\r\n"));
    }

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

void vApplicationIdleHook( void )
{
}

void vMainUARTPrintString( char * pcString )
{
    iot_uart_write_sync( xConsoleUart, ( uint8_t * ) pcString, strlen( pcString ) );
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

/*-----------------------------------------------------------*/

void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    switch( GPIO_Pin )
    {
        case ( USER_BUTTON_PIN ):
            vTaskNotifyGiveFromISR( xBhjHandle, &xHigherPriorityTaskWoken );
            break;

        default:
            break;
    }
}

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
