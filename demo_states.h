#ifndef _DEMO_STATES_H_
#define _DEMO_STATES_H_

void DemoStatesInit(void);

int DemoGetBrakeState(void);

int DemoGetMotoreState(void);


#define MOTOR_OFF 0
#define MOTOR_SOFT 1
#define MOTOR_FULL 2

#define BRAKE_OFF 0
#define BRAKE_SOFT 1
#define BRAKE_FULL 2


#endif
