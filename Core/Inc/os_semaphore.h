/**
 ******************************************************************************
 * @file    os_semaphore.h
 * @brief   Counting semaphore - API.
 * @author  Andre
 ******************************************************************************
 *
 * Used in this project for two distinct purposes:
 *   - as a signalling primitive between an ISR and a task (the HC-SR04 echo
 *     ISR gives g_echoDoneSemaphore, the sensor task takes it), and
 *   - as a general counting resource guard.
 *
 * OS_Semaphore_Give() is ISR-safe, which is what makes the first use case
 * possible. The take operations must only be called from task context, since
 * an ISR cannot block.
 *
 * Waiting tasks are tracked in a bitmask (one bit per task index); a give
 * wakes all of them and lets the scheduler decide who gets the count.
 *
 ******************************************************************************
 */

#ifndef OS_SEMAPHORE_H
#define OS_SEMAPHORE_H

#include <stdint.h>
#include "os_common.h"

/** @brief Semaphore control block. Initialise with OS_Semaphore_Init(). */
typedef struct
{
    volatile uint8_t u8Count;     ///< Currently available count
    uint8_t u8MaxCount;           ///< Upper limit, give() saturates here
    uint8_t u8TraceId;            ///< ID used by the TeSSLa instrumentation

    /// Bitmask of tasks blocked on this semaphore (bit n = tasks[n])
    volatile uint32_t u32WaitMask;
} OS_Semaphore_t;

/**
 * @brief Initialise a semaphore.
 * @param sem          Semaphore to initialise.
 * @param initialCount Count available right after initialisation.
 * @param maxCount     Maximum count the semaphore can reach.
 * @param u8TraceId    ID reported in the trace events of this object.
 * @author Andre
 *
 * For ISR-to-task signalling use initialCount = 0 and maxCount = 1.
 */
void OS_Semaphore_Init(OS_Semaphore_t *sem, uint8_t initialCount, uint8_t maxCount,
                       uint8_t u8TraceId);

/**
 * @brief  Non-blocking take: decrement the count if possible, return at once.
 * @param  sem Semaphore to take.
 * @return OS_OK if a count was taken, OS_WOULD_BLOCK if the count was zero.
 * @author Andre
 */
OS_Result_t OS_Semaphore_TakeNonBlocking(OS_Semaphore_t *sem);

/**
 * @brief  Blocking take: block the calling task until a count is available.
 * @param  sem Semaphore to take.
 * @return Always OS_OK (the call only returns once a count was taken).
 * @author Andre
 */
OS_Result_t OS_Semaphore_TakeBlocking(OS_Semaphore_t *sem);

/**
 * @brief  Take with timeout: block for at most u32TimeoutTicks.
 * @param  sem             Semaphore to take.
 * @param  u32TimeoutTicks Maximum wait time in SysTick ticks.
 * @return OS_OK if a count was taken, OS_TIMEOUT if the time expired.
 * @author Andre
 *
 * The task is woken up either by a give or by the timeout, whichever comes
 * first. This is the variant the sensor task uses to survive a missing echo.
 */
OS_Result_t OS_Semaphore_TakeTimeout(OS_Semaphore_t *sem, uint32_t u32TimeoutTicks);

/**
 * @brief Give: increment the count (saturating at maxCount) and wake up every
 *        task waiting on this semaphore.
 * @param sem Semaphore to give.
 * @author Andre
 *
 * ISR-safe: may be called from interrupt context, which is how the HC-SR04
 * echo ISR hands the completed measurement over to the sensor task.
 */
void OS_Semaphore_Give(OS_Semaphore_t *sem);

#endif
