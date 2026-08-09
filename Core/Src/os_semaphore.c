#include "os_semaphore.h"
#include "scheduler.h"
#include "os_trace.h"

/// SystemView-Cause fuer "blockiert an Semaphore"
#define SEM_SYSVIEW_CAUSE  ( 1u )

/// Task-ID fuer Trace-Events ermitteln - aus ISRs OS_TRACE_TASK_ISR melden
static uint8_t prv_u8TraceTask(void)
{
    return OS_bInIsrContext() ? OS_TRACE_TASK_ISR : Scheduler_u8GetCurrentTaskIdx();
}

void OS_Semaphore_Init(OS_Semaphore_t *sem, uint8_t initialCount, uint8_t maxCount,
                       uint8_t u8TraceId)
{
    sem->u8Count     = initialCount;
    sem->u8MaxCount  = maxCount;
    sem->u8TraceId   = u8TraceId;
    sem->u32WaitMask = 0u;
}

/**
 * Gemeinsamer Kern aller Take-Varianten.
 *
 * Ablauf pro Schleifendurchlauf:
 *  1. Kritischer Abschnitt: eigenes Wartebit loeschen (nur Retry), Take versuchen.
 *  2. Erfolg -> OS_OK.
 *  3. Non-Blocking -> OS_WOULD_BLOCK.
 *  4. Timeout abgelaufen -> OS_TIMEOUT.
 *  5. Sonst: Wartebit setzen, Task blockieren, kritischen Abschnitt
 *     verlassen und warten, bis SysTick/Give uns weckt -> Retry.
 *
 * Wecken mehrere Gives mehrere Wartende, "gewinnt" durch den Scheduler
 * automatisch der Task mit der hoechsten Prioritaet den Retry.
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

        // Eigenes Wartebit nur beim Retry (nach dem Aufwecken) loeschen.
        // Im ersten Durchlauf ist unser Bit garantiert 0 - und ein
        // NonBlocking-Aufruf aus einer ISR darf nicht versehentlich das
        // Wartebit des gerade unterbrochenen (blockierten) Tasks loeschen.
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

        // Timeout-Variante: Restzeit pruefen. Nach dem ersten Blockieren
        // steht die Restzeit im Scheduler-Countdown.
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
            // Mit (Rest-)Zeit blockieren
            sem->u32WaitMask |= u32Bit;
            OS_TRACE_SEM3(OS_TRACE_EVT_SEM_TAKE_BLOCK, sem->u8TraceId, u8Idx, u32Remaining);
            Scheduler_vBlockCurrentTask(u32Remaining, SEM_SYSVIEW_CAUSE);
        }
        else
        {
            // Blocking-Variante: unendlich warten
            sem->u32WaitMask |= u32Bit;
            OS_TRACE_SEM3(OS_TRACE_EVT_SEM_TAKE_BLOCK, sem->u8TraceId, u8Idx, OS_WAIT_FOREVER);
            Scheduler_vBlockCurrentTask(OS_WAIT_FOREVER, SEM_SYSVIEW_CAUSE);
        }

        u8FirstIteration = 0u;
        OS_vExitCritical(u32PriMask);

        Scheduler_vWaitWhileBlocked();
        // Geweckt (durch Give oder Timeout) -> Retry
    }
}

OS_Result_t OS_Semaphore_TakeNonBlocking(OS_Semaphore_t *sem)
{
    return prv_Take(sem, 0u, 1u);
}

OS_Result_t OS_Semaphore_TakeBlocking(OS_Semaphore_t *sem)
{
    return prv_Take(sem, OS_WAIT_FOREVER, 0u);
}

OS_Result_t OS_Semaphore_TakeTimeout(OS_Semaphore_t *sem, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Semaphore_TakeNonBlocking(sem);
    }
    return prv_Take(sem, u32TimeoutTicks, 0u);
}

void OS_Semaphore_Give(OS_Semaphore_t *sem)
{
    uint32_t u32PriMask = OS_u32EnterCritical();
    uint8_t  u8Task = prv_u8TraceTask();
    (void)u8Task;   /* nur fuer Trace - s. os_trace_config.h */

    if (sem->u8Count < sem->u8MaxCount)
    {
        sem->u8Count++;
        OS_TRACE_SEM3(OS_TRACE_EVT_SEM_GIVE_OK, sem->u8TraceId, u8Task, sem->u8Count);
    }
    else
    {
        // Binaere Semaphore war bereits gesetzt -> Give wird verworfen
        OS_TRACE_SEM2(OS_TRACE_EVT_SEM_GIVE_IGNORED, sem->u8TraceId, u8Task);
    }

    // Alle Wartenden aufwecken - sie versuchen erneut ein Take.
    // (Der hoechstpriore gewinnt, die anderen blockieren wieder.)
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
