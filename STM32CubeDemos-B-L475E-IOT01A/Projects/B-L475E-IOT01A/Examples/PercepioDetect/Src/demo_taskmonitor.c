#include <stdlib.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "dfm.h"

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

volatile int task1_spike = 0;
volatile int task2_blocked = 0;

void vTask1(void *pvParameters)
{
    (void) pvParameters;

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(5);
    
    xLastWakeTime = xTaskGetTickCount();
    
    for (;;)
    {

        vTaskDelayUntil( &xLastWakeTime, xFrequency );

        if (task1_spike == 1)
        {
            int n =  150000 + rand() % 50000;
            for (volatile int i=0; i<n; i++);
            task1_spike = 0;
        }
        else
        {
            int n =  7000 + rand() % 1000;
            for (volatile int i=0; i<n; i++);
        }
        
    }
}
        
        
void vTask2(void *pvParameters)
{
    (void) pvParameters;
    
    TickType_t xLastWakeTime;
    
    xLastWakeTime = xTaskGetTickCount();
    
    for (;;)
    {
      
        if (task2_blocked == 1)
        {
            vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(200) );
            task2_blocked = 0;
        }
        else
        {
            vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS(10) );
        }
        
        for (volatile int i=0; i<5000; i++);
    }
}

volatile int demo_done = 0;

void vTaskMonitor(void *pvParameters)
{
    (void) pvParameters;
         
    int demo_counter = 0;
    
    demo_done = 0;
    
    xTraceTaskMonitorPollReset();
    
    for (;;)
    {
        switch (demo_counter)
        {
          case 0:   printf("TaskMonitor example 1 - Nominal execution (no alerts)\n\r");                 
                    break;
          
          case 25:  printf("xDfmTaskMonitorPrint() shows high/low watermarks.\n\r");
                    printf("\n\r");
                    xDfmTaskMonitorPrint();
                    printf("\n\r");
                    break;
                    
          case 60:  printf("TaskMonitor example 2: Task 1 runs more than expected (sends alert)\n\r");
                    task1_spike = 1;  
                    break;
          
          case 95:  printf("TaskMonitor example 3: Task 2 runs less than expected (sends alert)\n\r");
                    task2_blocked = 1;  
                    break;
                    
          case 130: demo_done = 1;
                    vTaskSuspend(NULL); // Suspend this task, will be killed by the demo driver.
          
          default:  break;            
        }
      
        // Delay for 100 ms - the monitoring period. 
        // Important to use an absolute delay here, to ensure the time between
        // poll calls is sufficiently long. Otherwise, if using relative delays
        // that depend on the loop execution time, the monitoring period might
        // become too short or even zero in some cases, causing false alerts.
      
        vTaskDelay(pdMS_TO_TICKS(100));
        
        xTraceTaskMonitorPoll();
        
        int new_alerts = xDfmSessionGetNewAlerts();
        if (new_alerts > 0)
        {
           // Allow time to normalize (data output etc.)
           vTaskDelay(pdMS_TO_TICKS(500));
          
           printf("\n\r");
           printf("DFM reports %d new alerts have been sent.\r\nCalling xDfmTaskMonitorPrint:\n\r", new_alerts);
           printf("\n\r");
           xDfmTaskMonitorPrint();
           printf("\n\r");
           
           // Resets the task data.
           xTraceTaskMonitorPollReset();
        }
        
        demo_counter++;
        
    }
}

void demo_taskmonitor(void)
{

  TaskHandle_t hndTask1 = NULL;
  TaskHandle_t hndTask2 = NULL;
  TaskHandle_t hndMon = NULL;
    
  
  
  printf("\ndemo_taskmonitor - detecting CPU load anomalies in threads.\n");
    
  xTaskCreate(
      vTask1,
      "vTask1",
      configMINIMAL_STACK_SIZE*4,
      NULL,
      tskIDLE_PRIORITY + 3, // Higher priority than the demo driver.
      &hndTask1
  );
  
  xTaskCreate(
      vTask2,
      "vTask2",
      configMINIMAL_STACK_SIZE*4,
      NULL,
      tskIDLE_PRIORITY + 2,
      &hndTask2
  );
  
  xTaskCreate(
      vTaskMonitor,
      "vTaskMonitor",
      configMINIMAL_STACK_SIZE*4,
      NULL,
      tskIDLE_PRIORITY + 1, 
      &hndMon
  );
  
  // 0 - 30% CPU load is expected, otherwise make an alert.
  xDfmTaskMonitorRegister(hndTask1, 0, 30);
    
  // 2 - 98% CPU load is allowed, otherwise make an alert.
  xDfmTaskMonitorRegister(hndTask2, 2, 98);
    
  while (! demo_done)
  {
     vTaskDelay(1000);
  }
  
  xDfmTaskMonitorUnregister(hndTask1);  
  xDfmTaskMonitorUnregister(hndTask2);  
  
  vTaskDelete(hndTask1);
  vTaskDelete(hndTask2);      
  vTaskDelete(hndMon);
}
  
