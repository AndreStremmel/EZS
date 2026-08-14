/**
 ******************************************************************************
 * @file    os_mutex.h
 * @brief   Binary mutex with owner tracking - API.
 * @author  Andre
 ******************************************************************************
 *
 * A mutex differs from a binary semaphore in that it has an owner: only the
 * task that locked it is allowed to unlock it again. This is what makes it
 * the correct primitive for protecting a shared resource such as the UART.
 *
 * Waiting tasks are recorded in a bitmask (one bit per task index) instead of
 * a linked list, which keeps the object small and the wake-up loop bounded by
 * NUM_TASKS. On unlock, all waiters are made Ready and race for the mutex;
 * whoever the scheduler dispatches first wins, the others block again.
 *
 * @note This implementation does not perform priority inheritance, so priority
 *       inversion is possible if tasks of different priorities share a mutex.
 *
 ******************************************************************************
 */

#ifndef OS_MUTEX_H
#define OS_MUTEX_H

#include <stdint.h>
#include "os_common.h"

/** @brief Mutex control block. Initialise with OS_Mutex_Init() before use. */
typedef struct
{
    volatile uint8_t u8Locked;    ///< 1 = currently held, 0 = free
    uint8_t u8OwnerTaskId;        ///< Owner task ID (1-based), 0 = unowned
    uint8_t u8TraceId;            ///< ID used by the TeSSLa instrumentation

    /// Bitmask of tasks blocked on this mutex (bit n = tasks[n])
    volatile uint32_t u32WaitMask;
} OS_Mutex_t;

/**
 * @brief Initialise a mutex to the unlocked state.
 * @param mutex     Mutex to initialise.
 * @param u8TraceId ID reported in the trace events of this object.
 * @author Andre
 */
void OS_Mutex_Init(OS_Mutex_t *mutex, uint8_t u8TraceId);

/**
 * @brief  Non-blocking lock: try to take the mutex and return immediately.
 * @param  mutex Mutex to lock.
 * @return OS_OK if the mutex was acquired, OS_WOULD_BLOCK if it is held by
 *         another task.
 * @author Andre
 *
 * On success the calling task automatically becomes the owner.
 */
OS_Result_t OS_Mutex_LockNonBlocking(OS_Mutex_t *mutex);

/**
 * @brief  Blocking lock: block the calling task until the mutex becomes free.
 * @param  mutex Mutex to lock.
 * @return Always OS_OK (the call only returns once the mutex is held).
 * @author Andre
 */
OS_Result_t OS_Mutex_LockBlocking(OS_Mutex_t *mutex);

/**
 * @brief  Lock with timeout: block for at most u32TimeoutTicks.
 * @param  mutex           Mutex to lock.
 * @param  u32TimeoutTicks Maximum wait time in SysTick ticks.
 * @return OS_OK if the mutex was acquired, OS_TIMEOUT if the time expired.
 * @author Andre
 *
 * The task is woken up either when the mutex is released or when the timeout
 * elapses, whichever happens first.
 */
OS_Result_t OS_Mutex_LockTimeout(OS_Mutex_t *mutex, uint32_t u32TimeoutTicks);

/**
 * @brief Unlock the mutex and wake up every task waiting for it.
 * @param mutex Mutex to release.
 * @author Andre
 *
 * Only the owning task may unlock; calls from any other task are ignored.
 */
void OS_Mutex_Unlock(OS_Mutex_t *mutex);

#endif
