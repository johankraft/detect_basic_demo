#ifndef _DEMO_ALERT_H_
#define _DEMO_ALERT_H_

void DemoAlertInit(void);

void DemoAlert(void);

void testAssertFailed(char* str);
void testBufferOverrun(void);
int dosomething(int n);
void testCoreDumpNoRestart(int n);

#endif
