
#include <stdio.h>
#include <stdlib.h>
#include "dfmStoragePort.h"
#include "main.h"
#include "stdint.h"
#include "stdarg.h"

//#include "iot_uart.h"

#include "trcRecorder.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"

#include "demo_alert.h"
#include "demo_states.h"
#include "demo_user_events.h"
#include "demo_isr.h"

volatile int ButtonPressed = 0;


/******************************************************************************
 * REGISTER_TASK
 *
 * The TraceRecorder functions for RTOS task tracing is applicable also for
 * bare-metal systems, for example to show "idle time" in the trace views.
 *
 * This macro is used to manually register two tasks, "IDLE" and "main-thread".
 * where IDLE represents the HAL_Delay() call in main_superloop(). The macro
 * stores a Task ID together with a display name for Tracealyzer. The Task IDs
 * are passed as arguments to xTraceTaskSwitch() for tracing when entering and
 * leaving the HAL_Delay call.
 *
 * This is only needed for bare-metal systems. When using a supported RTOS
 * such as FreeRTOS, the tasks are registered and traced automatically.
 *****************************************************************************/
#define REGISTER_TASK(ID, name) xTraceTaskRegisterWithoutHandle((void*)ID, name, 0)

#define TASK_IDLE 100
#define TASK_MAIN 101

//static void DemoUpdate(int counter);
static void DemoInit(void);

TraceISRHandle_t xISRHandleTimer = 0;
TraceISRHandle_t xISRHandleOther = 0;

/*****
 * Changes:
 * - Added initial Stopwatch API. See serial output from alerts.
 * - Added xTracePause() and xTraceResume() to avoid reseting the trace.
 * - Optimized stopwatch API. Overhead now just 31-108 clock cycles on Arm.
 * - Solved hard fault issue in DFM caused by GCC bug on -O3.
 * - Demo app now remembers last test case, so it cycles through the alerts. Didn't require linker file changes (attribute noinit). Can be used to implement retained memory for any GCC target.
 * - Created a unit test framework and added 7 tests. Probably not complete, but seems decent for now. Consider adding a few more tests.
 * - Added new alert type and symptoms (server must be updated, see dfmCodes.h)
 * - Added print and clear functions in the API.
 * - Added id field, where 0 is invalid. Allows for checking if a stopwatch has been initialized.
 * - Fixed warnings and removed old invalid include paths from project.
 *
 * - Added "ready" events to get instances in Tz.
 *
 * TODO:
 * - Add stopwatch (or watchdog monitor?) on buffer read - expected 1 ms... Add something that causes a delay.
 *   Perhaps another ISR that runs with a different rate, only occationally interfering.
 *   And perhaps some if-statement in the main loop, that also adds to the execution time?
 *   When the perfect storm hits, the thread is delayed for too long.
 *   (near misses fairly common, error are rare but happen once in a while)
 *
 * - Finish and clean up demo code.
 * - Test loading alerts into Detect server.

 * - Consider adding a few more test cases.
 * - Allow for using DFM_TRAP without restarting.
 *
 * What to show in the demo?
 * - A plain "profiling" use-case (requirements)
 * - Sporadic "soft" error:
 * I'm simulating a sensor that is read by a (real) timer interrupt. This writes the data to a circular buffer,
 * and the main thread needs to read in a timely manner, or there will be data loss.
 * The main thread implements supersampling, i.e. it reads several samples and calculates the mean value.
 * This is monitored with user events so it can be plotted in Tracealyzer. But sometimes a disturbance/anomaly
 * delays the main thread so the buffer is filled more than usual, in worst case such that data is lost.
 * Here I'm just simulating another ISR for this, but will try to make a more realistic issue causing this delay,
 * that only happens occationally. Then I will catch the issue using a stopwatch that monitors the time between
 * buffer reads. What you see in this screenshot is just the sensor data, that "sets the scene" for the issue.
 *
 *
 */

typedef struct{
	uint8_t value;
	uint8_t seq_no;
} bufferentry_t;

uint8_t seq_no = 0;

int eth_jobs_for_main_thread = 0;

void ISR_sensor(void);
uint8_t read_sensor(void);
void write_to_buffer(uint8_t value);
uint8_t read_from_buffer(void);
int is_data_in_buffer(void);

TraceStringHandle_t chn_sensor = NULL;
TraceStringHandle_t chn_from_buffer = NULL;
TraceStringHandle_t chn_buf_count = NULL;
TraceStringHandle_t chn_mean = NULL;
TraceStringHandle_t chn_log = NULL;

int task_started = 0;

dfmStopwatch_t* sw = NULL;

void main_superloop(void)
{
	uint32_t sum = 0;
	uint32_t count = 0;

	task_started = 1;

	while (1)
	{
		uint8_t val;

		vDfmStopwatchBegin(sw);

		while (is_data_in_buffer())
		{
			val = read_from_buffer();

			sum += val;
			count++;

			int n = 1000 + (rand() % 200);
			for (volatile int i = 0; i < n; i++);

			if (count == 8)
			{
				uint8_t mean_value = (uint8_t) (sum / count);

				sum = 0;
				count = 0;

				if (chn_mean != NULL)
				{
					xTracePrintF(chn_mean, "%d", mean_value);
				}

			}
		}

		while (eth_jobs_for_main_thread > 0)
		{
			//xTracePrint(chn_log, "Eth job");
			eth_jobs_for_main_thread--;

			int n = 10000;

			if (rand() % 10 == 0)
			{
				n += 5000 + (rand() % 1000);
			}
			else
			{
				n += rand() % 200;
			}
			for (volatile int i = 0; i < n; i++);
		}

		vDfmStopwatchEnd(sw);

		xTraceTaskSwitch((void*) TASK_IDLE, 0);
		HAL_Delay(0);

		xTraceTaskReady(TASK_MAIN); // Makes Tracealyzer split the thread execution into "instances".
		xTraceTaskSwitch((void*) TASK_MAIN, 0);
	}
}



bufferentry_t sensor_data[16];

volatile int8_t write_index = 0;
volatile int8_t read_index = 0;
uint8_t sim_counter = 20;
int8_t incr = 1;

volatile uint32_t nWrites = 0;
volatile uint32_t nReads= 0;


uint8_t read_sensor(void)
{
	int8_t rnd = (rand() % 5) - 2; /* -2, -1, 0, 1, 2 */
	uint8_t reading = sim_counter + rnd;

	sim_counter += incr;

	if (sim_counter == 40)
	{
		incr = -1;
	}

	if (sim_counter == 20)
	{
		incr = 1;
	}

	//printf("read_sensor: %d, %d, %d\n", sim_counter, rnd, reading);

	return reading;
}


void ISR_sensor(void)
{
	if (task_started)
	{
		xTraceISRBegin(xISRHandleTimer);
		write_to_buffer( read_sensor() );
		xTraceISREnd(0);
	}
}

void ISR_other(void)
{
	xTraceISRBegin(xISRHandleOther);

	int n = 1000 + (rand()%2000);
	for (volatile int i = 0; i < n; i++);

	eth_jobs_for_main_thread++;

	xTraceISREnd(0);
}

void demo_systick_handler()
{
	ISR_sensor();
}

volatile int count_max = 0;

void write_to_buffer(uint8_t value)
{

	sensor_data[write_index].value = value;
	sensor_data[write_index].seq_no = seq_no++;

	if (chn_sensor != NULL)
	{
		xTracePrintF(chn_sensor, "%d", sensor_data[write_index].value);
	}

	/*
	nWrites++;
	if (chn_buf_count != NULL)
	{
		int count = nWrites - nReads;

		if (count > count_max)
		{
			xTracePrintF(chn_buf_count, "%d values in buffer",  count);
			count_max = count;
		}
	}*/

	write_index++;
	if (write_index == 16)
	{
		write_index = 0;
	}

}


int is_data_in_buffer(void)
{
	return read_index != write_index;
}

/* Not handling read overrun, assumes caller checks is_data_in_buffer first */
uint8_t read_from_buffer(void)
{
	static uint8_t last_seq_no = 255; /* -1 */
	uint8_t reading;

	reading = sensor_data[read_index].value;

	if (sensor_data[read_index].seq_no != (uint8_t)(last_seq_no + 1))
	{
		printf("Data loss!\n");
	}

	last_seq_no = sensor_data[read_index].seq_no;

	/*
	if (chn_from_buffer != NULL)
	{
		xTracePrintF(chn_from_buffer, "%d", sensor_data[read_index].value);
	}*/

	read_index++;

	/*
	nReads++;

	if (chn_buf_count != NULL)
	{
		int count = nWrites - nReads;

		xTracePrintF(chn_buf_count, "%d values in buffer", count);

		if (count > count_max)
		{
			count_max = count;
		}
	}*/

	if (read_index == 16)
	{
		read_index = 0;
	}

	return reading;
}




#ifdef OLD

void main_superloop2(void)
{
    int counter = 0;
    dfmStopwatch_t* my_stopwatch = xDfmStopwatchCreate("MyStopwatch1", 10);

    if (my_stopwatch == NULL)
    {
    	printf("ERROR, my_stopwatch == NULL\n");
    	return;
    }

    while (1)
    {
    	vDfmStopwatchBegin(my_stopwatch);
    	DemoUpdate(counter);
    	vDfmStopwatchEnd(my_stopwatch);

    	counter++;

        xTraceTaskSwitch( (void*)TASK_IDLE, 0);
    	HAL_Delay(1);
    	xTraceTaskSwitch( (void*)TASK_MAIN, 0);

    }
}


static void DemoUpdate(int counter)
{
	DemoISRUpdate(counter);
   	DemoStatesUpdate(counter);
   	DemoUserEventsUpdate(counter);

   	if (ButtonPressed == 1)
   	{
   		ButtonPressed = 0;
   	    DemoAlert();
   	}
}

static void DemoInit(void)
{
	DemoAlertInit();
    DemoISRInit();
    DemoStatesInit();
    DemoUserEventsInit();
}


#endif

/*
static void DemoUpdate(int counter)
{

	if (ButtonPressed == 1)
   	{
   		ButtonPressed = 0;
   	    DemoAlert();
   	}
}
*/


static void DemoInit(void)
{
	//srand(0); // Seed...

	xTraceStringRegister("Sensor Data", &chn_sensor);
	xTraceStringRegister("Mean Value", &chn_mean);
	xTraceStringRegister("From Buffer", &chn_from_buffer);
	xTraceStringRegister("Buffer Count", &chn_buf_count);
	xTraceStringRegister("Log", &chn_log);

	sw = xDfmStopwatchCreate("Reader", 1100000);
}


int main_baremetal( void )
{
	xTraceISRRegister("TimerISR", 1, &xISRHandleTimer);
	xTraceISRRegister("EthernetISR", 2, &xISRHandleOther);

    /* Just to set a better name for the main thread...*/
    REGISTER_TASK(TASK_MAIN, "main-thread");

    /* Used for tracing the HAL_Delay call in the bare-metal superloop. */
    REGISTER_TASK(TASK_IDLE, "IDLE");

    xTraceTaskReady(TASK_IDLE);
    xTraceTaskReady(TASK_MAIN);

    /* Trace TASK_MAIN as the executing task... */
    xTraceTaskSwitch((void*)TASK_MAIN, 0);

    DemoInit();

    __set_BASEPRI(0);
    __enable_irq();

    main_superloop();

    return 0;
}

void DemoOnButtonPressedBareMetal(void)
{
	ButtonPressed = 1;
}


