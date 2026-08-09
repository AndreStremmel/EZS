/**
 ******************************************************************************
 * @file    hcsr04.h
 * @brief   Interrupt-driven driver for the HC-SR04 ultrasonic sensor - API.
 * @author  __________
 ******************************************************************************
 *
 * Sequence of a single measurement, as performed by the sensor task:
 *   1. HCSR04_vStartMeasurement()
 *        emits the 10 us trigger pulse and arms the edge capture
 *   2. OS_Semaphore_TakeTimeout(&g_echoDoneSemaphore, HCSR04_ECHO_TIMEOUT_TICKS)
 *        - OS_OK:      the echo pulse was fully measured (the EXTI ISR gave
 *                      the semaphore)
 *        - OS_TIMEOUT: no echo or an incomplete one -> sensor error
 *   3. HCSR04_u32GetDistanceMmRaw()
 *        returns the uncalibrated distance in mm
 *
 * The task therefore spends the whole echo window blocked instead of polling,
 * which is the point of doing the timing in the ISR.
 *
 ******************************************************************************
 */

#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>

/** @brief Maximum time to wait for an echo, in SysTick ticks.
 *  The HC-SR04 pulse is at most ~25 ms, the rest is reserve. */
#define HCSR04_ECHO_TIMEOUT_TICKS   ( 40u )

/** @brief Lower plausibility limit of the echo pulse width (datasheet: ~150 us). */
#define HCSR04_PULSE_MIN_US         ( 100u )
/** @brief Upper plausibility limit of the echo pulse width (datasheet: ~25 ms). */
#define HCSR04_PULSE_MAX_US         ( 30000u )

/**
 * @brief Initialise the driver: enable the DWT cycle counter, configure the
 *        EXTI priority and drive the trigger pin low.
 * @author __________
 *
 * @note The cycle counter is enabled defensively - SystemView normally turns
 *       it on already - but it is never reset, see hcsr04.c.
 */
void HCSR04_vInit(void);

/**
 * @brief Emit the trigger pulse and arm the edge capture for one measurement.
 * @author __________
 */
void HCSR04_vStartMeasurement(void);

/**
 * @brief  Width of the last measured echo pulse in microseconds.
 * @return Pulse width in us; only meaningful after a successful semaphore take.
 * @author __________
 */
uint32_t HCSR04_u32GetPulseUs(void);

/**
 * @brief  Uncalibrated distance derived from the last echo pulse.
 * @return Distance in mm, without the offset and factor from g_userConfig.
 * @author __________
 */
uint32_t HCSR04_u32GetDistanceMmRaw(void);

/**
 * @brief  Plausibility check on the last measurement.
 * @return 1 if the pulse width was outside HCSR04_PULSE_MIN_US ..
 *         HCSR04_PULSE_MAX_US, 0 otherwise.
 * @author __________
 */
uint8_t HCSR04_u8IsOutOfRange(void);

/**
 * @brief Edge handler for the echo pin.
 * @author __________
 *
 * Call from HAL_GPIO_EXTI_Callback() whenever the ECHO pin triggers an edge.
 * Timestamps the rising edge, computes the pulse width on the falling edge
 * and then signals g_echoDoneSemaphore.
 */
void HCSR04_vEchoEdgeIsr(void);

#endif /* HCSR04_H */
