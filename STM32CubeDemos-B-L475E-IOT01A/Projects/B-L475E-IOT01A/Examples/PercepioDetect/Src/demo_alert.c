#include "main.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"
#include "FreeRTOS.h"
#include "task.h"

/******************************************************************************
 * demo_alert.c
 * Demonstrates custom alerts for Percepio Detect. In this example, an ASSERT
 * macro fails because of a bad argument. This calls DFM_TRAP to output an alert
 * via the Percepio Detect API (the DFM library).
 *
 * DFM alerts are machine-readable error reports, containing metadata about the
 * issue and debug data captured at the error, including a small core dump
 * with the call-stack trace, as well as a TraceRecorder trace providing the
 * most recent events. Viewer tools are integrated in the Detect client and
 * launched when clicking on the "payload" links in the Detect dashboard.
 * 
 * Learn more in main.c and at https://percepio.com/detect.
 *****************************************************************************/

#define ASSERT(x, msg) if(!(x)) { DFM_TRAP(DFM_TYPE_ASSERT_FAILED, msg, 0); return;}

void functionY(int arg1)
{
   /* If this assert fails, it calls DFM_TRAP to output an "alert" from DFM
      to Percepio Detect (core dump and trace). */
  
   ASSERT(arg1 >= 0, "Assert failed, arg1 negative");

   printf("functionY(%d)\n", arg1);
}

void functionX(int a, int b)
{
   if (a > b)
   {
     functionY(a);
   }
   else
   {
     functionY(b);
   }  
}
  
TraceStringHandle_t demo_log_chn;

void demo_alert(void)
{  
   /* Note: The DFM library and TraceRecorder must be initialized first (see main.c) */
      
   xTraceStringRegister("Demo Log", &demo_log_chn);  

   printf("\n\rdemo_alert.c - A bad function argument causes a DFM alert.\n\r\n\r"
           "The error will be reported to Percepio Detect using the DFM library.\n\r"
           "When DFM data has been ingested by the Detect receiver, an alert will appear\n\r"
           "in the dashboard, with a Tracealyzer trace and a core dump providing\n\r"
           "the function call stack, arguments and local variables.\n\n");
   
   vTaskDelay(2000);
         
   for(volatile int i = 0; i < 100000; i++); // Simulate some processing
   
   // Valid function arguments
   functionX(3, 4); 
   
   // Invalid arguments, triggering an alert.
   functionX(-1, -3); 
   
   
    
}
  