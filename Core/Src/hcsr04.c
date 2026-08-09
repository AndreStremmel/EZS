/**
 ******************************************************************************
 * @file    hcsr04.c
 * @brief   Interrupt-driven driver for the HC-SR04 ultrasonic sensor -
 *          implementation.
 * @author  __________
 ******************************************************************************
 *
 * Timing source: DWT->CYCCNT (the CPU cycle counter). The conversion to
 * microseconds goes through SystemCoreClock, so the driver stays correct at
 * any system clock (48 MHz, 72 MHz, ...). The counter free-runs; the unsigned
 * difference (end - start) is still correct across an overflow.
 *
 * IMPORTANT: never zero CYCCNT - SystemView uses the same counter for its
 * timestamps, and resetting it would corrupt the trace.
 *
 * Conversion: mm = pulse_us * 100 / 583
 * (speed of sound 343 m/s, and the pulse covers the round trip, so
 * us / 5.83 = mm)
 *
 * @see hcsr04.h for the measurement sequence.
 *
 ******************************************************************************
 */

#include "hcsr04.h"
#include "board_config.h"
#include "app_resources.h"
#include "os_semaphore.h"

/* Measurement state - only the ISR and the sensor task touch these. The task
 * reads them only after taking the semaphore, which the ISR gives after it has
 * finished writing, so there is no race. */
static volatile uint32_t s_u32RiseCycles  = 0u;   ///< CYCCNT at the rising edge
static volatile uint32_t s_u32PulseCycles = 0u;   ///< Pulse width in CPU cycles
static volatile uint8_t  s_u8Armed        = 0u;   ///< Measurement armed?
static volatile uint8_t  s_u8GotRise      = 0u;   ///< Rising edge seen?

/**
 * @brief Initialise the driver: cycle counter, EXTI priority, trigger pin.
 * @author __________
 */
void HCSR04_vInit(void)
{
    /* Enable the DWT cycle counter defensively (without resetting it!) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    /* EXTI priority 5: below SysTick (0), above UART (6) - the echo timing is
     * the most time-critical signal in the system. */
    HAL_NVIC_SetPriority(HCSR04_ECHO_IRQn, 5u, 0u);
    HAL_NVIC_EnableIRQ(HCSR04_ECHO_IRQn);

    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Busy-wait for a number of microseconds using the cycle counter.
 * @param us Delay in microseconds.
 * @author __________
 *
 * Only used for the 10 us trigger pulse - far too short to be worth a
 * scheduler round trip.
 */
static void prv_vDelayUs(uint32_t us)
{
    uint32_t u32Start  = DWT->CYCCNT;
    uint32_t u32Cycles = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - u32Start) < u32Cycles) { }
}

/**
 * @brief Emit the trigger pulse and arm the edge capture for one measurement.
 * @author __________
 */
void HCSR04_vStartMeasurement(void)
{
    s_u8GotRise = 0u;
    s_u8Armed   = 1u;

    /* 10 us trigger pulse as specified by the datasheet */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    prv_vDelayUs(10u);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Edge handler for the echo pin, called from HAL_GPIO_EXTI_Callback().
 * @author __________
 *
 * Timestamps the rising edge, computes the pulse width on the falling edge,
 * disarms the measurement and signals g_echoDoneSemaphore so the waiting
 * sensor task becomes runnable again.
 */
void HCSR04_vEchoEdgeIsr(void)
{
    if (s_u8Armed == 0u)
    {
        return;   /* Edge without a measurement in progress -> ignore */
    }

    if (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
    {
        /* Rising edge: the echo starts */
        s_u32RiseCycles = DWT->CYCCNT;
        s_u8GotRise     = 1u;
    }
    else if (s_u8GotRise != 0u)
    {
        /* Falling edge: the echo ends -> latch the pulse width, disarm the
         * measurement and wake the sensor task */
        s_u32PulseCycles = DWT->CYCCNT - s_u32RiseCycles;
        s_u8Armed        = 0u;
        s_u8GotRise      = 0u;
        OS_Semaphore_Give(&g_echoDoneSemaphore);
    }
}

/**
 * @brief  Width of the last measured echo pulse in microseconds.
 * @return Pulse width in us.
 * @author __________
 */
uint32_t HCSR04_u32GetPulseUs(void)
{
    return s_u32PulseCycles / (SystemCoreClock / 1000000u);
}

/**
 * @brief  Uncalibrated distance derived from the last echo pulse.
 * @return Distance in mm (see the conversion in the file header).
 * @author __________
 */
uint32_t HCSR04_u32GetDistanceMmRaw(void)
{
    return (HCSR04_u32GetPulseUs() * 100u) / 583u;
}

/**
 * @brief  Plausibility check on the last measurement.
 * @return 1 if the pulse width was outside the datasheet limits, 0 otherwise.
 * @author __________
 */
uint8_t HCSR04_u8IsOutOfRange(void)
{
    uint32_t u32Us = HCSR04_u32GetPulseUs();
    return (u32Us < HCSR04_PULSE_MIN_US || u32Us > HCSR04_PULSE_MAX_US) ? 1u : 0u;
}
