/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - DOS-RTOS final project (STM32L475VG)
  * @author         : __________
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  *
  * @note This file is based on the STM32CubeMX-generated skeleton. Everything
  *       inside the USER CODE sections, as well as the clock, GPIO and USART
  *       configuration below, was written by us; the surrounding structure and
  *       the HAL calls come from the code generator.
  *
  * Start-up order in main(), which matters:
  *   1. HAL + clock          - SysTick starts running here already
  *   2. Debug/sleep unlock   - so SWD survives the idle task's __WFI()
  *   3. Peripherals          - GPIO, USART
  *   4. Tracing              - before any RTOS code, so nothing is missed
  *   5. RTOS objects + tasks - queues/mutexes/semaphores, then the stacks
  *   6. Drivers              - UART RX interrupt, HC-SR04
  *   7. Interrupt priorities - SysTick highest, PendSV lowest
  *   8. Scheduler_vInit()    - from here on the SysTick actually schedules
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SEGGER_SYSVIEW.h"
#include "board_config.h"
#include "os_trace.h"
#include "app_resources.h"
#include "scheduler.h"
#include "stack.h"
#include "tasks.h"
#include "uart_driver.h"
#include "hcsr04.h"
#include "tests.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
UART_HandleTypeDef huart1;   /* expected by board_config.h as APP_UART_HANDLE */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void MX_USART1_UART_Init(void);
/* USER CODE END PFP */

/**
  * @brief  The application entry point.
  * @retval int (never returns - main becomes the idle task, see below)
  * @author __________
  */
int main(void)
{
  /* From HAL_Init() onwards the SysTick runs (1 ms). The scheduler ignores
   * those ticks until Scheduler_vInit() (boot guard in scheduler.c). */
  HAL_Init();
  SystemClock_Config();

  /* STM32L4 specific: the idle task calls __WFI() (sleep until the next
   * interrupt). Without the DBGMCU enables below, the debugger/SWD - and
   * therefore SEGGER RTT/SystemView - loses access to the core as soon as it
   * enters sleep mode. The symptom is a "Could not find RTT Control Block"
   * timeout, with the program stuck on the failed debug halt. */
  HAL_DBGMCU_EnableDBGSleepMode();
  HAL_DBGMCU_EnableDBGStopMode();
  HAL_DBGMCU_EnableDBGStandbyMode();

  MX_GPIO_Init();               /* TRIG as output, ECHO as EXTI input       */

  /* USER CODE BEGIN 2 */
  MX_USART1_UART_Init();        /* PB6/PB7 -> ST-LINK VCP, 115200 8N1       */

  /* --- Tracing (must come before any other RTOS code!) ------------------ */
  SEGGER_SYSVIEW_Conf();        /* set up SystemView over RTT               */
  OS_Trace_Init();              /* register our event module + task info    */

  /* --- RTOS objects and tasks ------------------------------------------- */
#if (OS_RUN_INTEGRATION_TESTS != 0)
  /* Test mode: mutex/semaphore/queue integration tests instead of the
   * distance measurement. Switch this in os_trace_config.h. */
  Tests_vInitResources();
  Tasks_vInitTaskArray();
  Stack_vInit(&tasks[0], TestHighTask);
  Stack_vInit(&tasks[1], TestMainTask);
  Stack_vInit(&tasks[2], TestPeerTask);
  Stack_vInit(&tasks[3], IdleTask);
#else
  App_Resources_Init();         /* queues, mutexes, semaphores, g_userConfig*/
  Tasks_vInitTaskArray();       /* id/prio/state of the 4 tasks             */
  Stack_vInit(&tasks[0], SensorTask);
  Stack_vInit(&tasks[1], ProcTask);
  Stack_vInit(&tasks[2], UartShellTask);
  Stack_vInit(&tasks[3], IdleTask);   /* stack is never used, see below     */
#endif

  /* --- Drivers ----------------------------------------------------------- */
  UART_Init();                  /* arm the RX interrupt (after MX_USART1!)  */
#if (OS_RUN_INTEGRATION_TESTS == 0)
  HCSR04_vInit();               /* DWT cycle counter for the pulse timing   */
#endif

  /* --- Interrupt priorities (higher number = lower priority) ------------
   *  SysTick 0  : makes the scheduling decision, nests inside all ISRs
   *  EXTI0   5  : echo edges (set in MX_GPIO_Init)
   *  USART1  6  : shell input (set in HAL_UART_MspInit)
   *  PendSV  15 : context switch ALWAYS last, after every other ISR        */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
  HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

  /* --- Start the scheduler ----------------------------------------------- */
  Scheduler_vInit();

  /* On the first context switch, PendSV saves THIS context (main) into the
   * TCB of tasks[3] - from then on main IS the idle task. That is why we drop
   * straight into the idle loop here (which is also why the stack prepared
   * for tasks[3] above is never actually used): */
  IdleTask();
  /* USER CODE END 2 */

  while (1)
  {
    /* never reached - IdleTask() does not return */
  }
}

/**
  * @brief System clock configuration: MSI 4 MHz -> PLL -> 80 MHz.
  * @author __________
  *
  * 80 MHz derived from the internal MSI (4 MHz) through the PLL (N=40, R=2).
  * The L475 board has no HSE crystal on the MCU main clock, hence MSI as the
  * PLL source. AHB/APB1/APB2 all run at 80 MHz.
  *
  * Voltage scale 1 and flash latency 4 are mandatory for 80 MHz.
  *
  * @note The HC-SR04 driver derives its timing from SystemCoreClock, so it
  *       adapts to this setting automatically.
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;        /* 4 MHz */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;              /* -> 80 MHz */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO initialisation, including the HC-SR04 pins.
  * @author __________
  *
  * The HC-SR04 pins (TRIG as output, ECHO as EXTI on both edges) are
  * configured inside the USER CODE block so they survive a regeneration from
  * the .ioc file.
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* --- HC-SR04: TRIG (output, initially low) ---------------------------- */
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = HCSR04_TRIG_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HCSR04_TRIG_PORT, &GPIO_InitStruct);

  /* --- HC-SR04: ECHO (EXTI, BOTH edges, pull-down) ---------------------- */
  GPIO_InitStruct.Pin   = HCSR04_ECHO_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  HAL_GPIO_Init(HCSR04_ECHO_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief Initialise USART1 for the shell (115200 8N1 on PB6/PB7).
  * @author __________
  *
  * PB6/PB7 are hard-wired to the ST-LINK virtual COM port on the
  * B-L475E-IOT01A, so no external adapter is needed.
  *
  * @warning As soon as USART1 is enabled in the .ioc and the project is
  *          regenerated, CubeMX emits MX_USART1_UART_Init() in main.c and
  *          HAL_UART_MspInit() in stm32l4xx_hal_msp.c itself - THESE two
  *          functions must then be deleted here to avoid duplicate symbols.
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 115200;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Low-level UART setup: clocks, pins and NVIC.
  * @param huart UART handle being initialised.
  * @author __________
  *
  * Called by HAL_UART_Init(). See the warning on MX_USART1_UART_Init() about
  * regenerating from the .ioc file.
  */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if (huart->Instance == USART1)
  {
    /* USART1 kernel clock: PCLK2 (80 MHz) */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;   /* PB6 TX, PB7 RX */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
}
/* USER CODE END 4 */

/**
  * @brief  Error handler: disable interrupts and halt.
  * @author __________
  *
  * Executed whenever a HAL initialisation call fails. Deliberately a hard stop
  * rather than a retry - a failed clock or peripheral setup leaves the system
  * in a state where continuing would produce misleading measurements.
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief Reports the name of the source file and the line number where an
  *        assert_param error has occurred.
  * @param file Pointer to the source file name.
  * @param line assert_param error line source number.
  * @author __________
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
