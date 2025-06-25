/******************************************************************************
 * demo_stack_monitoring.c
 *
 * Demonstrates how stack corruption can be reported using Percepio Detect.
 * The actual detection relies on commonly available compiler features for
 * stack checking, supported by at least gcc and IAR.
 *
 * If using gcc, use the compiler option -fstack-protector-strong.
 *
 * If using IAR, enable "Stack protection" in project options (found under
 * C/C++ Compiler -> Code)
 *
 * Learn more at https://percepio.com/detect
 *****************************************************************************/

#include "main.h"

/* Percepio includes */
#include "trcRecorder.h"
#include "dfm.h"

volatile int dummy = 0;

#define CHANNEL_B 2

int readTooMuchData(char* data, int max_size);

int readTooMuchData(char* data, int max_size)
{
    int i, len;
    char* incomingdata = "Incoming data...";
    
    i = 0;
    len = strlen(incomingdata) + 1; // Include zero termination
    
    while (i <= max_size && i <= len) // Bug here, causing buffer overrun...
    {
      data[i] = incomingdata[i];
      i++;
    }
}

void demo_stack_corruption(void)
{  
    char buffer[16];
    
    /* Note: The DFM library and TraceRecorder must be initialized first (see main.c) */
  
    printf("\n\rdemo_stack_monitoring.c - Shows how stack corruption can be reported\n\r"
           "using Percepio Detect. This relies on commonly available compiler features for\n\r"
           "stack checking, supported by e.g. gcc and IAR.\n\r\n\r"
           "The error will be reported to Percepio Detect using the DFM library.\n\r"
           "When DFM data has been ingested by the Detect receiver, an alert will appear\n\r"
           "in the dashboard, with a Tracealyzer trace and a core dump providing\n\r"
           "the function call stack, arguments and local variables.\n\n");
     
    readTooMuchData(buffer, sizeof(buffer));
}
