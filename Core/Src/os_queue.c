#include "os_queue.h"
#include <string.h>

void OS_Queue_Init(OS_Queue_t *queue,
                   uint8_t *buffer,
                   uint32_t itemSize,
                   uint32_t capacity)
{
    queue->pu8Buffer = buffer;
    queue->u32ItemSize = itemSize;
    queue->u32Capacity = capacity;

    queue->u32Head = 0;
    queue->u32Tail = 0;
    queue->u32Count = 0;
}

OS_QueueResult_t OS_Queue_Send(OS_Queue_t *queue, const void *item)
{
    if (queue->u32Count >= queue->u32Capacity)
    {
        return OS_QUEUE_FULL;
    }

    uint8_t *dest = queue->pu8Buffer + (queue->u32Head * queue->u32ItemSize);

    memcpy(dest, item, queue->u32ItemSize);

    queue->u32Head = (queue->u32Head + 1) % queue->u32Capacity;
    queue->u32Count++;

    return OS_QUEUE_OK;
}

OS_QueueResult_t OS_Queue_Receive(OS_Queue_t *queue, void *item)
{
    if (queue->u32Count == 0)
    {
        return OS_QUEUE_EMPTY;
    }

    uint8_t *src = queue->pu8Buffer + (queue->u32Tail * queue->u32ItemSize);

    memcpy(item, src, queue->u32ItemSize);

    queue->u32Tail = (queue->u32Tail + 1) % queue->u32Capacity;
    queue->u32Count--;

    return OS_QUEUE_OK;
}