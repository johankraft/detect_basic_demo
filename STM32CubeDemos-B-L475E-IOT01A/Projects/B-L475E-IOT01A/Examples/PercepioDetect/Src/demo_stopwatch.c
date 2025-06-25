#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "dfm.h"

/******************************************************************************
 * demo_stopwatch.c
 *
 * Demonstrates the use of the DFM Stopwatch feature for monitoring software
 * latencies. Alerts are provided to Percepio Detect if the latency is higher
 * than expected, together with a trace for debugging purposes. This can be used
 * not only to analyze execution time variations, but also for multithreading
 * issues that otherwise might be very hard to debug.
 * 
 * See also https://percepio.com/detect.
 *****************************************************************************/

dfmStopwatch_t* stopwatch;

void vComputeTask(void *pvParameters);
void vSporadicTask(void *pvParameters); 

void computeSomething(void);
void handleEvent(int eventCode);
int waitForEvent(void);


void vComputeTask(void *pvParameters) 
{
    while (1)
    {      
        vTaskDelay( pdMS_TO_TICKS(24) );
        
        // Start monitoring the latency
        vDfmStopwatchBegin(stopwatch);
        
        // Perform time-consuming processing
        computeSomething();
        
        // Check the elapsed time since begin. 
        // Generates a DFM alert to Percepio Detect if over the expected maximum (set in xDfmStopwatchCreate).
        vDfmStopwatchEnd(stopwatch);
    }
}

// May preempt and delay the ComputeTask since higher priority
void vSporadicTask(void *pvParameters) 
{    
    while (1)
    {
        int event = waitForEvent();
        
        // Simulated event processing.
        handleEvent(event);        
    }
}

void demo_stopwatch(void)
{

  TaskHandle_t hndTask1 = NULL;
  TaskHandle_t hndTask2 = NULL;

  /* Note: The DFM library and TraceRecorder must be initialized first (see main.c) */
    
  printf("\ndemo_stopwatch - detecting latency anomalies.\n");
  
  // Generates a DFM alert to Percepio Detect if over the expected maximum (specified in clock cycles).
  stopwatch = xDfmStopwatchCreate("ComputeTime", 300000);
    
  xTaskCreate(
      vComputeTask,     
      "PeriodicCompute",
      configMINIMAL_STACK_SIZE*4,
      NULL,
      tskIDLE_PRIORITY + 1, // Low priority
      &hndTask1
  );
  
  xTaskCreate(
      vSporadicTask,
      "SporadicEvent",
      configMINIMAL_STACK_SIZE*4,
      NULL,
      tskIDLE_PRIORITY + 3, // High priority
      &hndTask2
  );
    
  vTaskDelay(5000);
  printf("\n\rSummary:\n\r");
  vDfmStopwatchPrintAll();

  vDfmStopwatchClearAll();  
  
  vTaskDelete(hndTask1);
  vTaskDelete(hndTask2);
  
}
  

void computeSomething(void)
{
    int execTime = 15000 + rand() % 5000;

    // Simulate some processing time
    for (volatile int i=0; i < execTime; i++);
}

void handleEvent(int eventCode)
{
    (void)eventCode; // not used
    
    int execTime = 5000 + rand() % 1000;

    // Simulate some execution time
    for (volatile int i=0; i < execTime; i++);
}

int waitForEvent(void)
{
    vTaskDelay(rand() % 10);
    return 1;
}
