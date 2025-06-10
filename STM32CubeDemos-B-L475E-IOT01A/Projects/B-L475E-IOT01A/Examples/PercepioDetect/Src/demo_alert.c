#include "main.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"

#define ASSERT(x, msg) if(!(x)) DFM_TRAP(DFM_TYPE_ASSERT_FAILED, msg, 0);

void functionY(int arg1)
{
   /* If this assert fails, it calls DFM_TRAP to output an "alert" from DFM
      to Percepio Detect (core dump and trace). */
  
   ASSERT(arg1 >= 0, "Argument arg1 must not be negative");

   printf("Function Y\n");
}

void functionX(int a, int b)
{
   printf("Function X(%d, %d)\n", a, b);
  
   if (a > b)
   {
     functionY(a);
   }
   else
   {
     functionY(b);
   }  
}
  

void demo_alert(void)
{
   printf("demo_alert\n");
   
   functionX(-1, -2); 
}
  