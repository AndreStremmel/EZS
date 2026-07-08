#include "os_queue.h"
#include "scheduler.h"
#include "os_trace.h"
#include <string.h>

/// SystemView-Cause fuer "blockiert an Queue"
#define QUEUE_SYSVIEW_CAUSE  ( 3u )

void OS_Queue_Init(OS_Queue_t *queue,
                   uint8_t *buffer,
                   uint32_t itemSize,
                   uint32_t capacity,
                   uint8_t u8TraceId)
{
    queue->pu8Buffer   = buffer;
    queue->u32ItemSize = itemSize;
    queue->u32Capacity = capacity;

    queue->u32Head  = 0u;
    queue->u32Tail  = 0u;
    queue->u32Count = 0u;

    queue->u8TraceId = u8TraceId;

    queue->u32SendWaitMask = 0u;
    queue->u32RecvWaitMask = 0u;
}

/* ==========================================================================
 * Interne Helfer (muessen im kritischen Abschnitt aufgerufen werden!)
 * ========================================================================== */

/// Nachricht kopieren + Head weiterschieben. Weckt wartende Empfaenger.
static void prv_DoSend(OS_Queue_t *queue, const void *item)
{
    uint8_t *dest = queue->pu8Buffer + (queue->u32Head * queue->u32ItemSize);
    memcpy(dest, item, queue->u32ItemSize);

    queue->u32Head = (queue->u32Head + 1u) % queue->u32Capacity;
    queue->u32Count++;

    OS_Trace_Record3(OS_TRACE_EVT_Q_SEND_OK, queue->u8TraceId,
                     OS_Trace_u16Checksum(item, queue->u32ItemSize),
                     queue->u32Count);

    // Es ist jetzt eine Nachricht da -> alle wartenden Empfaenger wecken
    uint32_t u32Waiters = queue->u32RecvWaitMask;
    queue->u32RecvWaitMask = 0u;
    for (uint8_t i = 0u; i < NUM_TASKS; i++)
    {
        if (u32Waiters & (1u << i))
        {
            Scheduler_vUnblockTask(i);
        }
    }
}

/// Nachricht entnehmen + Tail weiterschieben. Weckt wartende Sender.
static void prv_DoReceive(OS_Queue_t *queue, void *item)
{
    uint8_t *src = queue->pu8Buffer + (queue->u32Tail * queue->u32ItemSize);
    memcpy(item, src, queue->u32ItemSize);

    queue->u32Tail = (queue->u32Tail + 1u) % queue->u32Capacity;
    queue->u32Count--;

    OS_Trace_Record3(OS_TRACE_EVT_Q_RECV_OK, queue->u8TraceId,
                     OS_Trace_u16Checksum(item, queue->u32ItemSize),
                     queue->u32Count);

    // Es ist jetzt Platz frei -> alle wartenden Sender wecken
    uint32_t u32Waiters = queue->u32SendWaitMask;
    queue->u32SendWaitMask = 0u;
    for (uint8_t i = 0u; i < NUM_TASKS; i++)
    {
        if (u32Waiters & (1u << i))
        {
            Scheduler_vUnblockTask(i);
        }
    }
}

/**
 * Gemeinsamer Kern fuer Send und Receive - gleiches Retry-Muster wie bei
 * Semaphore/Mutex:
 *   Bedingung erfuellt -> Operation ausfuehren -> OS_OK
 *   sonst: Non-Blocking -> OS_WOULD_BLOCK, Timeout abgelaufen -> OS_TIMEOUT,
 *          andernfalls blockieren und nach dem Aufwecken erneut versuchen.
 */
static OS_Result_t prv_Transfer(OS_Queue_t *queue,
                                void *item,
                                uint8_t u8IsSend,
                                uint32_t u32TimeoutTicks,
                                uint8_t u8NonBlocking)
{
    uint8_t  u8Idx  = Scheduler_u8GetCurrentTaskIdx();
    uint32_t u32Bit = (1u << u8Idx);
    uint8_t  u8FirstIteration = 1u;
    uint8_t  u8TraceTask = OS_bInIsrContext() ? OS_TRACE_TASK_ISR : u8Idx;

    for (;;)
    {
        uint32_t u32PriMask = OS_u32EnterCritical();

        if (u8FirstIteration != 0u)
        {
            OS_Trace_Record2(u8IsSend ? OS_TRACE_EVT_Q_SEND_TRY : OS_TRACE_EVT_Q_RECV_TRY,
                             queue->u8TraceId, u8TraceTask);
        }

        // Eigenes Wartebit nur beim Retry loeschen (siehe os_semaphore.c) -
        // schuetzt blockierte Tasks vor NonBlocking-Aufrufen aus ISRs.
        if (u8FirstIteration == 0u)
        {
            if (u8IsSend) { queue->u32SendWaitMask &= ~u32Bit; }
            else          { queue->u32RecvWaitMask &= ~u32Bit; }
        }

        // Operation moeglich?
        if (u8IsSend && (queue->u32Count < queue->u32Capacity))
        {
            prv_DoSend(queue, item);
            OS_vExitCritical(u32PriMask);
            return OS_OK;
        }
        if (!u8IsSend && (queue->u32Count > 0u))
        {
            prv_DoReceive(queue, item);
            OS_vExitCritical(u32PriMask);
            return OS_OK;
        }

        if (u8NonBlocking != 0u)
        {
            OS_Trace_Record2(u8IsSend ? OS_TRACE_EVT_Q_SEND_FULL : OS_TRACE_EVT_Q_RECV_EMPTY,
                             queue->u8TraceId, u8TraceTask);
            OS_vExitCritical(u32PriMask);
            return OS_WOULD_BLOCK;
        }

        if (u32TimeoutTicks != OS_WAIT_FOREVER)
        {
            uint32_t u32Remaining = u8FirstIteration ? u32TimeoutTicks
                                                     : Scheduler_u32GetRemainingDelay(u8Idx);
            if (u32Remaining == 0u)
            {
                OS_Trace_Record2(u8IsSend ? OS_TRACE_EVT_Q_SEND_TIMEOUT : OS_TRACE_EVT_Q_RECV_TIMEOUT,
                                 queue->u8TraceId, u8Idx);
                OS_vExitCritical(u32PriMask);
                return OS_TIMEOUT;
            }
            if (u8IsSend) { queue->u32SendWaitMask |= u32Bit; }
            else          { queue->u32RecvWaitMask |= u32Bit; }
            OS_Trace_Record3(u8IsSend ? OS_TRACE_EVT_Q_SEND_BLOCK : OS_TRACE_EVT_Q_RECV_BLOCK,
                             queue->u8TraceId, u8Idx, u32Remaining);
            Scheduler_vBlockCurrentTask(u32Remaining, QUEUE_SYSVIEW_CAUSE);
        }
        else
        {
            if (u8IsSend) { queue->u32SendWaitMask |= u32Bit; }
            else          { queue->u32RecvWaitMask |= u32Bit; }
            OS_Trace_Record3(u8IsSend ? OS_TRACE_EVT_Q_SEND_BLOCK : OS_TRACE_EVT_Q_RECV_BLOCK,
                             queue->u8TraceId, u8Idx, OS_WAIT_FOREVER);
            Scheduler_vBlockCurrentTask(OS_WAIT_FOREVER, QUEUE_SYSVIEW_CAUSE);
        }

        u8FirstIteration = 0u;
        OS_vExitCritical(u32PriMask);

        Scheduler_vWaitWhileBlocked();
    }
}

/* ==========================================================================
 * Oeffentliche API
 * ========================================================================== */

OS_Result_t OS_Queue_SendNonBlocking(OS_Queue_t *queue, const void *item)
{
    return prv_Transfer(queue, (void *)item, 1u, 0u, 1u);
}

OS_Result_t OS_Queue_SendBlocking(OS_Queue_t *queue, const void *item)
{
    return prv_Transfer(queue, (void *)item, 1u, OS_WAIT_FOREVER, 0u);
}

OS_Result_t OS_Queue_SendTimeout(OS_Queue_t *queue, const void *item, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Queue_SendNonBlocking(queue, item);
    }
    return prv_Transfer(queue, (void *)item, 1u, u32TimeoutTicks, 0u);
}

OS_Result_t OS_Queue_ReceiveNonBlocking(OS_Queue_t *queue, void *item)
{
    return prv_Transfer(queue, item, 0u, 0u, 1u);
}

OS_Result_t OS_Queue_ReceiveBlocking(OS_Queue_t *queue, void *item)
{
    return prv_Transfer(queue, item, 0u, OS_WAIT_FOREVER, 0u);
}

OS_Result_t OS_Queue_ReceiveTimeout(OS_Queue_t *queue, void *item, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Queue_ReceiveNonBlocking(queue, item);
    }
    return prv_Transfer(queue, item, 0u, u32TimeoutTicks, 0u);
}

bool OS_Queue_IsEmpty(const OS_Queue_t *queue)
{
    return (queue->u32Count == 0u);
}

bool OS_Queue_IsFull(const OS_Queue_t *queue)
{
    return (queue->u32Count >= queue->u32Capacity);
}
