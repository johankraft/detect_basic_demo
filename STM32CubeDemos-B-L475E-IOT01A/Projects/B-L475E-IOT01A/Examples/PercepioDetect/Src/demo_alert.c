#include "main.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"
#include "FreeRTOS.h"
#include "task.h"

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
   xTraceStringRegister("Demo Log", &demo_log_chn);
  
   printf("demo_alert.c - DFM alert follows in 2 sec.\n\n");
   xTracePrint(demo_log_chn, "demo_alert.c - DFM alert follows in 2 sec.");
   
   vTaskDelay(2000);
         
   for(volatile int i = 0; i < 100000; i++); // Simulate some processing
   functionX(3, 4); 
   functionX(-1, -3); 
   
}
  