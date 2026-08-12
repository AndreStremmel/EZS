/**
 ******************************************************************************
 * @file    os_queue.c
 * @brief   Fixed-size message queue (ring buffer) - implementation.
 * @author  Andre
 ******************************************************************************
 *
 * Send and receive share one implementation in prv_Transfer(), parameterised by
 * the u8IsSend flag. Both directions follow the same retry pattern as the
 * semaphore and the mutex: check the condition inside a critical section, and
 * treat a wake-up as "try again" rather than as a guarantee.
 *
 * Two separate wait masks are maintained so a send only wakes receivers and a
 * receive only wakes senders. Waking the wrong side would cost a pointless
 * context switch on every operation.
 *
 * Every completed transfer logs a checksum over the payload. Together with the
 * message count this lets queue.tessla verify FIFO order and data integrity
 * from the trace alone.
 *
 * @see os_queue.h for the API description.
 *
 ******************************************************************************
 */

#include "os_queue.h"
#include "scheduler.h"
#include "os_trace.h"
#include <string.h>

/** @brief SystemView "blocked on queue" cause code. */
#define QUEUE_SYSVIEW_CAUSE  ( 3u )

/**
 * @brief Initialise a queue over a caller-provided buffer.
 * @param queue     Queue to initialise.
 * @param buffer    Storage area, at least itemSize * capacity bytes.
 * @param itemSize  Size of one message in bytes.
 * @param capacity  Number of messages the buffer can hold.
 * @param u8TraceId ID reported in the trace events of this object.
 * @author __________
 */
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
 * Internal helpers (must be called from within a critical section!)
 * ========================================================================== */

/**
 * @brief Copy a message into the buffer, advance head and wake receivers.
 * @param queue Target queue, must have free space.
 * @param item  Message to copy in.
 * @author Berkay
 *
 * @warning Caller must hold a critical section and must have verified that
 *          the queue is not full.
 */
static void prv_DoSend(OS_Queue_t *queue, const void *item)
{
    uint8_t *dest = queue->pu8Buffer + (queue->u32Head * queue->u32ItemSize);
    memcpy(dest, item, queue->u32ItemSize);

    queue->u32Head = (queue->u32Head + 1u) % queue->u32Capacity;
    queue->u32Count++;

    OS_TRACE_Q3(OS_TRACE_EVT_Q_SEND_OK, queue->u8TraceId,
                     OS_Trace_u16Checksum(item, queue->u32ItemSize),
                     queue->u32Count);

    // A message is available now -> wake every waiting receiver
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

/**
 * @brief Copy a message out of the buffer, advance tail and wake senders.
 * @param queue Source queue, must not be empty.
 * @param item  Destination buffer for the message.
 * @author Berkay
 *
 * @warning Caller must hold a critical section and must have verified that
 *          the queue is not empty.
 */
static void prv_DoReceive(OS_Queue_t *queue, void *item)
{
    uint8_t *src = queue->pu8Buffer + (queue->u32Tail * queue->u32ItemSize);
    memcpy(item, src, queue->u32ItemSize);

    queue->u32Tail = (queue->u32Tail + 1u) % queue->u32Capacity;
    queue->u32Count--;

    OS_TRACE_Q3(OS_TRACE_EVT_Q_RECV_OK, queue->u8TraceId,
                     OS_Trace_u16Checksum(item, queue->u32ItemSize),
                     queue->u32Count);

    // A slot is free now -> wake every waiting sender
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
 * @brief  Shared core of all send and receive variants.
 * @param  queue           Queue to operate on.
 * @param  item            Source (send) or destination (receive) buffer.
 * @param  u8IsSend        1 = send, 0 = receive.
 * @param  u32TimeoutTicks Timeout in ticks, or OS_WAIT_FOREVER.
 * @param  u8NonBlocking   1 = return OS_WOULD_BLOCK instead of blocking.
 * @return OS_OK, OS_WOULD_BLOCK or OS_TIMEOUT.
 * @author Berkay
 *
 * Same retry pattern as the semaphore and mutex:
 *   condition satisfied -> perform the operation -> OS_OK
 *   otherwise: non-blocking -> OS_WOULD_BLOCK, timeout expired -> OS_TIMEOUT,
 *              else block and retry after being woken up.
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
    (void)u8TraceTask;   /* used by the trace macros only - see os_trace_config.h */

    for (;;)
    {
        uint32_t u32PriMask = OS_u32EnterCritical();

        if (u8FirstIteration != 0u)
        {
            OS_TRACE_Q_TRY2(u8IsSend ? OS_TRACE_EVT_Q_SEND_TRY : OS_TRACE_EVT_Q_RECV_TRY,
                             queue->u8TraceId, u8TraceTask);
        }

        // Clear our own wait bit only on a retry (see os_semaphore.c) - this
        // protects blocked tasks from non-blocking calls made inside ISRs.
        if (u8FirstIteration == 0u)
        {
            if (u8IsSend) { queue->u32SendWaitMask &= ~u32Bit; }
            else          { queue->u32RecvWaitMask &= ~u32Bit; }
        }

        // Can the operation proceed?
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
            OS_TRACE_Q2(u8IsSend ? OS_TRACE_EVT_Q_SEND_FULL : OS_TRACE_EVT_Q_RECV_EMPTY,
                             queue->u8TraceId, u8TraceTask);
            OS_vExitCritical(u32PriMask);
            return OS_WOULD_BLOCK;
        }

        if (u32TimeoutTicks != OS_WAIT_FOREVER)
        {
            // On a retry the remaining time comes from the scheduler countdown,
            // so the timeout is not restarted on every wake-up.
            uint32_t u32Remaining = u8FirstIteration ? u32TimeoutTicks
                                                     : Scheduler_u32GetRemainingDelay(u8Idx);
            if (u32Remaining == 0u)
            {
                OS_TRACE_Q2(u8IsSend ? OS_TRACE_EVT_Q_SEND_TIMEOUT : OS_TRACE_EVT_Q_RECV_TIMEOUT,
                                 queue->u8TraceId, u8Idx);
                OS_vExitCritical(u32PriMask);
                return OS_TIMEOUT;
            }
            if (u8IsSend) { queue->u32SendWaitMask |= u32Bit; }
            else          { queue->u32RecvWaitMask |= u32Bit; }
            OS_TRACE_Q3(u8IsSend ? OS_TRACE_EVT_Q_SEND_BLOCK : OS_TRACE_EVT_Q_RECV_BLOCK,
                             queue->u8TraceId, u8Idx, u32Remaining);
            Scheduler_vBlockCurrentTask(u32Remaining, QUEUE_SYSVIEW_CAUSE);
        }
        else
        {
            if (u8IsSend) { queue->u32SendWaitMask |= u32Bit; }
            else          { queue->u32RecvWaitMask |= u32Bit; }
            OS_TRACE_Q3(u8IsSend ? OS_TRACE_EVT_Q_SEND_BLOCK : OS_TRACE_EVT_Q_RECV_BLOCK,
                             queue->u8TraceId, u8Idx, OS_WAIT_FOREVER);
            Scheduler_vBlockCurrentTask(OS_WAIT_FOREVER, QUEUE_SYSVIEW_CAUSE);
        }

        u8FirstIteration = 0u;
        OS_vExitCritical(u32PriMask);

        Scheduler_vWaitWhileBlocked();
    }
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

/**
 * @brief  Non-blocking send.
 * @param  queue Target queue.
 * @param  item  Message to enqueue.
 * @return OS_OK or OS_WOULD_BLOCK (queue full).
 * @author Andre
 */
OS_Result_t OS_Queue_SendNonBlocking(OS_Queue_t *queue, const void *item)
{
    return prv_Transfer(queue, (void *)item, 1u, 0u, 1u);
}

/**
 * @brief  Blocking send: wait until there is room.
 * @param  queue Target queue.
 * @param  item  Message to enqueue.
 * @return Always OS_OK.
 * @author Andre
 */
OS_Result_t OS_Queue_SendBlocking(OS_Queue_t *queue, const void *item)
{
    return prv_Transfer(queue, (void *)item, 1u, OS_WAIT_FOREVER, 0u);
}

/**
 * @brief  Send with timeout.
 * @param  queue           Target queue.
 * @param  item            Message to enqueue.
 * @param  u32TimeoutTicks Maximum wait time in ticks.
 * @return OS_OK or OS_TIMEOUT.
 * @author Berkay
 */
OS_Result_t OS_Queue_SendTimeout(OS_Queue_t *queue, const void *item, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Queue_SendNonBlocking(queue, item);
    }
    return prv_Transfer(queue, (void *)item, 1u, u32TimeoutTicks, 0u);
}

/**
 * @brief  Non-blocking receive.
 * @param  queue Source queue.
 * @param  item  Destination buffer.
 * @return OS_OK or OS_WOULD_BLOCK (queue empty).
 * @author Berkay
 */
OS_Result_t OS_Queue_ReceiveNonBlocking(OS_Queue_t *queue, void *item)
{
    return prv_Transfer(queue, item, 0u, 0u, 1u);
}

/**
 * @brief  Blocking receive: wait until a message arrives.
 * @param  queue Source queue.
 * @param  item  Destination buffer.
 * @return Always OS_OK.
 * @author Berkay
 */
OS_Result_t OS_Queue_ReceiveBlocking(OS_Queue_t *queue, void *item)
{
    return prv_Transfer(queue, item, 0u, OS_WAIT_FOREVER, 0u);
}

/**
 * @brief  Receive with timeout.
 * @param  queue           Source queue.
 * @param  item            Destination buffer.
 * @param  u32TimeoutTicks Maximum wait time in ticks.
 * @return OS_OK or OS_TIMEOUT.
 * @author Berkay
 */
OS_Result_t OS_Queue_ReceiveTimeout(OS_Queue_t *queue, void *item, uint32_t u32TimeoutTicks)
{
    if (u32TimeoutTicks == 0u)
    {
        return OS_Queue_ReceiveNonBlocking(queue, item);
    }
    return prv_Transfer(queue, item, 0u, u32TimeoutTicks, 0u);
}

/**
 * @brief  Check whether the queue currently holds no messages.
 * @param  queue Queue to inspect.
 * @return true if empty.
 * @author Andre
 */
bool OS_Queue_IsEmpty(const OS_Queue_t *queue)
{
    return (queue->u32Count == 0u);
}

/**
 * @brief  Check whether the queue has reached its capacity.
 * @param  queue Queue to inspect.
 * @return true if full.
 * @author Andre
 */
bool OS_Queue_IsFull(const OS_Queue_t *queue)
{
    return (queue->u32Count >= queue->u32Capacity);
}
