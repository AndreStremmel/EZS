/*
 * board_config.h
 *  Zentrale Pin-/Peripherie-Zuordnung fuer das STM32F303VC Discovery Board.
 *  Bei anderer Verdrahtung NUR diese Datei anpassen.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f3xx_hal.h"

/* --------------------------------------------------------------------------
 * UART (Shell + Distanzausgabe)
 * PA2 = USART2_TX, PA3 = USART2_RX  ->  externer USB-TTL-Adapter (3.3V!)
 * (Das F3-Discovery routet KEINEN VCP ueber den ST-Link - Adapter noetig.)
 * -------------------------------------------------------------------------- */
extern UART_HandleTypeDef huart2;
#define APP_UART_HANDLE   huart2
#define APP_UART_IRQn     USART2_IRQn

/* --------------------------------------------------------------------------
 * HC-SR04 Ultraschallsensor
 * TRIG: PB4 (GPIO Output Push-Pull)
 * ECHO: PB5 (EXTI, beide Flanken) - PB4/PB5 sind 5V-tolerant (FT),
 *       trotzdem Spannungsteiler empfohlen (siehe README, Verkabelung).
 * -------------------------------------------------------------------------- */
#define HCSR04_TRIG_PORT   GPIOB
#define HCSR04_TRIG_PIN    GPIO_PIN_4

#define HCSR04_ECHO_PORT   GPIOB
#define HCSR04_ECHO_PIN    GPIO_PIN_5
#define HCSR04_ECHO_IRQn   EXTI9_5_IRQn

#endif /* BOARD_CONFIG_H */
