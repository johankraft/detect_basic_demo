#ifndef _DEMO_ISR_H_
#define _DEMO_ISR_H_

void DemoISRInit(void);

static volatile int demo_enabled = 0;

extern void DemoSimulateExecutionTime(int n);


#endif
