
#include <stdio.h>
#include <stdlib.h>
#include "dfmStoragePort.h"
#include "main.h"
#include "stdint.h"
#include "stdarg.h"

#include "trcRecorder.h"
#include "dfm.h"
#include "dfmCrashCatcher.h"

#include "demo_alert.h"
#include "demo_states.h"
#include "demo_user_events.h"
#include "demo_isr.h"


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
 * Calling xTraceTaskSwitch and REGISTER_TASK manually like this is only needed
 * for bare-metal systems. When using a supported RTOS such as FreeRTOS, all
 * kernel tasks are traced automatically.
 *
 * Other alternatives for more detailed TraceRecorder tracing includes:
 *  - User events (see TraceRecorder/include/trcPrint.h)
 *  - State Machines (see TraceRecorder/include/trcStateMachine.h)
 *  - Runnables (see TraceRecorder/include/trcRunnable.h)
 *  - Interval Sets (see TraceRecorder/include/trcInterval.h)
 *
 *****************************************************************************/
#define REGISTER_TASK(ID, name) xTraceTaskRegisterWithoutHandle((void*)ID, name, 0)

// Handles for baremetal "tasks" (see above)
#define TASK_IDLE 100
#define TASK_MAIN 101

// Handles for tracing ISRs
TraceISRHandle_t xISRHandleTimer = 0;
TraceISRHandle_t xISRHandleOther = 0;

// Handles for state machine tracing (used for main thread)
TraceStateMachineHandle_t reader_jobs;
TraceStateMachineStateHandle_t reader_job_netw_msg;
TraceStateMachineStateHandle_t reader_job_sens_data;
TraceStateMachineStateHandle_t reader_idle;

// Handles for User Event channels
TraceStringHandle_t chn_sensor = NULL;
TraceStringHandle_t chn_from_buffer = NULL;
TraceStringHandle_t chn_buf_count = NULL;
TraceStringHandle_t chn_mean = NULL;
TraceStringHandle_t chn_log = NULL;

// Stopwatch
dfmStopwatch_t* sw = NULL;


// Demo app internals...
void ethernet_isr_handler(void);
void ISR_sensor(void);
uint8_t read_sensor(void);
void write_to_buffer(uint8_t value);
uint8_t read_from_buffer(void);
int is_data_in_buffer(void);
void print_demo_commands(void);
void run_demo_command(char* buf);
static void DemoInit(void);

extern int kbhit(void);
extern int get_string(char *buf);

// A circular buffer where ISR_sensor writes its data. Read by main thread.
typedef struct{
	uint8_t value;
	uint8_t seq_no;
} bufferentry_t;

bufferentry_t sensor_data[16];

volatile int8_t write_index = 0;
volatile int8_t read_index = 0;
uint8_t seq_no = 0;

// Simulates network jobs.
int eth_jobs_for_main_thread = 0;

// For generating the simulated sensor data (a triangular wave with noise added)
uint8_t sim_counter = 20;
int8_t incr = 1;

// Demo control variables
volatile int demo_interrupt_rate = 16;
volatile int demo_isrs_enabled = 0;
int poll = 0;

uint32_t demo_time = 0;
uint32_t command_time = 0;

#define DEMO_CMD_MAX_LEN 32
char demo_command[DEMO_CMD_MAX_LEN];

/* Wrapper for printf, pausing the ISR inputs to avoid data loss in the sensor_data queue
 * during the printf calls. This is not representative for real applications. */
#define DEMO_PRINTF(...) \
	{ \
	demo_isrs_enabled = 0; \
	printf(__VA_ARGS__); \
	demo_isrs_enabled = 1; \
	}

void main_superloop(void)
{
	uint32_t sum = 0;
	uint32_t count = 0;

	uint32_t stopwatch_alert_count = 0;

	print_demo_commands();

	demo_isrs_enabled = 1;

	xTraceStateMachineSetState(reader_jobs, reader_idle);

	// Creates a new Detect stopwatch for this thread.
	sw = xDfmStopwatchCreate("Reader", 500000);

	while (1)
	{
		uint8_t val;

		stopwatch_alert_count = sw->times_above;

		// Detect stopwatch, starts monitoring the runtime of this thread.
		vDfmStopwatchBegin(sw);

		while (is_data_in_buffer())
		{
			xTraceStateMachineSetState(reader_jobs, reader_job_sens_data);

			val = read_from_buffer();

			xTracePrintF(chn_log, "Sensor data (%d), sample %d/8", val, count+1);

			sum += val;
			count++;

			// Simulate longer execution time
			int n = 3000 + (rand() % 100);
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
			xTraceStateMachineSetState(reader_jobs, reader_job_netw_msg);

			xTracePrint(chn_log, "Network request");
			eth_jobs_for_main_thread--;

			// Simulate longer execution time
			int n = 5000;
			if (rand() % 10 == 0)
			{
				n += 8000 + (rand() % 2000);
			}
			else
			{
				n += rand() % 200;
			}
			for (volatile int i = 0; i < n; i++);
		}

		xTraceStateMachineSetState(reader_jobs, reader_idle);

		// Checks the elapsed time since vDfmStopwatchStart, output trace if over threshold.
		vDfmStopwatchEnd(sw);

		// For clearer demo, avoid multiple stopwatch alerts in series.
		if (sw->times_above > stopwatch_alert_count)
		{
			demo_interrupt_rate = 16; // Default load (not causing stopwatch alerts)
			DEMO_PRINTF("\nRestored default interrupt probability (1/16)\n");
		}

		// Command from the serial interface are buffered and executed here.
		if ((demo_time == command_time) && (command_time > 0))
		{
			run_demo_command(demo_command);
		}

		xTraceTaskSwitch((void*) TASK_IDLE, 0);

		HAL_Delay(0);

		demo_time++;

		if ((demo_time % 5000 == 0) && (poll == 1))
		{
			DEMO_PRINTF("\nInterrupt probability: 1/%d - SW exp_max: %u, times_above: %u, high_watermark: %u\n", demo_interrupt_rate, (unsigned int)sw->expected_duration, (unsigned int)sw->times_above, (unsigned int)sw->high_watermark);
		}

		// If a key is pressed, read a string (command) from the STLINK VCOM port (reads until newline, blocking)
		if (kbhit())
		{
				get_string(demo_command);
				DEMO_PRINTF("\n");

				// Demo commands are executed during the "busy period", for more realistic traces.
				command_time = demo_time + 800;
		}

		// Makes Tracealyzer show the next job as a new "instance".
		xTraceTaskReady(TASK_MAIN);

		xTraceTaskSwitch((void*) TASK_MAIN, 0);
	}
}

// Called sporadically, how often depend on "load" parameter (demo_interrupt_rate)
void ethernet_isr_handler(void)
{
		xTraceISRBegin(xISRHandleOther);

		// Simulate longer execution time
		int n = 1000 + (rand()%2000);
		for (volatile int i = 0; i < n; i++);

		eth_jobs_for_main_thread++;

		xTraceISREnd(0);
}

// Called by ISR_sensor, simulates reading a sensor.
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

	return reading;
}

// Called by ISR_sensor, writes data to buffer.
void write_to_buffer(uint8_t value)
{
	sensor_data[write_index].value = value;
	sensor_data[write_index].seq_no = seq_no++;

	if (chn_sensor != NULL)
	{
		xTracePrintF(chn_sensor, "%d", sensor_data[write_index].value);
	}

	write_index++;
	if (write_index == 16)
	{
		write_index = 0;
	}
}

// Called periodically every millisecond
void ISR_sensor(void)
{
	xTraceISRBegin(xISRHandleTimer);
	write_to_buffer( read_sensor() );
	xTraceISREnd(0);
}

// Called by main thread, checks if data available in the buffer.
int is_data_in_buffer(void)
{
	return read_index != write_index;
}

// Called by main thread, reads data from buffer
uint8_t read_from_buffer(void)
{
	static uint8_t last_seq_no = 255; /* -1 */
	uint8_t reading;

	reading = sensor_data[read_index].value;

	if (sensor_data[read_index].seq_no != (uint8_t)(last_seq_no + 1))
	{
		// Main thread has not been able to read all data in time.
		// Logging this to TraceRecorder. Another option is to use DFM_TRAP to trigger an alert.
		xTracePrint(chn_log, "Data loss!");
	}

	last_seq_no = sensor_data[read_index].seq_no;

	read_index++;

	if (read_index == 16)
	{
		read_index = 0;
	}

	return reading;
}

// Registers TraceRecorder objects and DFM Stopwatch
static void DemoInit(void)
{
	xTraceISRRegister("TimerISR", 1, &xISRHandleTimer);
	xTraceISRRegister("EthernetISR", 2, &xISRHandleOther);

	xTraceStringRegister("Sensor Data", &chn_sensor);
	xTraceStringRegister("Mean Value", &chn_mean);
	xTraceStringRegister("From Buffer", &chn_from_buffer);
	xTraceStringRegister("Buffer Count", &chn_buf_count);
	xTraceStringRegister("Job", &chn_log);

	xTraceStateMachineCreate("Reader Jobs", &reader_jobs);
	xTraceStateMachineStateCreate(reader_jobs, "Job: Network msg", &reader_job_netw_msg);
	xTraceStateMachineStateCreate(reader_jobs, "Job: Sensor data", &reader_job_sens_data);
	xTraceStateMachineStateCreate(reader_jobs, "IDLE", &reader_idle);
}

int main_baremetal( void )
{

    /* Just to set a better name for the main thread...*/
    REGISTER_TASK(TASK_MAIN, "main-thread");

    /* Used for tracing the HAL_Delay call in the bare-metal superloop. */
    REGISTER_TASK(TASK_IDLE, "IDLE");

    // Perhaps not needed.
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

void print_demo_commands(void)
{
	DEMO_PRINTF("\n Demo commands:"
			   	"\n  test 1: Failed ASSERT() with restart."
				"\n  test 2: DFM_TRAP() without restart"
				"\n  test 3: Hard fault exception"
				"\n  test 4: Buffer overrun, stack corruption"
			    "\n  test 5: Increase interrupt rate to trigger a stopwatch alert."
			    "\n  print: show all stopwatches"
			    "\n  poll on: prints stopwatch 0 every 5 s"
			    "\n  poll off: stops printing every 5 s"
			    "\n  clear: resets stopwatch 0\n\n");
}

void run_demo_command(char* buf)
{
	if (strncmp(buf, "print", 5) == 0)
	{
		DEMO_PRINTF("\n");
		vDfmStopwatchPrintAll();
		DEMO_PRINTF("\n");
	}else if (strncmp(buf, "poll on", 7) == 0)
	{
		poll = 1;
		DEMO_PRINTF("\nPrinting status every 5 s...\n");
	}
	else if (strncmp(buf, "poll off", 8) == 0)
	{
		poll = 0;
		DEMO_PRINTF("\nSilent now...\n");
	}
	else if (strncmp(buf, "clear", 5) == 0)
	{
		sw->high_watermark = 0;
		sw->times_above = 0;
		DEMO_PRINTF("\nStopwatch 0 cleared\n");
	}
	else if (strncmp(buf, "test 1", 6) == 0)
	{
		demo_isrs_enabled = 0;
		testAssertFailed(NULL);
		demo_isrs_enabled = 1;

	}
	else if (strncmp(buf, "test 2", 6) == 0)
	{
		demo_isrs_enabled = 0;
		testCoreDumpNoRestart(42);
		demo_isrs_enabled = 1;
	}
	else if (strncmp(buf, "test 3", 6) == 0)
	{
		demo_isrs_enabled = 0;
		dosomething(32);
		demo_isrs_enabled = 1;
	}
	else if (strncmp(buf, "test 4", 6) == 0)
	{
		demo_isrs_enabled = 0;
		testBufferOverrun();
		demo_isrs_enabled = 1;
	}
	else if (strncmp(buf, "test 5", 6) == 0)
	{
		DEMO_PRINTF("\nInterrupt probability now 1/4. This should soon trigger a stopwatch alert. Please wait...\n");
		demo_interrupt_rate = 4;
	}
	else
	{
		print_demo_commands();
	}
}


void ethernet_ISR_simulator(void)
{
	if ( rand() % demo_interrupt_rate == 0)
	{
		ethernet_isr_handler();
	}
}
