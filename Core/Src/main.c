/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - DOS-RTOS Endprojekt (STM32L475VG)
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
UART_HandleTypeDef huart1;   /* von board_config.h als APP_UART_HANDLE erwartet */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void MX_USART1_UART_Init(void);
/* USER CODE END PFP */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Ab HAL_Init() laeuft der SysTick (1ms). Der Scheduler ignoriert Ticks
   * bis Scheduler_vInit() (Boot-Guard in scheduler.c). */
  HAL_Init();
  SystemClock_Config();

  /* STM32L4-Spezifikum: der Idle-Task ruft __WFI() auf (Sleep bis zum
   * naechsten Interrupt). Ohne die folgenden DBGMCU-Freigaben verliert
   * der Debugger/SWD (und damit SEGGER RTT/SystemView!) den Zugriff auf
   * den Kern, sobald dieser im Sleep-Modus haengt - das aeussert sich
   * als "Could not find RTT Control Block" mit Timeout, und das
   * Programm bleibt beim fehlgeschlagenen Debug-Halt haengen. */
  HAL_DBGMCU_EnableDBGSleepMode();
  HAL_DBGMCU_EnableDBGStopMode();
  HAL_DBGMCU_EnableDBGStandbyMode();

  MX_GPIO_Init();               /* PB4 = TRIG (Out), PB5 = ECHO (EXTI)      */

  /* USER CODE BEGIN 2 */
  MX_USART1_UART_Init();        /* PB6/PB7 -> ST-LINK VCP, 115200 8N1       */

  /* --- Tracing (vor allem anderen RTOS-Code!) --------------------------- */
  SEGGER_SYSVIEW_Conf();        /* SystemView-RTT einrichten                */
  OS_Trace_Init();              /* eigenes Event-Modul + Task-Infos melden  */

  /* --- RTOS-Objekte und Tasks ------------------------------------------- */
#if (OS_RUN_INTEGRATION_TESTS != 0)
  /* Testmodus: Mutex-/Semaphore-/Queue-Integrationstests statt der
   * Distanzmessung. Umschalten in os_trace_config.h. */
  Tests_vInitResources();
  Tasks_vInitTaskArray();
  Stack_vInit(&tasks[0], TestHighTask);
  Stack_vInit(&tasks[1], TestMainTask);
  Stack_vInit(&tasks[2], TestPeerTask);
  Stack_vInit(&tasks[3], IdleTask);
#else
  App_Resources_Init();         /* Queues, Mutexe, Semaphoren, g_userConfig */
  Tasks_vInitTaskArray();       /* Id/Prio/State der 4 Tasks                */
  Stack_vInit(&tasks[0], SensorTask);
  Stack_vInit(&tasks[1], ProcTask);
  Stack_vInit(&tasks[2], UartShellTask);
  Stack_vInit(&tasks[3], IdleTask);   /* Stack wird nie genutzt, s.u.       */
#endif

  /* --- Treiber ----------------------------------------------------------- */
  UART_Init();                  /* RX-Interrupt scharf (nach MX_USART1!)    */
#if (OS_RUN_INTEGRATION_TESTS == 0)
  HCSR04_vInit();               /* DWT-Zykluszaehler fuer die Pulsmessung   */
#endif

  /* --- Interrupt-Prioritaeten (Zahl groesser = niedriger) ---------------
   *  SysTick 0  : trifft die Scheduling-Entscheidung, nistet in allen ISRs
   *  EXTI0   5  : Echo-Flanken (in MX_GPIO_Init gesetzt)
   *  USART1  6  : Shell-Eingabe (in HAL_UART_MspInit gesetzt)
   *  PendSV  15 : Kontextwechsel IMMER als letztes, nach allen ISRs        */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
  HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

  /* --- Scheduler starten ------------------------------------------------- */
  Scheduler_vInit();

  /* Beim ersten Kontextwechsel sichert PendSV DIESEN Kontext (main) in
   * den TCB von tasks[3] - main IST danach der Idle-Task. Deshalb geht
   * es hier direkt in die Idle-Schleife (WFI + SystemView-OnIdle): */
  IdleTask();
  /* USER CODE END 2 */

  while (1)
  {
    /* nie erreicht - IdleTask() kehrt nicht zurueck */
  }
}

/**
  * @brief System Clock Configuration
  *
  * 80 MHz aus dem internen MSI (4 MHz) * PLL (N=40, R=2). Das L475-Board
  * hat keinen HSE-Quarz am MCU-Haupttakt. AHB/APB1/APB2 = 80 MHz.
  * Der HC-SR04-Treiber rechnet ueber SystemCoreClock - passt automatisch.
  */
/**
 * @brief Systemtakt konfigurieren: MSI 4 MHz -> PLL -> 80 MHz.
 *
 * Das L475-Board hat keinen HSE-Quarz am Haupttakt, daher MSI als
 * PLL-Quelle. Voltage Scale 1 und Flash-Latency 4 sind fuer 80 MHz
 * zwingend.
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
  * @brief GPIO Initialization Function
  */
/**
 * @brief GPIO-Initialisierung inkl. HC-SR04-Pins.
 *
 * Die HC-SR04-Pins (TRIG als Ausgang, ECHO als EXTI mit beiden Flanken)
 * werden im USER-CODE-Block gesetzt, damit sie eine .ioc-Neugenerierung
 * ueberleben.
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
  /* --- HC-SR04: PB4 = TRIG (Ausgang, initial Low) ----------------------- */
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = HCSR04_TRIG_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HCSR04_TRIG_PORT, &GPIO_InitStruct);

  /* --- HC-SR04: PB5 = ECHO (EXTI, BEIDE Flanken, Pull-Down) ------------- */
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
  * @brief USART1 Initialization Function (115200 8N1 auf PB6/PB7 = ST-LINK VCP)
  *
  * HINWEIS: Sobald ihr USART1 im .ioc aktiviert und neu generiert,
  * erzeugt CubeMX MX_USART1_UART_Init in der main.c und
  * HAL_UART_MspInit in stm32l4xx_hal_msp.c selbst - dann DIESE beiden
  * Funktionen hier loeschen (sonst doppelte Symbole).
  */
/**
 * @brief USART1 fuer die Shell initialisieren (115200 8N1, PB6/PB7).
 *
 * PB6/PB7 sind auf dem B-L475E-IOT01A fest mit dem ST-LINK Virtual COM
 * Port verbunden.
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

/* Wird von HAL_UART_Init() aufgerufen. */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if (huart->Instance == USART1)
  {
    /* USART1-Kernel-Takt: PCLK2 (80 MHz) */
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
  * @brief  This function is executed in case of error occurrence.
  */
/**
 * @brief Fehlerbehandlung: Interrupts sperren und anhalten.
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
