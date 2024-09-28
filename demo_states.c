/*******************************************************************************
 * demo_state.c
 *
 * Demonstrates state tracing for Tracealyzer using the trcStateMachine API in
 * TraceRecorder.
 *
 * xTraceStateMachineCreate
 * Registers a "state machine" in TraceRecorder for tracing a state variable.
 * Call once per state machine (state variable) during the initialization.
 *
 * xTraceStateMachineStateCreate:
 * Registers a state name for a specific state machine. Call once per state
 * during the initialization.
 *
 * xTraceStateMachineSetState:
 * Traces a state transition event.
 *
 * See full documentation in TraceRecorder/include/trcStateMachine.h.
 */

#include "main.h"
#include "stdint.h"
#include "stdarg.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "iot_uart.h"

/* Percepio includes */
#include "trcRecorder.h"

#include "demo_states.h"

void DemoStatesInit(void);
void DemoSetMotorState(int is_on);
void DemoSetBrakeState(int is_on);
void DemoTaskStates(void* argument);
void DemoSimulateExecutionTime(int n);

extern void DemoSimulateExecutionTime(int n);

TraceStateMachineHandle_t statemachine_motor;
TraceStateMachineStateHandle_t motor_state_handle[3];

TraceStateMachineHandle_t statemachine_brake;
TraceStateMachineStateHandle_t brake_state_handle[3];

int brake_state = 0;
int motor_state = 0;

int DemoGetMotorState(void)
{
	return motor_state;
}

int DemoGetBrakeState(void)
{
	return brake_state;
}

void DemoSetMotorState(int state)
{
	DemoSimulateExecutionTime(1500);

	// Valid states are 0, 1, 2
	if (state < 0 || state > 2)
	{
		// Error, invalid state!
		return;
	}

	motor_state = state;

	xTraceStateMachineSetState(statemachine_motor, motor_state_handle[state]);

	DemoSimulateExecutionTime(500);
}


void DemoSetBrakeState(int state)
{
	DemoSimulateExecutionTime(1000);

	// Valid states are 0, 1, 2
	if (state < 0 || state > 2)
	{
		// Error, invalid state!
		return;
	}

	brake_state = state;

	xTraceStateMachineSetState(statemachine_brake, brake_state_handle[state]);

	DemoSimulateExecutionTime(1000);
}


void DemoTaskStates(void* argument)
{
	/* Registers the two state machines for tracing the state variables. */
	xTraceStateMachineCreate("Motor State", &statemachine_motor);
	xTraceStateMachineCreate("Brake State", &statemachine_brake);

	/* Registers the Motor state names. */
	xTraceStateMachineStateCreate(statemachine_motor, "MOTOR_FULL", &motor_state_handle[MOTOR_FULL]);
	xTraceStateMachineStateCreate(statemachine_motor, "MOTOR_SOFT", &motor_state_handle[MOTOR_SOFT]);
	xTraceStateMachineStateCreate(statemachine_motor, "MOTOR_OFF", &motor_state_handle[MOTOR_OFF]);

	/* Registers the Brake state names. */
	xTraceStateMachineStateCreate(statemachine_brake, "BRAKE_FULL", &brake_state_handle[BRAKE_FULL]);
	xTraceStateMachineStateCreate(statemachine_brake, "BRAKE_SOFT", &brake_state_handle[BRAKE_SOFT]);
	xTraceStateMachineStateCreate(statemachine_brake, "BRAKE_OFF", &brake_state_handle[BRAKE_OFF]);

    for(;;)
    {
    	DemoSetMotorState(MOTOR_SOFT);
    	DemoSetBrakeState(BRAKE_OFF);

    	vTaskDelay(2);

    	DemoSetMotorState(MOTOR_FULL);

    	vTaskDelay(10);

    	DemoSetMotorState(MOTOR_OFF);
    	DemoSetBrakeState(BRAKE_SOFT);

    	vTaskDelay(2);

    	DemoSetBrakeState(BRAKE_FULL);

    	vTaskDelay(10);
    }
}

void DemoStatesInit(void)
{
   if( xTaskCreate( DemoTaskStates, "DemoState", 1000, NULL, 6, NULL ) != pdPASS )
   {
	   configPRINT_STRING(("Failed creating DemoTaskStates."));
   }
}
