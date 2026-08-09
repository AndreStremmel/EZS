/**
 ******************************************************************************
 * @file    os_semaphore.c
 * @brief   Counting semaphore - implementation.
 * @author  __________
 ******************************************************************************
 *
 * All take variants share prv_Take(). The central design decision is that a
 * wake-up does not hand ownership to the woken task: it only means "the count
 * may have changed, try again". This keeps the give path free of any knowledge
 * about which task should get the count and lets the scheduler decide, which
 * in turn gives the priority ordering the TeSSLa specs check for.
 *
 * OS_Semaphore_Give() is deliberately ISR-safe - it is the mechanism the
 * HC-SR04 echo ISR uses to hand a completed measurement to the sensor task.
 *
 * @see os_semaphore.h for the API description.
 *
 ******************************************************************************
 */

#include "os_semaphore.h"
#include "scheduler.h"
#include "os_trace.h"

/** @brief SystemView "blocked on semaphore" cause code. */
#define SEM_SYSVIEW_CAUSE  ( 1u )

/**
 * @brief  Determine the task ID to report in trace events.
 * @return The current task index, or OS_TRACE_TASK_ISR when running in an ISR.
 * @author __________
 *
 * Needed because a give may come from interrupt context, where the "current
 * task" is whatever happened to be interrupted and would be misleading.
 */
static uint8_t prv_u8TraceTask(void)
{
    return OS_bInIsrContext() ? OS_TRACE_TASK_ISR : Scheduler_u8GetCurrentTaskIdx();
}

/**
 * @brief Initialise a semaphore.
 * @param sem          Semaphore to initialise.
 * @param initialCount Count available right after initialisation.
 * @param maxCount     Maximum count the semaphore can reach.
 * @param u8TraceId    ID reported in the trace events of this object.
 * @author __________
 */
void OS_Semaphore_Init(OS_Semaphore_t *sem, uint8_t initialCount, uint8_t maxCount,
                       uint8_t u8TraceId)
{
    sem->u8Count     = initialCount;
    sem->u8MaxCount  = maxCount;
    sem->u8TraceId   = u8TraceId;
    sem->u32WaitMask = 0u;
}

/**
 * @brief  Shared core of all take variants.
 * @param  sem             Semaphore to take.
 * @param  u32TimeoutTicks Timeout in ticks, or OS_WAIT_FOREVER.
 * @param  u8NonBlocking   1 = return OS_WOULD_BLOCK instead of blocking.
 * @return OS_OK, OS_WOULD_BLOCK or OS_TIMEOUT.
 * @author __________
 *
 * One pass through the loop:
 *  1. Critical section: clear our own wait bit (retry only), attempt the take.
 *  2. Success              -> OS_OK.
 *  3. Non-blocking call    -> OS_WOULD_BLOCK.
 *  4. Timeout expired      -> OS_TIMEOUT.
 *  5. Otherwise: set the wait bit, block the task, leave the critical section
 *     and wait until SysTick or a give wakes us -> retry.
 *
 * If several gives wake several waiters, the scheduler automatically lets the
 * highest priority task win the retry.
 */
static OS_Result_t prv_Take(OS_Semaphore_t *sem, uint32_t u32TimeoutTicks, uint8_t u8NonBlocking)
{
    uint8_t  u8Idx  = Scheduler_u8GetCurrentTaskIdx();
    uint32_t u32Bit = (1u << u8Idx);
    uint8_t  u8FirstIteration = 1u;

    for (;;)
    {
        uint32_t u32PriMask = OS_u32EnterCritical();

        if (u8FirstIteration != 0u)
        {
            OS_TRACE_SEM_TRY2(OS_TRACE_EVT_SEM_TAKE_TRY, sem->u8TraceId, u8Idx);
        }

        // Clear our own wait bit only on a retry (i.e. after being woken up).
        // On the first pass our bit is guaranteed to be 0 anyway - and a
        // non-blocking call made from an ISR must not accidentally clear the
        // wait bit of the (blocked) task it just interrupted.
        if (u8FirstIteration == 0u)
        {
            sem->u32WaitMask &= ~u32Bit;
        }

        if (sem->u8Count > 0u)
        {
            sem->u8Count--;
            OS_TRACE_SEM3(OS_TRACE_EVT_SEM_TAKE_OK, sem->u8TraceId, u8Idx, sem->u8Count);
            OS_vExitCritical(u32PriMask);
            return OS_OK;
        }

        if (u8NonBlocking != 0u)
        {
            OS_vExitCritical(u32PriMask);
            return OS_WOULD_BLOCK;
        }

        // Timeout variant: check the remaining time. After the first block,
        // the remaining time is tracked by the scheduler countdown, so a
        // spurious wake-up cannot restart the full timeout.
        if (u32TimeoutTicks != OS_WAIT_FOREVER)
        {
            uint32_t u32Remaining = u8FirstIteration ? u32TimeoutTicks
                                                     : Scheduler_u32GetRemainingDelay(u8Idx);
            if (u32Remaining == 0u)
            {
                OS_TRACE_SEM2(OS_TRACE_EVT_SEM_TAKE_TIMEOUT, sem->u8TraceId, u8Idx);
                OS_vExitCritical(u32PriMask);
                return OS_TIMEOUT;
            }
            // Block for the remaining time
            sem->u32WaitMask |= u32Bit;
            OS_TRACE_SEM3(OS_TRACE_EVT_SEM_TAKE_BLOCK, sem->u8TraceId, u8Idx, u32Remaining);
            Scheduler_vBlockCurrentTask(u32Remaining, SEM_SYSVIEW_CAUSE);
        }
        else
        {
            // Blocking variant: wait indefinitely
            sem->u32WaitMask |= u32Bit;
            OS_TRACE_SEM3(OS_TRACE_EVT_SEM_TAKE_BLOCK, sem->u8TraceId, u8Idx, OS_WAIT_FOREVER);
            Scheduler_vBlockCurrentTask(OS_WAIT_FOREVER, SEM_SYSVIEW_CAUSE);
        }

        u8FirstIteration = 0u;
        OS_vExitCritical(u32PriMask);

        Scheduler_vWaitWhileBlocked();
        // Woken up (by a give or by the timeout) -> retry
    }
}

/**
 * @brief  Non-blocking take.
 * @param  sem Semaphore to take.
 * @return OS_OK or OS_WOULD_BLOCK.
 * @author __________
 */
OS_Result_t OS_Semaphore_TakeNonBlocking(OS_Semaphore_t *sem)
{
    return prv_Take(sem, 0u, 1u);
}

/**
 * @brief  Blocking take: wait indefinitely for a count.
 * @param  sem Semaphore to take.
 * @return Always OS_OK.
 * @author __________
 */
OS_Result_t OS_Semaphore_TakeBlocking(OS_Semaphore_t *sem)
{
    return prv_Take(sem, OS_WAIT_FOREVER, 0u);
}

/**
 * @brief  Take with timeout.
 * @param  sem             Semaphore to take.
 * @param  u32TimeoutTicks Maximum wait time in ticks.
 * @return OS_OK or OS_TIMEOUT.
 * @author __________
 *
 * A timeout of 0 would block forever in the countdown logic, so it is mapped
 * to the non-blocking variant instead.
 */
OS_Result_t OS_Semaphore_TakeTimeout(OS_Semaphore_t *sem, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Semaphore_TakeNonBlocking(sem);
    }
    return prv_Take(sem, u32TimeoutTicks, 0u);
}

/**
 * @brief Give: increment the count (saturating at maxCount) and wake up every
 *        waiting task.
 * @param sem Semaphore to give.
 * @author __________
 *
 * ISR-safe. A give on an already full semaphore is discarded rather than
 * queued, and reported as OS_TRACE_EVT_SEM_GIVE_IGNORED so the loss is visible
 * in the trace instead of silent.
 */
void OS_Semaphore_Give(OS_Semaphore_t *sem)
{
    uint32_t u32PriMask = OS_u32EnterCritical();
    uint8_t  u8Task = prv_u8TraceTask();
    (void)u8Task;   /* used by the trace macros only - see os_trace_config.h */

    if (sem->u8Count < sem->u8MaxCount)
    {
        sem->u8Count++;
        OS_TRACE_SEM3(OS_TRACE_EVT_SEM_GIVE_OK, sem->u8TraceId, u8Task, sem->u8Count);
    }
    else
    {
        // Binary semaphore was already signalled -> this give is discarded
        OS_TRACE_SEM2(OS_TRACE_EVT_SEM_GIVE_IGNORED, sem->u8TraceId, u8Task);
    }

    // Wake all waiters - each of them retries the take.
    // (The highest priority one wins, the others block again.)
    uint32_t u32Waiters = sem->u32WaitMask;
    sem->u32WaitMask = 0u;
    for (uint8_t i = 0u; i < NUM_TASKS; i++)
    {
        if (u32Waiters & (1u << i))
        {
            Scheduler_vUnblockTask(i);
        }
    }

    OS_vExitCritical(u32PriMask);
}
