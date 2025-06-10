#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "trcRecorder.h"

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
  
  printf("demo_data_logging - logs accelerometer data (move the board!) \n");
  
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
  
