/**
 ******************************************************************************
 * @file    board_config.h
 * @brief   Central pin and peripheral assignment for the STM32L475VG board.
 * @author  Andre
 ******************************************************************************
 *
 * Targets the B-L475E-IOT01A Discovery kit. If the project is moved to a
 * different board or the sensor is rewired, ONLY this file needs to change -
 * no other module hard-codes a pin or a peripheral handle.
 *
 ******************************************************************************
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32l4xx_hal.h"

/* --------------------------------------------------------------------------
 * UART (shell + distance output): USART1
 * PB6 = USART1_TX, PB7 = USART1_RX
 *
 * On the B-L475E-IOT01A, USART1 is wired straight to the ST-LINK virtual COM
 * port, so input and output are available over the ST-LINK USB connection in
 * a terminal (115200 8N1). No external USB-TTL adapter is required.
 * -------------------------------------------------------------------------- */
extern UART_HandleTypeDef huart1;
/** @brief HAL handle used by the whole application for shell I/O. */
#define APP_UART_HANDLE   huart1
/** @brief Interrupt line of the application UART, enabled in UART_Init(). */
#define APP_UART_IRQn     USART1_IRQn

/* --------------------------------------------------------------------------
 * HC-SR04 ultrasonic sensor
 * TRIG: PA4 (GPIO output, push-pull) - Arduino pin D7 on the IOT01A
 * ECHO: PB0 (EXTI line 0, both edges) - Arduino pin D3 on the IOT01A
 *       A 1k/2k voltage divider is recommended, since the sensor drives the
 *       echo line at 5 V while the MCU pin is 3.3 V!
 *
 * When moving to a different board or different pins, adjust only the two
 * defines below - but mind the EXTI handler naming: an ECHO pin NUMBER of
 * 0..4 has its own handler EXTIx_IRQHandler (x = pin number), pin numbers
 * 5..9 share EXTI9_5_IRQHandler, and 10..15 share EXTI15_10_IRQHandler. The
 * handler implemented in stm32l4xx_it.c has to be renamed accordingly.
 * -------------------------------------------------------------------------- */
/** @brief GPIO port of the HC-SR04 trigger output. */
#define HCSR04_TRIG_PORT   GPIOA
/** @brief GPIO pin of the HC-SR04 trigger output. */
#define HCSR04_TRIG_PIN    GPIO_PIN_4

/** @brief GPIO port of the HC-SR04 echo input. */
#define HCSR04_ECHO_PORT   GPIOB
/** @brief GPIO pin of the HC-SR04 echo input. */
#define HCSR04_ECHO_PIN    GPIO_PIN_0
/** @brief EXTI interrupt line associated with the echo pin. */
#define HCSR04_ECHO_IRQn   EXTI0_IRQn

#endif /* BOARD_CONFIG_H */
