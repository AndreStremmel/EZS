/**
 ******************************************************************************
 * @file    os_mutex.c
 * @brief   Binary mutex with owner tracking - implementation.
 * @author  __________
 ******************************************************************************
 *
 * All three lock variants (non-blocking, blocking, timeout) share a single
 * implementation in prv_Lock(). The retry loop follows the same pattern as the
 * semaphore (see os_semaphore.c): after being woken up, a task does not assume
 * it owns the mutex - it re-checks under a critical section and blocks again if
 * another task won the race.
 *
 * @see os_mutex.h for the API description.
 *
 ******************************************************************************
 */

#include "os_mutex.h"
#include "scheduler.h"
#include "os_trace.h"

/** @brief SystemView "blocked on mutex" cause code. */
#define MUTEX_SYSVIEW_CAUSE  ( 2u )

/**
 * @brief Initialise a mutex to the unlocked state.
 * @param mutex     Mutex to initialise.
 * @param u8TraceId ID reported in the trace events of this object.
 * @author __________
 */
void OS_Mutex_Init(OS_Mutex_t *mutex, uint8_t u8TraceId)
{
    mutex->u8Locked      = 0u;
    mutex->u8OwnerTaskId = 0u;
    mutex->u8TraceId     = u8TraceId;
    mutex->u32WaitMask   = 0u;
}

/**
 * @brief  Shared core of all lock variants.
 * @param  mutex           Mutex to lock.
 * @param  u32TimeoutTicks Timeout in ticks, or OS_WAIT_FOREVER.
 * @param  u8NonBlocking   1 = return OS_WOULD_BLOCK instead of blocking.
 * @return OS_OK, OS_WOULD_BLOCK or OS_TIMEOUT.
 * @author __________
 *
 * Uses the same retry pattern as the semaphore (see os_semaphore.c): the
 * availability check happens inside a critical section, and being woken up
 * only means "try again", not "you own it now".
 */
static OS_Result_t prv_Lock(OS_Mutex_t *mutex, uint32_t u32TimeoutTicks, uint8_t u8NonBlocking)
{
    uint8_t  u8Idx  = Scheduler_u8GetCurrentTaskIdx();
    uint32_t u32Bit = (1u << u8Idx);
    uint8_t  u8FirstIteration = 1u;

    for (;;)
    {
        uint32_t u32PriMask = OS_u32EnterCritical();

        if (u8FirstIteration != 0u)
        {
            OS_TRACE_MTX_TRY2(OS_TRACE_EVT_MTX_LOCK_TRY, mutex->u8TraceId, u8Idx);
        }

        // Clear our own wait bit only on a retry (see os_semaphore.c)
        if (u8FirstIteration == 0u)
        {
            mutex->u32WaitMask &= ~u32Bit;
        }

        if (mutex->u8Locked == 0u)
        {
            mutex->u8Locked      = 1u;
            mutex->u8OwnerTaskId = g_pCurrentTask->u8TaskId;
            OS_TRACE_MTX2(OS_TRACE_EVT_MTX_LOCK_OK, mutex->u8TraceId, u8Idx);
            OS_vExitCritical(u32PriMask);
            return OS_OK;
        }

        if (u8NonBlocking != 0u)
        {
            OS_vExitCritical(u32PriMask);
            return OS_WOULD_BLOCK;
        }

        if (u32TimeoutTicks != OS_WAIT_FOREVER)
        {
            // On a retry the remaining time comes from the scheduler, so the
            // timeout is not restarted from scratch on every wake-up.
            uint32_t u32Remaining = u8FirstIteration ? u32TimeoutTicks
                                                     : Scheduler_u32GetRemainingDelay(u8Idx);
            if (u32Remaining == 0u)
            {
                OS_TRACE_MTX2(OS_TRACE_EVT_MTX_LOCK_TIMEOUT, mutex->u8TraceId, u8Idx);
                OS_vExitCritical(u32PriMask);
                return OS_TIMEOUT;
            }
            mutex->u32WaitMask |= u32Bit;
            OS_TRACE_MTX3(OS_TRACE_EVT_MTX_LOCK_BLOCK, mutex->u8TraceId, u8Idx, u32Remaining);
            Scheduler_vBlockCurrentTask(u32Remaining, MUTEX_SYSVIEW_CAUSE);
        }
        else
        {
            mutex->u32WaitMask |= u32Bit;
            OS_TRACE_MTX3(OS_TRACE_EVT_MTX_LOCK_BLOCK, mutex->u8TraceId, u8Idx, OS_WAIT_FOREVER);
            Scheduler_vBlockCurrentTask(OS_WAIT_FOREVER, MUTEX_SYSVIEW_CAUSE);
        }

        u8FirstIteration = 0u;
        OS_vExitCritical(u32PriMask);

        // Leave the critical section first, then wait: the wake-up comes from
        // SysTick or from another task's unlock, both of which need interrupts.
        Scheduler_vWaitWhileBlocked();
    }
}

/**
 * @brief  Non-blocking lock: try to take the mutex and return immediately.
 * @param  mutex Mutex to lock.
 * @return OS_OK or OS_WOULD_BLOCK.
 * @author __________
 */
OS_Result_t OS_Mutex_LockNonBlocking(OS_Mutex_t *mutex)
{
    return prv_Lock(mutex, 0u, 1u);
}

/**
 * @brief  Blocking lock: block until the mutex becomes free.
 * @param  mutex Mutex to lock.
 * @return Always OS_OK.
 * @author __________
 */
OS_Result_t OS_Mutex_LockBlocking(OS_Mutex_t *mutex)
{
    return prv_Lock(mutex, OS_WAIT_FOREVER, 0u);
}

/**
 * @brief  Lock with timeout.
 * @param  mutex           Mutex to lock.
 * @param  u32TimeoutTicks Maximum wait time in ticks.
 * @return OS_OK or OS_TIMEOUT.
 * @author __________
 *
 * A timeout of 0 would block forever in the countdown logic, so it is mapped
 * to the non-blocking variant instead.
 */
OS_Result_t OS_Mutex_LockTimeout(OS_Mutex_t *mutex, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Mutex_LockNonBlocking(mutex);
    }
    return prv_Lock(mutex, u32TimeoutTicks, 0u);
}

/**
 * @brief Unlock the mutex and wake up every task waiting for it.
 * @param mutex Mutex to release.
 * @author __________
 *
 * Ownership is enforced here: a release attempt by a task that does not own
 * the mutex is rejected and reported as OS_TRACE_EVT_MTX_UNLOCK_DENIED, which
 * is one of the properties checked by mutex.tessla.
 */
void OS_Mutex_Unlock(OS_Mutex_t *mutex)
{
    uint32_t u32PriMask = OS_u32EnterCritical();
    uint8_t  u8Idx = Scheduler_u8GetCurrentTaskIdx();
    (void)u8Idx;   /* used by the trace macros only - see os_trace_config.h */

    // Only the owning task may release the mutex
    if (mutex->u8Locked != 0u && mutex->u8OwnerTaskId == g_pCurrentTask->u8TaskId)
    {
        mutex->u8Locked      = 0u;
        mutex->u8OwnerTaskId = 0u;
        OS_TRACE_MTX2(OS_TRACE_EVT_MTX_UNLOCK_OK, mutex->u8TraceId, u8Idx);

        // Snapshot and clear the mask first: the woken tasks set their own bit
        // again if they lose the race, and we must not wipe that out.
        uint32_t u32Waiters = mutex->u32WaitMask;
        mutex->u32WaitMask = 0u;
        for (uint8_t i = 0u; i < NUM_TASKS; i++)
        {
            if (u32Waiters & (1u << i))
            {
                Scheduler_vUnblockTask(i);
            }
        }
    }
    else
    {
        // Release by a non-owner (or on an unlocked mutex) -> rejected
        OS_TRACE_MTX2(OS_TRACE_EVT_MTX_UNLOCK_DENIED, mutex->u8TraceId, u8Idx);
    }

    OS_vExitCritical(u32PriMask);
}
