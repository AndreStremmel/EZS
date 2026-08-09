/*
 * board_config.h
 *  Zentrale Pin-/Peripherie-Zuordnung fuer das STM32L475VG
 *  (z.B. B-L475E-IOT01A Discovery). Bei anderer Verdrahtung NUR diese
 *  Datei anpassen.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32l4xx_hal.h"

/* --------------------------------------------------------------------------
 * UART (Shell + Distanzausgabe): USART1
 * PB6 = USART1_TX, PB7 = USART1_RX
 * Auf dem B-L475E-IOT01A ist USART1 direkt mit dem ST-LINK Virtual COM
 * Port verbunden -> Ausgabe/Eingabe einfach ueber den ST-LINK-USB im
 * Terminal (115200 8N1), KEIN externer USB-TTL-Adapter noetig.
 * -------------------------------------------------------------------------- */
extern UART_HandleTypeDef huart1;
#define APP_UART_HANDLE   huart1
#define APP_UART_IRQn     USART1_IRQn

/* --------------------------------------------------------------------------
 * HC-SR04 Ultraschallsensor
 * TRIG: PA4 (GPIO Output Push-Pull) - auf dem IOT01A: Arduino-Pin D7
 * ECHO: PB0 (EXTI Line 0, beide Flanken) - auf dem IOT01A: Arduino-Pin D3
 *       Spannungsteiler 1k/2k empfohlen (5V-Sensorpegel -> 3.3V-MCU)!
 *
 * WICHTIG: PB4/PB5 sind auf dem B-L475E-IOT01A NICHT frei nutzbar - laut
 * UM2153 Table 11 sind sie fest mit dem Onboard-Sub-GHz-Funkmodul
 * (SPSGRF-915: SPI3_CSN auf PB5, TIM3_CH1 auf PB4) verdrahtet. PA4 und
 * PB0 sind laut derselben Tabelle frei ("GPIO_Output, ARD.D7" bzw.
 * "TIM3_CH3, ARD.D3-PWM/INT1_EXTI0") und liegen direkt am Arduino-Header.
 *
 * Falls ihr ein ANDERES Board oder andere Pins verwendet: nur diese
 * beiden Defines anpassen. ECHO auf einer Pin-NUMMER 0..4 -> Handler
 * heisst EXTIx_IRQHandler (x = Pin-Nummer, eigener Handler pro Pin);
 * Pin-Nummer 5..9 -> EXTI9_5_IRQHandler; Pin-Nummer 10..15 ->
 * EXTI15_10_IRQHandler. Der Handler-Name in stm32l4xx_it.c muss dann
 * mitgeaendert werden.
 * -------------------------------------------------------------------------- */
#define HCSR04_TRIG_PORT   GPIOA
#define HCSR04_TRIG_PIN    GPIO_PIN_4

#define HCSR04_ECHO_PORT   GPIOB
#define HCSR04_ECHO_PIN    GPIO_PIN_0
#define HCSR04_ECHO_IRQn   EXTI0_IRQn

#endif /* BOARD_CONFIG_H */
