#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ==========================================================================
 * STM32L4 – B-L475E-IOT01A
 * ========================================================================== */
#if defined(STM32L475xx) || defined(STM32L476xx)

    #include "stm32l4xx_hal.h"

    /* TRIG: PA5 (ARD_D13) – GPIO Push-Pull Output
     * PA5 ist als SPI1_SCK vorkonfiguriert, aber als reiner GPIO Output
     * für den kurzen TRIG-Puls (10 µs) problemlos nutzbar. */
    #define TRIG_GPIO_Port      GPIOA
    #define TRIG_Pin            GPIO_PIN_5
    #define TRIG_CLK_EN()       __HAL_RCC_GPIOA_CLK_ENABLE()

    /* ECHO: PD10 (ARD_D5) – EXTI10, Rising + Falling
     * PA6 (ARD_D12) NICHT verwenden – EXTI6 ist durch das
     * Bluetooth-Modul SPBTLE-RF auf PE6 bereits belegt. */
    #define ECHO_GPIO_Port      GPIOD
    #define ECHO_Pin            GPIO_PIN_2
    #define ECHO_CLK_EN()       __HAL_RCC_GPIOB_CLK_ENABLE()
    #define ECHO_IRQn           EXTI2_IRQn

	/* --- UART (ST-LINK Virtual COM Port) --------------------------------- */

    /* USART1: PB6 (TX) / PB7 (RX), 115200 Baud, konfiguriert von CubeMX.
     * UART_Init() muss nach MX_USART1_UART_Init() aufgerufen werden. */
    extern UART_HandleTypeDef   huart1;
    #define APP_UART_HANDLE     huart1
    #define APP_UART_IRQn       USART1_IRQn

/* ==========================================================================
 * STM32F3 – STM32F303VC Board
 * ========================================================================== */
#elif defined(STM32F303xC) || defined(STM32F303xE)

    #include "stm32f3xx_hal.h"

    /* TRIG: freien GPIO-Output-Pin wählen, der nicht mit SPI/I2C/UART
     * belegt ist. */

    #define TRIG_GPIO_Port      GPIOA
    #define TRIG_Pin            GPIO_PIN_3  
    #define TRIG_CLK_EN()       __HAL_RCC_GPIOA_CLK_ENABLE()

    /* ECHO: Pin ohne EXTI-Konflikt wählen. Achtung beim F303:
     * EXTI2 teilt sich den IRQ-Vektor mit dem Touch-Sensing-Controller
     * (EXTI2_TSC_IRQHandler) – besser EXTI0, EXTI1, EXTI3 oder EXTI4
     * verwenden. Unten ist PB1 / EXTI1 als Platzhalter.
     * !! ANPASSEN auf den tatsächlich verfügbaren Pin !!
     * !! ECHO_IRQn muss zur Pin-Nummer passen:
     *    Pin 0 → EXTI0_IRQn, Pin 1 → EXTI1_IRQn, Pin 4 → EXTI4_IRQn,
     *    Pin 5–9 → EXTI9_5_IRQn, Pin 10–15 → EXTI15_10_IRQn         !! */
    #define ECHO_GPIO_Port      GPIOE
    #define ECHO_Pin            GPIO_PIN_3
    #define ECHO_CLK_EN()       __HAL_RCC_GPIOE_CLK_ENABLE()
    #define ECHO_IRQn           EXTI3_IRQn  

	/* --- UART ------------------------------------------------------------ */

    /* USART1: PA9 (TX) / PA10 (RX) – typisch für F303-Boards.
     * !! ANPASSEN falls das Board einen anderen UART nutzt !! */
    extern UART_HandleTypeDef   huart1;
    #define APP_UART_HANDLE     huart1
    #define APP_UART_IRQn       USART1_IRQn 

#else
    #error "Unbekanntes Board – STM32L475xx oder STM32F303xC in den Projekt-Properties prüfen."

#endif

#endif /* BOARD_CONFIG_H */
