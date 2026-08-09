#include "os_mutex.h"
#include "scheduler.h"
#include "os_trace.h"

/// SystemView-Cause fuer "blockiert an Mutex"
#define MUTEX_SYSVIEW_CAUSE  ( 2u )

void OS_Mutex_Init(OS_Mutex_t *mutex, uint8_t u8TraceId)
{
    mutex->u8Locked      = 0u;
    mutex->u8OwnerTaskId = 0u;
    mutex->u8TraceId     = u8TraceId;
    mutex->u32WaitMask   = 0u;
}

/**
 * Gemeinsamer Kern aller Lock-Varianten. Gleiches Retry-Muster wie bei
 * der Semaphore (siehe os_semaphore.c).
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

        // Eigenes Wartebit nur beim Retry loeschen (siehe os_semaphore.c)
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

        Scheduler_vWaitWhileBlocked();
    }
}

OS_Result_t OS_Mutex_LockNonBlocking(OS_Mutex_t *mutex)
{
    return prv_Lock(mutex, 0u, 1u);
}

OS_Result_t OS_Mutex_LockBlocking(OS_Mutex_t *mutex)
{
    return prv_Lock(mutex, OS_WAIT_FOREVER, 0u);
}

OS_Result_t OS_Mutex_LockTimeout(OS_Mutex_t *mutex, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Mutex_LockNonBlocking(mutex);
    }
    return prv_Lock(mutex, u32TimeoutTicks, 0u);
}

void OS_Mutex_Unlock(OS_Mutex_t *mutex)
{
    uint32_t u32PriMask = OS_u32EnterCritical();
    uint8_t  u8Idx = Scheduler_u8GetCurrentTaskIdx();
    (void)u8Idx;   /* nur fuer Trace - s. os_trace_config.h */

    // Nur der Besitzer darf freigeben
    if (mutex->u8Locked != 0u && mutex->u8OwnerTaskId == g_pCurrentTask->u8TaskId)
    {
        mutex->u8Locked      = 0u;
        mutex->u8OwnerTaskId = 0u;
        OS_TRACE_MTX2(OS_TRACE_EVT_MTX_UNLOCK_OK, mutex->u8TraceId, u8Idx);

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
        // Freigabe durch Nicht-Besitzer (oder unlocked) -> abgelehnt
        OS_TRACE_MTX2(OS_TRACE_EVT_MTX_UNLOCK_DENIED, mutex->u8TraceId, u8Idx);
    }

    OS_vExitCritical(u32PriMask);
}
