#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "dfm.h"

/******************************************************************************
 * demo_kernel_tracing.c
 *
 * Demonstrates inter-task communication, synchronization and timing using
 * FreeRTOS queue and mutex with variable execution time.
 *
 * See also https://percepio.com/tracealyzer.
 *****************************************************************************/

#define QUEUE_LENGTH 10
#define QUEUE_ITEM_SIZE sizeof(int)

static QueueHandle_t xQueue = NULL;
static SemaphoreHandle_t xMutex = NULL;

static void vTask1(void *pvParameters);
static void vTask2(void *pvParameters);
static void vTask3(void *pvParameters);

static void dummy_exectime(int min, int max);

void vTask1(void *pvParameters)
{
    (void) pvParameters;

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10);
    
    xLastWakeTime = xTaskGetTickCount();
    
    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        dummy_exectime(6000, 7000);
    
        // Send dummy message to queue
        int msg = rand();
        xQueueSend(xQueue, &msg, 0);
    
    }
}
        
        
void vTask2(void *pvParameters)
{
    (void) pvParameters;
    
    int msg;
    
    for (;;)
    {
        // Receive dummy message from queue
        if (xQueueReceive(xQueue, &msg, portMAX_DELAY) == pdPASS)
        {

            dummy_exectime(5000, 6000);
            
            xSemaphoreTake(xMutex, portMAX_DELAY);
            dummy_exectime(3000, 5000);
            xSemaphoreGive(xMutex);                       
        }
    }
}


void vTask3(void *pvParameters)
{
    (void) pvParameters;
    
    for (;;)
    {        
        xSemaphoreTake(xMutex, portMAX_DELAY);
        dummy_exectime(3000, 5000);
        xSemaphoreGive(xMutex);
        
        dummy_exectime(3000, 4000);   
         
        vTaskDelay(pdMS_TO_TICKS(11));
    
    }
}


void demo_kernel_tracing(void)
{
    TaskHandle_t hndTask1 = NULL;
    TaskHandle_t hndTask2 = NULL;
    TaskHandle_t hndTask3 = NULL;

    // Create queue
    xQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    
    // Create mutex
    xMutex = xSemaphoreCreateMutex();

    if (xQueue == NULL || xMutex == NULL)
    {
        printf("Failed to create queue or mutex\n");
        return;
    }

    // Set object names
    vTraceSetQueueName(xQueue, "My Queue");
    vTraceSetMutexName(xMutex, "My Mutex");
    
    printf("\ndemo_kernel_tracing\n");
    
    xTaskCreate(
        vTask1,
        "vTask1",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 3,
        &hndTask1
    );
    
    xTaskCreate(
        vTask2,
        "vTask2",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 2,
        &hndTask2
    );
    
    xTaskCreate(
        vTask3,
        "vTask3",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 4, 
        &hndTask3
    );
   
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    vTaskDelete(hndTask1);
    vTaskDelete(hndTask2);      
    vTaskDelete(hndTask3);
    
    // Delete queue and mutex
    vQueueDelete(xQueue);
    vSemaphoreDelete(xMutex);
}

static void dummy_exectime(int min, int max)
{
     // Some execution time variations...
     int n = min + rand() % (max-min);
     for (volatile int i = 0; i < n; i++);
}
