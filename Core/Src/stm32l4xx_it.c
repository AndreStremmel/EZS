/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l4xx_it.c
  * @brief   Interrupt Service Routines.
  * @author  Berkay (USER CODE sections only)
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
  * @note This file is STM32CubeMX-generated. Only the contents of the USER CODE
  *       sections were written by us - namely the RTOS hook in SysTick_Handler,
  *       the HC-SR04 echo interrupt (EXTI0) and the UART RX interrupt, plus the
  *       trace instrumentation around them.
  *
  * Interrupt priorities used here (set in main.c and the drivers):
  *   SysTick 0  - highest, drives the scheduling decision
  *   EXTI0   5  - HC-SR04 echo edges, the most timing-critical signal
  *   USART1  6  - shell input
  *   PendSV  15 - lowest, so the context switch runs after every other ISR
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

/* NOTE: PendSV_Handler is DELIBERATELY not defined here - it is provided as
 * an assembly routine in pendsv.s, which performs the context switch.
 *
 * In CubeMX, under System Core -> NVIC -> Code generation, the
 * "Generate IRQ handler" checkbox for "Pendable request" must be CLEARED,
 * otherwise the generator emits a duplicate and the link fails.
 */


/**
  * @brief This function handles System tick timer.
  * @author Berkay (USER CODE sections)
  *
  * The heartbeat of the RTOS: advances the HAL tick, runs the delay/timeout
  * countdown, asks the scheduler for the next task and pends a PendSV if the
  * selection actually changed. The switch itself is deferred to PendSV so it
  * happens at the lowest priority, after all other pending interrupts.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
  OS_TRACE_ISR_ENTER();
  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  Scheduler_vCountdown();          // non-blocking delays & timeouts
  Scheduler_pGetNextTask();        // select the next task (sets g_pNextTask)

  if (g_pNextTask != g_pCurrentTask)
  {
      // Plain write instead of read-modify-write: PENDSVSET is write-1-to-set
      // and every other bit ignores a written 0, so no read is needed.
      SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;    // trigger PendSV
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
 * HC-SR04: echo edge interrupt (ECHO pin -> EXTI0)
 * -------------------------------------------------------------------------- */

/**
  * @brief Handles the HC-SR04 echo edge interrupt.
  * @author Berkay
  *
  * The ISR only captures timestamps and gives the semaphore at the end of the
  * pulse; it then returns to the interrupted task without triggering any
  * scheduling of its own. The actual work happens in HCSR04_vEchoEdgeIsr().
  */
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

/**
  * @brief GPIO EXTI callback, dispatched by the HAL per pin.
  * @param GPIO_Pin Pin that triggered the interrupt.
  * @author Berkay
  *
  * Called by the HAL from within HAL_GPIO_EXTI_IRQHandler(). The pin check
  * keeps the handler correct if further EXTI sources are added later.
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == HCSR04_ECHO_PIN)
  {
      HCSR04_vEchoEdgeIsr();
  }
}

/* --------------------------------------------------------------------------
 * UART: RX interrupt
 * -------------------------------------------------------------------------- */

/**
  * @brief Handles the USART1 interrupt (shell input).
  * @author Berkay
  *
  * The HAL dispatches to HAL_UART_RxCpltCallback() in uart_driver.c, which
  * stores the character in the ring buffer and gives g_uartRxSemaphore once a
  * complete line has arrived.
  */
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
