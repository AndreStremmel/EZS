#ifndef OS_QUEUE_H
#define OS_QUEUE_H

#include <stdint.h>
#include <stddef.h>

typedef enum
{
    OS_QUEUE_OK = 0,
    OS_QUEUE_FULL,
    OS_QUEUE_EMPTY
} OS_QueueResult_t;

typedef struct
{
    uint8_t *pu8Buffer;

    uint32_t u32ItemSize;
    uint32_t u32Capacity;

    uint32_t u32Head;
    uint32_t u32Tail;
    uint32_t u32Count;

} OS_Queue_t;

void OS_Queue_Init(OS_Queue_t *queue,
                   uint8_t *buffer,
                   uint32_t itemSize,
                   uint32_t capacity);

OS_QueueResult_t OS_Queue_Send(OS_Queue_t *queue, const void *item);
OS_QueueResult_t OS_Queue_Receive(OS_Queue_t *queue, void *item);

#endif