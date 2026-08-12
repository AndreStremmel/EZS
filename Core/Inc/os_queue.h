/**
 ******************************************************************************
 * @file    os_queue.h
 * @brief   Fixed-size message queue (ring buffer) - API.
 * @author  Andre
 ******************************************************************************
 *
 * The queue carries messages by value: every send copies u32ItemSize bytes
 * into the internal ring buffer, every receive copies them back out. This
 * avoids any lifetime questions about the sender's local variables, which is
 * exactly what the producer/consumer chain of this project needs:
 *
 *   SensorTask --SensorData_t--> g_sensorQueue    --> ProcTask
 *   ProcTask   --ProcessedData_t--> g_processedQueue --> UartShellTask
 *
 * The storage is supplied by the caller at init time, so the kernel needs no
 * dynamic memory allocation at all.
 *
 * Two separate wait masks are kept: one for senders blocked on a full queue
 * and one for receivers blocked on an empty queue. Keeping them apart means a
 * send only wakes receivers and a receive only wakes senders, instead of
 * waking everybody on every operation.
 *
 ******************************************************************************
 */

#ifndef OS_QUEUE_H
#define OS_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "os_common.h"

/** @brief Queue control block. Initialise with OS_Queue_Init() before use. */
typedef struct
{
    uint8_t *pu8Buffer;           ///< Caller-provided storage, itemSize*capacity bytes

    uint32_t u32ItemSize;         ///< Size of a single message in bytes
    uint32_t u32Capacity;         ///< Maximum number of messages held at once

    volatile uint32_t u32Head;    ///< Write index (next free slot)
    volatile uint32_t u32Tail;    ///< Read index (oldest message)
    volatile uint32_t u32Count;   ///< Number of messages currently stored

    uint8_t u8TraceId;            ///< ID used by the TeSSLa instrumentation

    /// Tasks waiting for free space (send on a full queue)
    volatile uint32_t u32SendWaitMask;
    /// Tasks waiting for a message (receive from an empty queue)
    volatile uint32_t u32RecvWaitMask;

} OS_Queue_t;

/**
 * @brief Initialise a queue over a caller-provided buffer.
 * @param queue     Queue to initialise.
 * @param buffer    Storage area, must be at least itemSize * capacity bytes.
 * @param itemSize  Size of one message in bytes.
 * @param capacity  Number of messages the buffer can hold.
 * @param u8TraceId ID reported in the trace events of this object.
 * @author Andre
 */
void OS_Queue_Init(OS_Queue_t *queue,
                   uint8_t *buffer,
                   uint32_t itemSize,
                   uint32_t capacity,
                   uint8_t u8TraceId);

/* ---- Send ---------------------------------------------------------------- */

/**
 * @brief  Non-blocking send: enqueue a message and return immediately.
 * @param  queue Target queue.
 * @param  item  Message to copy into the queue (itemSize bytes are read).
 * @return OS_OK on success, OS_WOULD_BLOCK if the queue is full.
 * @author Andre
 */
OS_Result_t OS_Queue_SendNonBlocking(OS_Queue_t *queue, const void *item);

/**
 * @brief  Blocking send: block until there is room, then enqueue.
 * @param  queue Target queue.
 * @param  item  Message to copy into the queue (itemSize bytes are read).
 * @return Always OS_OK.
 * @author Andre
 */
OS_Result_t OS_Queue_SendBlocking(OS_Queue_t *queue, const void *item);

/**
 * @brief  Send with timeout: wait at most u32TimeoutTicks for free space.
 * @param  queue           Target queue.
 * @param  item            Message to copy into the queue.
 * @param  u32TimeoutTicks Maximum wait time in SysTick ticks.
 * @return OS_OK if the message was enqueued, OS_TIMEOUT otherwise.
 * @author Andre
 */
OS_Result_t OS_Queue_SendTimeout(OS_Queue_t *queue, const void *item, uint32_t u32TimeoutTicks);

/* ---- Receive ------------------------------------------------------------- */

/**
 * @brief  Non-blocking receive: dequeue a message and return immediately.
 * @param  queue Source queue.
 * @param  item  Destination buffer, receives itemSize bytes.
 * @return OS_OK on success, OS_WOULD_BLOCK if the queue is empty.
 * @author Andre
 */
OS_Result_t OS_Queue_ReceiveNonBlocking(OS_Queue_t *queue, void *item);

/**
 * @brief  Blocking receive: block until a message arrives, then dequeue it.
 * @param  queue Source queue.
 * @param  item  Destination buffer, receives itemSize bytes.
 * @return Always OS_OK.
 * @author Andre
 */
OS_Result_t OS_Queue_ReceiveBlocking(OS_Queue_t *queue, void *item);

/**
 * @brief  Receive with timeout: wait at most u32TimeoutTicks for a message.
 * @param  queue           Source queue.
 * @param  item            Destination buffer, receives itemSize bytes.
 * @param  u32TimeoutTicks Maximum wait time in SysTick ticks.
 * @return OS_OK if a message was dequeued, OS_TIMEOUT otherwise.
 * @author Andre
 *
 * Used by UartShellTask so it can keep polling the shell input even while no
 * measurement results are arriving.
 */
OS_Result_t OS_Queue_ReceiveTimeout(OS_Queue_t *queue, void *item, uint32_t u32TimeoutTicks);

/* ---- Status -------------------------------------------------------------- */

/**
 * @brief  Check whether the queue currently holds no messages.
 * @param  queue Queue to inspect.
 * @return true if empty, false otherwise.
 * @author Andre
 */
bool OS_Queue_IsEmpty(const OS_Queue_t *queue);

/**
 * @brief  Check whether the queue has reached its capacity.
 * @param  queue Queue to inspect.
 * @return true if full, false otherwise.
 * @author Andre
 */
bool OS_Queue_IsFull(const OS_Queue_t *queue);

#endif
