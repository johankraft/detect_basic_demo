/**
  ******************************************************************************
  * @file    BSP/Src/main.c 
  * @author  MCD Application Team
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "dfm.h"
#include "dfmCrashCatcher.h"

#include "FreeRTOS.h"
#include "task.h"

#include "trcRecorder.h"

/** @addtogroup STM32L4xx_HAL_Examples
  * @{
  */

/** @addtogroup BSP
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef hDiscoUart;
#ifdef __GNUC__
/* With GCC/RAISONANCE, small msg_info (option LD Linker->Libraries->Small msg_info
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define GETCHAR_PROTOTYPE int __io_getchar(void)
#else
//#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#define GETCHAR_PROTOTYPE int fgetc(FILE *f)
#endif /* __GNUC__ */

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);

void vTaskDemoDriver(void *pvParameters);

/* Private functions ---------------------------------------------------------*/


/* Note: For ITM logging to file on STLINK v2, reduce SWO speed to 500 KHz to
  avoid issues. The default 2000 KHz does not seem reliable. */

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{

  /* STM32L4xx HAL library initialization:
       - Configure the Flash prefetch, Flash preread and Buffer caches
       - Systick timer is configured by default as source of time base, but user 
             can eventually implement his proper time base source (a general purpose 
             timer for example or other time source), keeping in mind that Time base 
             duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and 
             handled in milliseconds basis.
       - Low Level Initialization
     */
  HAL_Init();

  /* Configure the System clock to have a frequency of 80 MHz */
  SystemClock_Config();


  /* Configure the User LED */
  BSP_LED_Init(LED2); 
  
  /* Configure the User Button in GPIO Mode */
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);
  
  /* Initialize all configured peripherals */
  hDiscoUart.Instance = DISCOVERY_COM1; 
  hDiscoUart.Init.BaudRate = 115200;
  hDiscoUart.Init.WordLength = UART_WORDLENGTH_8B;
  hDiscoUart.Init.StopBits = UART_STOPBITS_1;
  hDiscoUart.Init.Parity = UART_PARITY_NONE;
  hDiscoUart.Init.Mode = UART_MODE_TX_RX;
  hDiscoUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hDiscoUart.Init.OverSampling = UART_OVERSAMPLING_16;
  hDiscoUart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hDiscoUart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  BSP_COM_Init(COM1, &hDiscoUart);
  
  int32_t bytesWritten;
  extern traceResult prvTraceItmWrite(void* ptrData, uint32_t size, int32_t* ptrBytesWritten);
    
  printf("Initializing TraceRecorder: ");
  if (xTraceEnable(TRC_START) == TRC_FAIL)
  {
    printf("FAIL\n");
  }
  else
  {
    printf("OK\n");
  }
  
  printf("Initializing DFM: ");
  if (xDfmInitializeForLocalUse() == DFM_FAIL)
  {
      printf("FAIL\n");
  }
  else
  {
    printf("OK\n");
  }
  
  xTaskCreate(
      vTaskDemoDriver,             
      "DemoDriver",            
      configMINIMAL_STACK_SIZE*4,
      NULL,               // Parameter
      tskIDLE_PRIORITY + 1,
      NULL                // Task handle
  );
    
  printf("Starting FreeRTOS\n");
  vTaskStartScheduler();
  
  /* Infinite loop */
  while (1)
  {
    /*QSPI_demo();
    QSPI_MemoryMapped_demo();    
    Temperature_Test();
    Humidity_Test();
    Pressure_Test();
    Magneto_Test();
    Gyro_Test();
    Accelero_Test();*/
  }
}

extern void demo_data_logging(void);
extern void demo_alert(void);

void vTaskDemoDriver(void *pvParameters)
{
    (void) pvParameters;
    
    for (;;)
    {
        printf("\nPercepio demo (re)starting\n\n");
        
        demo_data_logging();
        
        demo_alert();
              
        vTaskDelay(pdMS_TO_TICKS(1000));  // delay 1 second      
    }
}




static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* MSI is enabled after System reset, activate PLL with MSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLP = 7;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}

/**
  * @brief  Gets numeric values from the Hyperterminal.
  * @param  Maximum value allowed (value)
  * @retval The character received
  */
uint32_t Serial_Scanf(uint32_t value)
{
  uint16_t tmp = 0;
  
  tmp = getchar();
  
  if (tmp > value)
  {
    printf("\n\r  !!! Please enter valid number between 0 and %lu \n", value);
    return 0xFF;
  }
  return tmp;
}

#if (0)

/**
  * @brief Retargets the C library msg_info function to the USART.
  * @param None
  * @retval None
  */

PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the serial port and Loop until the end of transmission */
  while (HAL_OK != HAL_UART_Transmit(&hDiscoUart, (uint8_t *) &ch, 1, 30000))
  {
    ;
  }
  return ch;
}

#endif

/**
  * @brief Retargets the C library scanf function to the USART.
  * @param None
  * @retval None
  */
GETCHAR_PROTOTYPE
{
  /* Place your implementation of fgetc here */
  /* e.g. readwrite a character to the USART2 and Loop until the end of transmission */
  uint8_t ch = 0;
  while (HAL_OK != HAL_UART_Receive(&hDiscoUart, (uint8_t *)&ch, 1, 30000))
  {
    ;
  }
  return ch;
}

#include <yfuns.h>     // Required for __write declaration
#include "stm32l4xx_hal.h"

#if (0)
size_t __write(int handle, const unsigned char *buf, size_t size)
{
    // Only support stdout (handle 1) and stderr (handle 2)
    if (handle != _LLIO_STDOUT && handle != _LLIO_STDERR)
        return _LLIO_ERROR;

    HAL_UART_Transmit(&hDiscoUart, (uint8_t *)buf, size, HAL_MAX_DELAY);
    return size;
}

#endif

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack overflow in task: %s\n", pcTaskName);

    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void)
{    
    printf("Malloc failed!\n");

    taskDISABLE_INTERRUPTS();
    for (;;);
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{  
  /* User can add his own implementation to report the HAL error return state */
  printf("!!! ERROR !!!\n");
  while(1) 
  {
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */ 

/**
  * @}
  */ 

