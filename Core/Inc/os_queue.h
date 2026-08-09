#ifndef OS_QUEUE_H
#define OS_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "os_common.h"

typedef struct
{
    uint8_t *pu8Buffer;

    uint32_t u32ItemSize;
    uint32_t u32Capacity;

    volatile uint32_t u32Head;
    volatile uint32_t u32Tail;
    volatile uint32_t u32Count;

    uint8_t u8TraceId;            ///< ID fuer die TeSSLa-Instrumentierung

    /// Tasks, die auf freien Platz warten (Send an volle Queue)
    volatile uint32_t u32SendWaitMask;
    /// Tasks, die auf Nachrichten warten (Receive aus leerer Queue)
    volatile uint32_t u32RecvWaitMask;

} OS_Queue_t;

void OS_Queue_Init(OS_Queue_t *queue,
                   uint8_t *buffer,
                   uint32_t itemSize,
                   uint32_t capacity,
                   uint8_t u8TraceId);

/* ---- Send ---------------------------------------------------------------- */

/// Non-Blocking Send: OS_OK oder OS_WOULD_BLOCK (Queue voll)
OS_Result_t OS_Queue_SendNonBlocking(OS_Queue_t *queue, const void *item);

/// Blocking Send: blockiert, bis Platz in der Queue ist. Immer OS_OK.
OS_Result_t OS_Queue_SendBlocking(OS_Queue_t *queue, const void *item);

/// Send mit Timeout: OS_OK oder OS_TIMEOUT
OS_Result_t OS_Queue_SendTimeout(OS_Queue_t *queue, const void *item, uint32_t u32TimeoutTicks);

/* ---- Receive ------------------------------------------------------------- */

/// Non-Blocking Receive: OS_OK oder OS_WOULD_BLOCK (Queue leer)
OS_Result_t OS_Queue_ReceiveNonBlocking(OS_Queue_t *queue, void *item);

/// Blocking Receive: blockiert, bis eine Nachricht da ist. Immer OS_OK.
OS_Result_t OS_Queue_ReceiveBlocking(OS_Queue_t *queue, void *item);

/// Receive mit Timeout: OS_OK oder OS_TIMEOUT
OS_Result_t OS_Queue_ReceiveTimeout(OS_Queue_t *queue, void *item, uint32_t u32TimeoutTicks);

/* ---- Status -------------------------------------------------------------- */

bool OS_Queue_IsEmpty(const OS_Queue_t *queue);
bool OS_Queue_IsFull(const OS_Queue_t *queue);

#endif
