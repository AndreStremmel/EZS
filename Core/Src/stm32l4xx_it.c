/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l4xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32l4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "scheduler.h"
#include "board_config.h"
#include "hcsr04.h"
#include "SEGGER_SYSVIEW.h"
#include "os_trace_config.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/* HINWEIS: PendSV_Handler ist ABSICHTLICH NICHT hier definiert - er
 * kommt als Assembler-Routine aus pendsv.s (Kontextwechsel). In CubeMX
 * unter System Core -> NVIC -> Code generation das Haekchen
 * "Generate IRQ handler" fuer "Pendable request" ENTFERNEN, sonst
 * erzeugt der Generator ein Duplikat (Linker-Fehler).
 */


/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
  OS_TRACE_ISR_ENTER();
  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  Scheduler_vCountdown();          // Non-Blocked-Delays & Timeouts
  Scheduler_pGetNextTask();        // naechsten Task waehlen (setzt g_pNextTask)

  if (g_pNextTask != g_pCurrentTask)
  {
      // Direkter Write statt Read-Modify-Write: PENDSVSET ist
      // write-1-to-set, alle anderen Bits ignorieren eine 0.
      SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;    // PendSV anstossen
  }

  OS_TRACE_ISR_EXIT();
  /* USER CODE END SysTick_IRQn 1 */
}


/******************************************************************************/
/* STM32F3xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f3xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */
/* --------------------------------------------------------------------------
 * HC-SR04: ECHO-Flanken-Interrupt (PB0 -> EXTI0)
 * Die ISR misst nur Zeitstempel und gibt am Pulsende die Semaphore frei -
 * sie kehrt danach zum unterbrochenen Task zurueck (kein Scheduling!).
 * -------------------------------------------------------------------------- */
void EXTI0_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_IRQn 0 */
  OS_TRACE_ISR_ENTER();
  /* USER CODE END EXTI0_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(HCSR04_ECHO_PIN);
  /* USER CODE BEGIN EXTI0_IRQn 1 */
  OS_TRACE_ISR_EXIT();
  /* USER CODE END EXTI0_IRQn 1 */
}

/* In einen USER-CODE-Block (z.B. in main.c oder hier) - HAL ruft diesen
 * Callback aus HAL_GPIO_EXTI_IRQHandler heraus auf: */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == HCSR04_ECHO_PIN)
  {
      HCSR04_vEchoEdgeIsr();
  }
}

/* --------------------------------------------------------------------------
 * UART: RX-Interrupt (Zeichen -> Ringpuffer, bei Zeilenende Semaphore-Give
 * aus HAL_UART_RxCpltCallback in uart_driver.c)
 * -------------------------------------------------------------------------- */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  OS_TRACE_ISR_ENTER();
  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&APP_UART_HANDLE);
  /* USER CODE BEGIN USART1_IRQn 1 */
  OS_TRACE_ISR_EXIT();
  /* USER CODE END USART1_IRQn 1 */
}

/* USER CODE END 1 */
