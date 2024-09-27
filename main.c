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


/*
 * Debug setup instructions:
 * 1) Open the debug configuration dialog.
 * 2) Go to the Debugger tab.
 * 3) If the 'Mode Setup' options are not visible, click the 'Show Generator' button.
 * 4) In the Mode Setup|Reset Mode drop down ensure that
 *    'Software System Reset' is selected.
 */

#include "main.h"
#include "stdint.h"
#include "stdarg.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo includes */
//#include "iot_system_init.h"
//#include "iot_logging_task.h"
//#include "iot_wifi.h"
//#include "aws_clientcredential.h"
//#include "aws_dev_mode_key_provisioning.h"
#include "iot_uart.h"

#include "trcRecorder.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"
#include "dfmStoragePort.h"

/* WiFi driver includes. */
#include "es_wifi.h"

/* The SPI driver polls at a high priority. The logging task's priority must also
 * be high to be not be starved of CPU time. */
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

// Move this to a DFM file?
int vDfmSetGCCBuildID(char* dest, int size);

void vApplicationDaemonTaskStartupHook( void );

/* Defined in es_wifi_io.c. */
extern void SPI_WIFI_ISR( void );
extern SPI_HandleTypeDef hspi;

#ifdef USE_OFFLOAD_SSL
#if (DEMO_CFG_SERIAL_UPLOAD_ONLY != 1)
    /* Defined in iot_wifi.c. */
    extern WIFIReturnCode_t WIFI_GetFirmwareVersion( uint8_t * pucBuffer );
#endif
#endif /* USE_OFFLOAD_SSL */

/**********************
* Global Variables
**********************/
RTC_HandleTypeDef xHrtc;
RNG_HandleTypeDef xHrng;

IotUARTHandle_t xConsoleUart;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config( void );
static void Console_UART_Init( void );
static void RTC_Init( void );

#if (DEMO_CFG_SERIAL_UPLOAD_ONLY != 1)
static void prvWifiConnect( void );
#endif

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

unsigned int getHardwareRand(void);

DfmResult_t myGetUniqueSessionID(char cBuffer[], uint32_t ulSize, uint32_t* pulBytesWritten);

DfmResult_t myGetDeviceName(char cBuffer[], uint32_t ulSize, uint32_t* pulBytesWritten);

#ifdef USE_OFFLOAD_SSL

/**
 * @brief Checks whether the Inventek module's firmware version needs to be
 * updated.
 *
 * Prints a message to inform the user to update the WiFi firmware.
 */
#if (DEMO_CFG_SERIAL_UPLOAD_ONLY != 1)
    static void prvCheckWiFiFirmwareVersion( void );
#endif

#endif /* USE_OFFLOAD_SSL */
/*-----------------------------------------------------------*/

void ButtonTask(void* argument);

char* stackoverflowpadding = NULL;

typedef struct task_arg {
    uint32_t type;
    const char* message;
} task_arg_t;

// Provokes a hard fault exception
static int MakeFaultExceptionByIllegalRead(void)
{
   int r;
   volatile unsigned int* p;

   p = (unsigned int*)0x00100000;  // 0x00100000-0x07FFFFFF is reserved on STM32F4
   r = *p;

   return r;
}

// This is just to make the call stack a bit longer and more interesting...
int dosomething(int n)
{
	if (n != -42)
	{
		return MakeFaultExceptionByIllegalRead();
	}
	return 0;
}

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

// This will cause a buffer overrun. See the error handler __stack_chk_fail in dfmCrashCatcher.c
void testBufferOverrun(void)
{
	prvCheckForNetworkMessages();
}

void testAssertFailed(char* str)
{
	configASSERT( str != NULL );

	configPRINTF(("Input: %s", str));
}

/**
 * Button checker
 */

TaskHandle_t xBhjHandle = NULL;

char* ptr = NULL;

void ButtonTask(void* argument)
{
	configPRINT_STRING( "\n\nDemo ready - Press blue button to trigger an error that is captured by DevAlert.\n\n" );
    for(;;)
    {
    	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  /* Block indefinitely. */

    	switch(getHardwareRand() % 4)
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
/*
        		case 4:

        			configPRINTF(( "Test case: Stack overflow\n"));

        	        vTaskDelay(1000);

        	        {
        	        	char data[4000];
        	        	memset(data, 0, 4000);
        	        }

        	        break; */

        }
    }
}


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
	int delay;
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

    	xTracePrintF(command_chn, messages[counter]);

    	// Just cycle through these...
    	counter = (counter+1) % nMSG;
    	state = (state+1) % nSTATE;

    	// Make the task run longer...
   		for (dummy = 0; dummy < 3000; dummy++);

   		// Log a state transition
    	xTraceStateMachineSetState(myfsm, myfsm_state[state]);

    	// Make the task run longer...
    	for (dummy = 0; dummy < (1000 * len); dummy++);

    	// Simulate timing of incoming messages
    	delay = 5 + getHardwareRand() % 85;

    	vTaskDelay(delay);
    }

}


char gcc_build_id[48];

/**
 * @brief Application runtime entry point.
 */
int main( void )
{
	char tmp[48];
	uint32_t bytesWritten = 0;

	/* Perform any hardware initialization that does not require the RTOS to be
     * running.  */
    prvMiscInitialization();

    if (myGetDeviceName(tmp, sizeof(tmp), &bytesWritten) == DFM_SUCCESS)
    {
    	configPRINT_STRING(("DeviceName: "));
    	configPRINT_STRING((tmp));
    	configPRINT_STRING(("\n"));
    }
    else
    {
    	configPRINT_STRING(("DeviceName: FAIL\n"));
    }

    if (myGetUniqueSessionID(tmp, sizeof(tmp), &bytesWritten) == DFM_SUCCESS)
    {
    	configPRINT_STRING(("SessionID: "));
    	configPRINT_STRING((tmp));
    	configPRINT_STRING(("\n"));

    }
    else
    {
    	configPRINT_STRING(("SessionID: FAIL\n"));
    }


    BSP_LED_Off( LED_GREEN );

    if( xTaskCreate( prvRXTask, "RX", 1000, NULL, 6, NULL ) != pdPASS )
    {
    	configPRINT_STRING(("Failed creating task."));
    }

    /* Create tasks that are not dependent on the WiFi being initialized. */

    /*
    xLoggingTaskInitialize( mainLOGGING_TASK_STACK_SIZE,
                            mainLOGGING_TASK_PRIORITY,
                            mainLOGGING_MESSAGE_QUEUE_LENGTH );
    */

    /* Start the scheduler.  Initialization that requires the OS to be running,
     * including the WiFi initialization, is performed in the RTOS daemon task
     * startup hook. */
    vTaskStartScheduler();

    return 0;
}
/*-----------------------------------------------------------*/

/* For STM32, tested on STM32L475 */
unsigned int getHardwareRand(void)
{
    uint32_t rand;
    HAL_RNG_GenerateRandomNumber(&xHrng, &rand);
    return rand;
}

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
    configPRINTF( ( "\n\n\n\n--- Percepio DevAlert Demonstration ---\n" ) );

    // Can be read from elf file by "arm-none-eabi-readelf -n aws_demos.elf"
    configPRINTF(("Revision: %s\n", DFM_CFG_FIRMWARE_VERSION));

#if (DEMO_CFG_SERIAL_UPLOAD_ONLY == 1)
    configPRINTF(("Upload method: Serial (upload via host computer)\n"));
#else
    configPRINTF(("Upload method: AWS_MQTT (direct upload to cloud)\n"));
#endif

/* For testing the serial port upload, Wifi/AWS connectivity not needed */
#if (DEMO_CFG_SERIAL_UPLOAD_ONLY != 1)

    WIFIReturnCode_t xWifiStatus;

    /* Turn on the WiFi before key provisioning. This is needed because
     * if we want to use offload SSL, device certificate and key is stored
     * on the WiFi module during key provisioning which requires the WiFi
     * module to be initialized. */

    xWifiStatus = WIFI_On();

    if( xWifiStatus == eWiFiSuccess )
    {
        configPRINTF( ( "WiFi module initialized.\r\n" ) );

        /* A simple example to demonstrate key and certificate provisioning in
         * microcontroller flash using PKCS#11 interface. This should be replaced
         * by production ready key provisioning mechanism. */

        if( SYSTEM_Init() == pdPASS )
        {
        	// Must be after SYSTEM_Init
        	vDevModeKeyProvisioning();

        	/* Connect to the WiFi before running the demos */
            prvWifiConnect();

            srand(getHardwareRand());

            // When using AWS MQTT upload, DFM is initialized after the connection is established.
            if (xDfmInitialize(myGetUniqueSessionID, myGetDeviceName) == DFM_FAIL)
            {
                configPRINTF(("Failed to initialize DFM\r\n"));
            }

            #ifdef USE_OFFLOAD_SSL
                /* Check if WiFi firmware needs to be updated. */

                prvCheckWiFiFirmwareVersion();

            #endif /* USE_OFFLOAD_SSL */

            configPRINTF(("DFM: Sending any stored alerts.\n"));

            /* Try sending any stored alerts from a prior crash */
            xDfmAlertSendAll();

            /* Reset and delete any alerts from storage - this is specific for the STM32 FLASH storage port. */
            xDfmStoragePortReset();

            xTaskCreate(ButtonTask,       /* Function that implements the task. */
                        "DemoTask1",          /* Text name for the task. */
                        1024,           /* Stack size in words, not bytes. */
                        NULL,    		/* Parameter passed into the task. */
                        tskIDLE_PRIORITY,/* Priority at which the task is created. */
                        &xBhjHandle );      /* Used to pass out the created task's handle. */

            BSP_LED_On( LED_GREEN );

        }
    }
    else
    {
        configPRINTF( ( "WiFi module failed to initialize.\r\n" ) );

        /* Stop here if we fail to initialize WiFi. */
        configASSERT( xWifiStatus == eWiFiSuccess );
    }

#else

#if (0)

    {
    	static DfmAlertHandle_t xAlertHandle = 0;
    	char* payloadString = "Example Payload from DFM";

		if (xDfmAlertBegin(DFM_TYPE_ASSERT_FAILED, "Demo alert", &xAlertHandle) == DFM_SUCCESS)
    	{
    		xDfmAlertAddSymptom(xAlertHandle, DFM_SYMPTOM_LINE, __LINE__);
    		xDfmAlertAddPayload(xAlertHandle, payloadString, strlen(payloadString), "demo_payload.txt");
    		xDfmAlertEnd(xAlertHandle);
    	}

		for(;;);
    }
#else
    /* Basic demo initialization for serial port upload */
	xTaskCreate(ButtonTask,       /* Function that implements the task. */
			   "DemoTask1",          /* Text name for the task. */
			   1024,           /* Stack size in words, not bytes. */
			   NULL,    		/* Parameter passed into the task. */
			   tskIDLE_PRIORITY,/* Priority at which the task is created. */
			   &xBhjHandle );      /* Used to pass out the created task's handle. */

	/* Test - Getting a crashes in FreeRTOS functions when testing the Stack Overflow issue, but this didn't help.
	 * The crashes occur BEFORE the stack overflow test, but the behavior changes if I modify the "data" stack allocation in the switch statement above.
	 * This is weird. Might be that I'm getting stack overflows at an earlier point?
	 * Needs further digging. NOTE: RECORDER DISABLED (that wasn't the problem)*/
	stackoverflowpadding = pvPortMalloc(4096);

	#endif

#if (0)
	/* Try sending any stored alerts from a prior crash */
	/* Not needed when using serial output, as no data is stored on the device */
	{
    	int dfmResult = DFM_SUCCESS;

		dfmResult = xDfmAlertSendAll();

		if ((dfmResult == DFM_FAIL) || (dfmResult == DFM_NO_ALERTS))
		{
		   configPRINTF(("DFM: No stored alerts.\n\n"));
		}
		else if (dfmResult == DFM_SUCCESS)
		{
			configPRINTF(("DFM: Found and uploaded alerts.\n\n"));

			/* Erase stored alerts to avoid they are sent repeatedly */
			xDfmStoragePortReset();

		}
		else
		{
			configPRINTF(("DFM: Unexpected return code (%d)!\n\n", dfmResult));
		}

		// Needed?
		xDfmSessionSetStorageStrategy(DFM_STORAGE_STRATEGY_OVERWRITE);
	}
#endif

	BSP_LED_On( LED_GREEN );

#endif

}
/*-----------------------------------------------------------*/

#if (DEMO_CFG_SERIAL_UPLOAD_ONLY != 1)

static void prvWifiConnect( void )
{
    WIFINetworkParams_t xNetworkParams = { 0 };
    WIFIReturnCode_t xWifiStatus = eWiFiSuccess;
    WIFIIPConfiguration_t xIpConfig;
    uint8_t *pucIPV4Byte;
    size_t xSSIDLength, xPasswordLength;

    /* Setup WiFi parameters to connect to access point. */
    if( clientcredentialWIFI_SSID != NULL )
    {
        xSSIDLength = strlen( clientcredentialWIFI_SSID );
        if( ( xSSIDLength > 0 ) && ( xSSIDLength <= wificonfigMAX_SSID_LEN ) )
        {
            xNetworkParams.ucSSIDLength = xSSIDLength;
            memcpy( xNetworkParams.ucSSID, clientcredentialWIFI_SSID, xSSIDLength );
        }
        else
        {
            configPRINTF(( "Invalid WiFi SSID configured, empty or exceeds allowable length %d.\n", wificonfigMAX_SSID_LEN ));
            xWifiStatus = eWiFiFailure;
        }
    }
    else
    {
        configPRINTF(( "WiFi SSID is not configured.\n" ));
        xWifiStatus = eWiFiFailure;
    }

    xNetworkParams.xSecurity = clientcredentialWIFI_SECURITY;
    if( clientcredentialWIFI_SECURITY == eWiFiSecurityWPA2 )
    {
        if( clientcredentialWIFI_PASSWORD != NULL )
        {
            xPasswordLength = strlen( clientcredentialWIFI_PASSWORD );
            if( ( xPasswordLength > 0 ) && ( xSSIDLength <= wificonfigMAX_PASSPHRASE_LEN ) )
            {
                xNetworkParams.xPassword.xWPA.ucLength = xPasswordLength;
                memcpy( xNetworkParams.xPassword.xWPA.cPassphrase, clientcredentialWIFI_PASSWORD, xPasswordLength );
            }
            else
            {
                configPRINTF(( "Invalid WiFi password configured, empty password or exceeds allowable password length %d.\n", wificonfigMAX_PASSPHRASE_LEN ));
                xWifiStatus = eWiFiFailure;
            }
        }
        else
        {
            configPRINTF(( "WiFi password is not configured.\n" ));
            xWifiStatus = eWiFiFailure;
        }
    }
    xNetworkParams.ucChannel = 0;

    if( xWifiStatus == eWiFiSuccess )
    {
    	configPRINTF( ( "Connecting to WiFi..." ) );

        /* Try connecting using provided wifi credentials. */
        xWifiStatus = WIFI_ConnectAP( &( xNetworkParams ) );

        if( xWifiStatus == eWiFiSuccess )
        {
            configPRINTF( ( "WiFi connected!") );

            /* Get IP address of the device. */
            xWifiStatus = WIFI_GetIPInfo( &xIpConfig );
            if( xWifiStatus == eWiFiSuccess )
            {
                pucIPV4Byte = ( uint8_t * ) ( &xIpConfig.xIPAddress.ulAddress[0] );
                configPRINTF( ( "IP Address acquired %d.%d.%d.%d.\r\n",
                        pucIPV4Byte[ 0 ], pucIPV4Byte[ 1 ], pucIPV4Byte[ 2 ], pucIPV4Byte[ 3 ] ) );
            }
        }
        else
        {
            /* Connection failed configure softAP to allow user to set wifi credentials. */
            configPRINTF( ( "WiFi failed to connect to AP %.*s.\r\n", xNetworkParams.ucSSIDLength, ( char * ) xNetworkParams.ucSSID  ) );
        }
    }

    if( xWifiStatus != eWiFiSuccess )
    {
        /* Enter SOFT AP mode to provision access point credentials. */
        configPRINTF(( "Entering soft access point WiFi provisioning mode.\n" ));
        xWifiStatus = eWiFiSuccess;
        if( wificonfigACCESS_POINT_SSID_PREFIX != NULL )
        {
            xSSIDLength = strlen( wificonfigACCESS_POINT_SSID_PREFIX );
            if( ( xSSIDLength > 0 ) && ( xSSIDLength <= wificonfigMAX_SSID_LEN ) )
            {
                xNetworkParams.ucSSIDLength = xSSIDLength;
                memcpy( xNetworkParams.ucSSID, clientcredentialWIFI_SSID, xSSIDLength );
            }
            else
            {
                configPRINTF(( "Invalid AP SSID configured, empty or exceeds allowable length %d.\n", wificonfigMAX_SSID_LEN ));
                xWifiStatus = eWiFiFailure;
            }
        }
        else
        {
            configPRINTF(( "WiFi AP SSID is not configured.\n" ));
            xWifiStatus = eWiFiFailure;
        }

        xNetworkParams.xSecurity = wificonfigACCESS_POINT_SECURITY;
        if( wificonfigACCESS_POINT_SECURITY == eWiFiSecurityWPA2 )
        {
            if( wificonfigACCESS_POINT_PASSKEY != NULL )
            {
                xPasswordLength = strlen( wificonfigACCESS_POINT_PASSKEY );
                if( ( xPasswordLength > 0 ) && ( xSSIDLength <= wificonfigMAX_PASSPHRASE_LEN ) )
                {
                    xNetworkParams.xPassword.xWPA.ucLength = xPasswordLength;
                    memcpy( xNetworkParams.xPassword.xWPA.cPassphrase, wificonfigACCESS_POINT_PASSKEY, xPasswordLength );
                }
                else
                {
                    configPRINTF(( "Invalid WiFi AP password, empty or exceeds allowable password length %d.\n", wificonfigMAX_PASSPHRASE_LEN ));
                    xWifiStatus = eWiFiFailure;
                }
            }
            else
            {
                configPRINTF(( "WiFi AP password is not configured.\n" ));
                xWifiStatus = eWiFiFailure;
            }
        }
        xNetworkParams.ucChannel = wificonfigACCESS_POINT_CHANNEL;

        if( xWifiStatus == eWiFiSuccess )
        {
            configPRINTF( ( "Connect to softAP %.*s using password %.*s. \r\n",
                    xNetworkParams.ucSSIDLength, ( char * ) xNetworkParams.ucSSID,
                    xNetworkParams.xPassword.xWPA.ucLength, xNetworkParams.xPassword.xWPA.cPassphrase ) );
            do
            {
                xWifiStatus = WIFI_ConfigureAP( &xNetworkParams );
                configPRINTF( ( "Connect to softAP and configure wiFi returned %d\r\n", xWifiStatus ) );
                vTaskDelay( pdMS_TO_TICKS( 1000 ) );

            } while( ( xWifiStatus != eWiFiSuccess ) && ( xWifiStatus != eWiFiNotSupported ) );
        }
    }

    configASSERT( xWifiStatus == eWiFiSuccess );
}
#endif
/*-----------------------------------------------------------*/

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
/*-----------------------------------------------------------*/

/**
 * @brief Publishes a character to the STM32L475 UART
 *
 * This is used to implement the tinyprintf created by Spare Time Labs
 * http://www.sparetimelabs.com/tinyprintf/tinyprintf.php
 *
 * @param pv    unused void pointer for compliance with tinyprintf
 * @param ch    character to be printed
 */
void vSTM32L475putc( void * pv,
                     char ch )
{
    int32_t status;

    do {
        status = iot_uart_write_sync( xConsoleUart, ( uint8_t * ) &ch, 1 );
    } while( status == IOT_UART_BUSY );
}
/*-----------------------------------------------------------*/

/**
 * @brief Read a character from the STM32L475 UART
 *
 * Implemented by BHJ - Seems that it does not work
 *
 * @param pv    unused void pointer for compliance with tinyprintf
 * @param ch    character to be printed
 */
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
/*-----------------------------------------------------------*/


/**
 * @brief Initializes the board.
 */
static void prvMiscInitialization( void )
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock. */
    SystemClock_Config();

    /* Heap_5 is being used because the RAM is not contiguous in memory, so the
     * heap must be initialized. */
    prvInitializeHeap();


	// Enable usage fault and bus fault
	SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk;

    /* Init and start tracing */
    xTraceEnable(TRC_START);

    BSP_LED_Init( LED_GREEN );
    BSP_PB_Init( BUTTON_USER, BUTTON_MODE_EXTI );

    /* RNG init function. */
    xHrng.Instance = RNG;

    if( HAL_RNG_Init( &xHrng ) != HAL_OK )
    {
        Error_Handler();
    }

    /* RTC init. */
    RTC_Init();

    /* UART console init. */
    Console_UART_Init();

    vDfmSetGCCBuildID(gcc_build_id, 48);
    printf("BuildID: %s\n", gcc_build_id);


#if (DEMO_CFG_SERIAL_UPLOAD_ONLY == 1)

    // DFM can be initialized quite early when using the Serial upload

    if (xDfmInitialize(myGetUniqueSessionID, myGetDeviceName) == DFM_FAIL)
    {
    	configPRINTF(("Failed to initialize DFM\r\n"));
    }

#endif

}


DfmResult_t myGetUniqueSessionID(char cBuffer[], uint32_t ulSize, uint32_t* pulBytesWritten)
{
	uint32_t nBytes;
	nBytes = snprintf(cBuffer, ulSize, "S-%X-%X-%X", getHardwareRand(), getHardwareRand(), getHardwareRand());
	if (nBytes > 0)
	{
		*pulBytesWritten = nBytes;
		return DFM_SUCCESS;
	}

	return DFM_FAIL;
}


/*** Serial number addresses for STM32Lx ***/
#define DEVICE_ID1 (unsigned int*)(0x1FFF7590)
#define DEVICE_ID2 (unsigned int*)(0x1FFF7594)
#define DEVICE_ID3 (unsigned int*)(0x1FFF7598)

DfmResult_t myGetDeviceName(char cBuffer[], uint32_t ulSize, uint32_t* pulBytesWritten)
{
	uint32_t nBytes;
	nBytes = snprintf(cBuffer, ulSize, "D-%08X-%08X-%08X", *DEVICE_ID1, *DEVICE_ID2, *DEVICE_ID3);

	if (nBytes > 0)
	{
		*pulBytesWritten = nBytes;
		return DFM_SUCCESS;
	}

	return DFM_FAIL;
}


/*-----------------------------------------------------------*/

/**
 * @brief Initializes the system clock.
 */
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
/*-----------------------------------------------------------*/

/**
 * @brief UART console initialization function.
 */
static void Console_UART_Init( void )
{
    int32_t status = IOT_UART_SUCCESS;

     /* Default setting:
      * Mode: UART_MODE_TX_RX;
      * OverSampling: UART_OVERSAMPLING_16;
      * OneBitSampling: UART_ONE_BIT_SAMPLE_DISABLE;
      * AdvancedInit.AdvFeatureInit: UART_ADVFEATURE_NO_INIT; */
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
/*-----------------------------------------------------------*/

/**
 * @brief RTC init function.
 */
static void RTC_Init( void )
{
    RTC_TimeTypeDef xsTime;
    RTC_DateTypeDef xsDate;

    /* Initialize RTC Only. */
    xHrtc.Instance = RTC;
    xHrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    xHrtc.Init.AsynchPrediv = 127;
    xHrtc.Init.SynchPrediv = 255;
    xHrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    xHrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    xHrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    xHrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    if( HAL_RTC_Init( &xHrtc ) != HAL_OK )
    {
        Error_Handler();
    }

    /* Initialize RTC and set the Time and Date. */
    xsTime.Hours = 0x12;
    xsTime.Minutes = 0x0;
    xsTime.Seconds = 0x0;
    xsTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    xsTime.StoreOperation = RTC_STOREOPERATION_RESET;

    if( HAL_RTC_SetTime( &xHrtc, &xsTime, RTC_FORMAT_BCD ) != HAL_OK )
    {
        Error_Handler();
    }

    xsDate.WeekDay = RTC_WEEKDAY_FRIDAY;
    xsDate.Month = RTC_MONTH_JANUARY;
    xsDate.Date = 0x24;
    xsDate.Year = 0x17;

    if( HAL_RTC_SetDate( &xHrtc, &xsDate, RTC_FORMAT_BCD ) != HAL_OK )
    {
        Error_Handler();
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief  This function is executed in case of error occurrence.
 */
void Error_Handler( void )
{
    while( 1 )
    {
        BSP_LED_Toggle( LED_GREEN );
        HAL_Delay( 200 );
    }
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
}
/*-----------------------------------------------------------*/

void vMainUARTPrintString( char * pcString )
{

    /* Ignore returned status. */
    iot_uart_write_sync( xConsoleUart, ( uint8_t * ) pcString, strlen( pcString ) );
}
/*-----------------------------------------------------------*/

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
	DFM_TRAP(DFM_TYPE_MALLOC_FAILED, "Could not allocate heap memory");
}

/*-----------------------------------------------------------*/

#ifdef USE_OFFLOAD_SSL
#if (DEMO_CFG_SERIAL_UPLOAD_ONLY != 1)

    static void prvCheckWiFiFirmwareVersion( void )
    {
        int32_t lWicedMajorVersion = 0, lWicedMinorVersion = 0, lWicedPatchVersion = 0, lInventekVersion = 0, lParsedFields = 0;
        uint8_t ucFirmwareVersion[ ES_WIFI_FW_REV_SIZE ];

        BaseType_t xNeedsUpdate = pdFALSE;

        if( WIFI_GetFirmwareVersion( &( ucFirmwareVersion[ 0 ] ) ) == eWiFiSuccess )
        {
            configPRINTF( ( "WiFi firmware version is: %s\r\n", ( char * ) ucFirmwareVersion ) );

            /* Parse the firmware revision number. */
            lParsedFields = sscanf( ( char * ) ucFirmwareVersion,
                                    "C%ld.%ld.%ld.%ld.STM",
                                    &( lWicedMajorVersion ),
                                    &( lWicedMinorVersion ),
                                    &( lWicedPatchVersion ),
                                    &( lInventekVersion ) );

            /* Check if the firmware version needs to be updated. */
            if( lParsedFields > 0 )
            {
                if( lWicedMajorVersion < mainREQUIRED_WIFI_FIRMWARE_WICED_MAJOR_VERSION )
                {
                    xNeedsUpdate = pdTRUE;
                }
                else if( ( lWicedMajorVersion == mainREQUIRED_WIFI_FIRMWARE_WICED_MAJOR_VERSION ) &&
                         ( lWicedMinorVersion < mainREQUIRED_WIFI_FIRMWARE_WICED_MINOR_VERSION ) )
                {
                    xNeedsUpdate = pdTRUE;
                }
                else if( ( lWicedMajorVersion == mainREQUIRED_WIFI_FIRMWARE_WICED_MAJOR_VERSION ) &&
                         ( lWicedMinorVersion == mainREQUIRED_WIFI_FIRMWARE_WICED_MINOR_VERSION ) &&
                         ( lWicedPatchVersion < mainREQUIRED_WIFI_FIRMWARE_WICED_PATCH_VERSION ) )
                {
                    xNeedsUpdate = pdTRUE;
                }
                else if( ( lWicedMajorVersion == mainREQUIRED_WIFI_FIRMWARE_WICED_MAJOR_VERSION ) &&
                         ( lWicedMinorVersion == mainREQUIRED_WIFI_FIRMWARE_WICED_MINOR_VERSION ) &&
                         ( lWicedPatchVersion == mainREQUIRED_WIFI_FIRMWARE_WICED_PATCH_VERSION ) &&
                         ( lInventekVersion < mainREQUIRED_WIFI_FIRMWARE_INVENTEK_VERSION ) )
                {
                    xNeedsUpdate = pdTRUE;
                }
                else if( NULL == strstr( (char*)ucFirmwareVersion, mainREQUIRED_WIFI_FIRMWARE_DESCRIPTOR_STRING ) )
                {
                    xNeedsUpdate = pdTRUE;
                }

                /* Print a warning for the user to inform that the WiFi Firmware
                 * needs to be updated. */
                if( xNeedsUpdate == pdTRUE )
                {
                    configPRINTF( ( "[WARN] WiFi firmware needs to be updated.\r\n" ) );
                }
                else
                {
                    configPRINTF( ( "WiFi firmware is up-to-date.\r\n" ) );
                }
            }
            else
            {
                configPRINTF( ( "Failed to parse the WiFi firmware version.\r\n" ) );
            }
        }
        else
        {
            configPRINTF( ( "Failed to get WiFi firmware version.\r\n" ) );
        }
    }

#endif
#endif /* USE_OFFLOAD_SSL */

    /*-----------------------------------------------------------*/

/**
 * @brief  EXTI line detection callback.
 *
 * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
 */
void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    switch( GPIO_Pin )
    {
        /* Pin number 1 is connected to Inventek Module Cmd-Data
         * ready pin. */
        case ( GPIO_PIN_1 ):
            //SPI_WIFI_ISR();
            break;
        case ( USER_BUTTON_PIN ):
            vTaskNotifyGiveFromISR( xBhjHandle, &xHigherPriorityTaskWoken );
            break;

        default:
            break;
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief SPI Interrupt Handler.
 *
 * @note Inventek module is configured to use SPI3.
 */
void SPI3_IRQHandler( void )
{
    // HAL_SPI_IRQHandler( &( hspi ) );
}
/*-----------------------------------------------------------*/

/**
 * @brief Period elapsed callback in non blocking mode
 *
 * @note This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 *
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback( TIM_HandleTypeDef * htim )
{
    if( htim->Instance == TIM6 )
    {
        HAL_IncTick();
    }
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                     char * pcTaskName )
 {

 	// TODO: Add Alert here
 	configPRINT_STRING( ( "ERROR: stack overflow\r\n" ) );
     portDISABLE_INTERRUPTS();

     DFM_TRAP(DFM_TYPE_STACK_OVERFLOW, "Stack overflow");

     /* Unused Parameters */
     ( void ) xTask;
     ( void ) pcTaskName;

     /* Loop forever */
     for( ; ; )
     {
     }
 }
