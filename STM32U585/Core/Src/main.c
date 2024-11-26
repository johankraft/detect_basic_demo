/**
  **********************************************************************************************************************
  * @file    main.c
  * @author  MCD Application Team
  * @brief   Main program body
  **********************************************************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  **********************************************************************************************************************
  */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "main.h"
#include "trcRecorder.h"
#include "dfm.h"

/* Private typedef ---------------------------------------------------------------------------------------------------*/
/* Private define ----------------------------------------------------------------------------------------------------*/
/* Private macro -----------------------------------------------------------------------------------------------------*/
/* Private variables -------------------------------------------------------------------------------------------------*/
/* Private function prototypes ---------------------------------------------------------------------------------------*/
extern int main_baremetal( void );

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void) {
	/* STM32U5xx HAL library initialization:
	 - Configure the Flash prefetch
	 - Configure the Systick to generate an interrupt each 1 msec
	 - Set NVIC Group Priority to 3
	 - Low Level Initialization
	 */
	vTraceInitialize();


	HAL_Init();

	/* Configure the System clock to have a frequency of 120 MHz */
	system_clock_config();

	/* Use systick as time base source and configure 1ms tick (default clock after Reset is HSI) */
	HAL_InitTick(TICK_INT_PRIORITY);

	/* Enable the Instruction Cache */
	instruction_cache_enable();

	/* Initialize bsp resources */
	bsp_init();

	/* No buffer for printf usage, just print characters one by one.*/
	setbuf(stdout, NULL);

	/* Initialize web server system */
	if (system_init() != 0)
	{
		process_error();
	}

	/* Initialize Percepio TraceRecorder (stores events to ring buffer) */
	xTraceEnable(TRC_START);

	/* Initialize the Percepio DFM library for creating Alerts. */
	if (xDfmInitializeForLocalUse() == DFM_FAIL)
	{
		configPRINTF(("Failed to initialize DFM\r\n"));
	}

	printf("STM32U585 demo platform\nNote: Timer ISR enabled -> LPTIM_UpdateEventCallback\n");

	main_baremetal();

	while (1) {
	}
}

