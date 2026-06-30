#include "hc_sr04.h"
#include "app_resources.h"
#include "main.h"
#include "board_config.h"
#include <stdint.h>

static TIM_HandleTypeDef htim2; /*brauchen wir das bzw. ist das bereits implementiert?*/

static volatile uint32_t echo_start_us = 0;
static volatile uint32_t echo_end_us   = 0;
static volatile uint32_t echo_pulse_us = 0;

static void delay_us(uint32_t us)
{
    uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
    while ((__HAL_TIM_GET_COUNTER(&htim2) - start) < us)
    {
        /* Busy-Wait – CPU blockiert absichtlich für µs-genauen Puls */
    }
}

void HC_SR04_Init(void)
{
    /* ------------------------------------------------------------------
     * 1. TIM2: 32-bit-Freilaufzähler, 1 MHz (= 1 Tick pro Mikrosekunde)
     * ------------------------------------------------------------------ */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /*
     * Timer-Takt berechnen:
     *   APB1-Timer-Takt = HAL_RCC_GetPCLK1Freq()         wenn APB1-Prescaler = 1
     *   APB1-Timer-Takt = HAL_RCC_GetPCLK1Freq() × 2     wenn APB1-Prescaler ≠ 1
     *
     * Beispiel B-L475E-IOT01A (SYSCLK = 80 MHz, kein APB1-Teiler):
     *   tim_clk  = 80 000 000
     *   Prescaler = 80 000 000 / 1 000 000 - 1 = 79
     *   → Ein Zähler-Tick = 1 µs
     */
    uint32_t pclk1   = HAL_RCC_GetPCLK1Freq();
    uint32_t tim_clk = ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
                       ? (pclk1 * 2U)
                       : pclk1;

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = (tim_clk / 1000000UL) - 1U;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFFU;  /* 32-bit Timer, maximaler Zählerstand */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    /* Freilaufzähler starten – läuft dauerhaft weiter */
    HAL_TIM_Base_Start(&htim2);

    /* ------------------------------------------------------------------
     * 2. TRIG-Pin: Push-Pull-Output, initial Low
     * ------------------------------------------------------------------ */
    TRIG_CLK_EN();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = TRIG_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TRIG_GPIO_Port, &gpio);
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    /* ------------------------------------------------------------------
     * 3. ECHO-Pin: Interrupt bei steigender UND fallender Flanke
     *    Pull-Down: definierter Low-Pegel wenn kein Echo aktiv
     * ------------------------------------------------------------------ */
    ECHO_CLK_EN();

    gpio.Pin  = ECHO_Pin;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLDOWN;
    /* Speed irrelevant für Interrupt-Eingänge */
    HAL_GPIO_Init(ECHO_GPIO_Port, &gpio);

    /* ------------------------------------------------------------------
     * 4.  NVIC: ECHO-IRQ aktivieren (IRQn kommt aus board_config.h)
     *    Priorität 5 liegt zwischen SysTick (0) und PendSV (0xFF).
     *    Kein Zugriff auf OS-Datenstrukturen aus dieser ISR heraus nötig
     *    außer OS_Semaphore_Give – das ist interrupt-safe (keine Preemption
     *    nötig, da kooperativer Scheduler).
     * ------------------------------------------------------------------ */
    HAL_NVIC_SetPriority(ECHO_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(ECHO_IRQn);
}

void HC_SR04_Trigger(void)
{
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
    delay_us(2U);

    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
    delay_us(10U);

    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
}

uint32_t HC_SR04_GetEchoPulseUs(void)
{
    return echo_pulse_us;
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ECHO_Pin)
    {
        if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
        {
            /* Steigende Flanke: Echo-Puls beginnt */
            echo_start_us = __HAL_TIM_GET_COUNTER(&htim2);
        }
        else
        {
            /* Fallende Flanke: Echo-Puls endet */
            echo_end_us   = __HAL_TIM_GET_COUNTER(&htim2);

            /*
             * Korrekt auch bei 32-bit-Überlauf:
             * Beispiel: start = 0xFFFFFFFC, end = 0x00000005
             *   end - start = 5 - 4294967292 = 9 (mod 2^32)  ✓
             */
            echo_pulse_us = echo_end_us - echo_start_us;

            OS_Semaphore_Give(&g_echoDoneSemaphore);
        }
    }
}
