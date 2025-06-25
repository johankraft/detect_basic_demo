#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "trcRecorder.h"

/******************************************************************************
 * demo_data_logging.c
 *
 * Demonstrates the use of TraceRecorder "user events" for custom data logging. 
 * The data is included in the trace data for Percepio Tracealyzer, that displays
 * it at events and plots.
 *
 * The demo application reads the accelerometer on the STMicro B-L475-IOT01
 * board and logs the data using the xTracePrintF function of the TraceRecorder
 * library. See TraceRecorder/include/trcPrint.h for more details. 
 *
 * See also https://percepio.com/understanding-your-application-with-user-events.
 *
 * This can be used both with Tracealyzer as a stand-alone tool, and together
 * with Percepio Detect for systematic observability on errors and anomalies.
 * 
 * Learn more in main.c and at https://percepio.com/tracealyzer.
 *****************************************************************************/

void vTaskAccelerometer(void *pvParameters)
{
    (void) pvParameters;
    TraceStringHandle_t AccZ_chn;
    int16_t pDataXYZ[3] = {0};
    
    BSP_ACCELERO_Init();
    
    xTraceStringRegister("AccZ", &AccZ_chn);
    
    for (;;)
    {
        BSP_ACCELERO_AccGetXYZ(pDataXYZ);        
        xTracePrintF(AccZ_chn, "%d", (uint32_t)pDataXYZ[2]);
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void demo_data_logging(void)
{

  TaskHandle_t hnd = NULL;
  
  /* Note: The TraceRecorder library must be initialized first (see main.c) */
  
  printf("\n\rdemo_data_logging - logs accelerometer data to TraceRecorder.\n\r"
             "Physically move the board and then halt quickly. When halted, take a snapshot\n\r"
             "of the trace buffer and open in Percepio Tracealyzer. The sensor data is can be\n\r"
             "seen as yellow \"User Events\" and is also plotted in the User Event Signal Plot.\n\r\n\r" );
  
  /* To learn how to take snapshots from TraceRecorder and see them in Tracealyzer,
   see https://percepio.com/tracealyzer/gettingstarted/ and scroll down to 
   "Guides for specific development tools", or consult the Tracealyzer User Manual. */
  
  xTaskCreate(
      vTaskAccelerometer,
      "vTaskAccelerometer",
      configMINIMAL_STACK_SIZE*4,
      NULL,
      tskIDLE_PRIORITY + 1,
      &hnd
  );
  
  vTaskDelay(5000);
  
  vTaskDelete(hnd);
}
  
