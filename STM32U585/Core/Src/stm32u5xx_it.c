/**
  **********************************************************************************************************************
  * @file    stm32u5xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "webserver.h"
#include "stm32u5xx_it.h"

/* Private includes --------------------------------------------------------------------------------------------------*/
/* Private typedef ---------------------------------------------------------------------------------------------------*/
/* Private define ----------------------------------------------------------------------------------------------------*/
/* Private macro -----------------------------------------------------------------------------------------------------*/
/* Private variables -------------------------------------------------------------------------------------------------*/
/* Private function prototypes ---------------------------------------------------------------------------------------*/
/* Private user code -------------------------------------------------------------------------------------------------*/
/* External variables ------------------------------------------------------------------------------------------------*/
extern SPI_HandleTypeDef   Wifi_SPIHandle;
extern LPTIM_HandleTypeDef LPTIMHandle;

/**********************************************************************************************************************/
/*                                Cortex Processor Interruption and Exception Handlers                                */
/**********************************************************************************************************************/

#if (0)
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}
#endif

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
}

/**
  * @brief This function handles System tick timer.
  */

extern volatile int demo_isrs_enabled;

void SysTick_Handler(void)
{

  HAL_IncTick();

  extern void ethernet_ISR_simulator(void);
  extern void ISR_sensor(void);

  if (demo_isrs_enabled == 1)
  {
  	  ethernet_ISR_simulator();
  	  ISR_sensor();
  }

}

/**********************************************************************************************************************/
/* STM32U5xx Peripheral Interrupt Handlers                                                                            */
/* Add here the Interrupt Handlers for the used peripherals.                                                          */
/* For the available peripheral interrupt handler names,                                                              */
/* please refer to the startup file (startup_stm32u5xx.s).                                                            */
/**********************************************************************************************************************/

/**
  * @brief This function handles SPI2 global interrupt.
  */
void SPI2_IRQHandler(void)
{
}

/**
  * @brief This function handles EXTI Line13 interrupt.
  */
void EXTI13_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(BUTTON_USER_PIN);
}

/**
  * @brief This function handles EXTI Line15 interrupt.
  */
void EXTI15_IRQHandler(void)
{
}

/**
  * @brief This function handles LPTIM1 global interrupt.
  */
void LPTIM1_IRQHandler(void)
{
}
