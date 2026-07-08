#ifndef OS_SEMAPHORE_H
#define OS_SEMAPHORE_H

#include <stdint.h>
#include "os_common.h"

typedef struct
{
    volatile uint8_t u8Count;
    uint8_t u8MaxCount;
    uint8_t u8TraceId;            ///< ID fuer die TeSSLa-Instrumentierung

    /// Bitmaske der Tasks, die auf diese Semaphore warten
    volatile uint32_t u32WaitMask;
} OS_Semaphore_t;

void OS_Semaphore_Init(OS_Semaphore_t *sem, uint8_t initialCount, uint8_t maxCount,
                       uint8_t u8TraceId);

/**
 * @brief Non-Blocking Acquire: Versucht die Semaphore zu nehmen und kehrt
 *        sofort zurueck.
 * @return OS_OK oder OS_WOULD_BLOCK
 */
OS_Result_t OS_Semaphore_TakeNonBlocking(OS_Semaphore_t *sem);

/**
 * @brief Blocking Acquire: Task geht in den Blocked-State, bis die Semaphore
 *        verfuegbar ist (unendliches Warten).
 * @return OS_OK
 */
OS_Result_t OS_Semaphore_TakeBlocking(OS_Semaphore_t *sem);

/**
 * @brief Acquire mit Timeout: Task geht fuer max. u32TimeoutTicks in den
 *        Blocked-State. Er wird geweckt, wenn die Semaphore freigegeben
 *        wird ODER die Zeit ablaeuft.
 * @return OS_OK oder OS_TIMEOUT
 */
OS_Result_t OS_Semaphore_TakeTimeout(OS_Semaphore_t *sem, uint32_t u32TimeoutTicks);

/**
 * @brief Release: Zaehler erhoehen (bis MaxCount) und alle wartenden Tasks
 *        aufwecken. ISR-sicher.
 */
void OS_Semaphore_Give(OS_Semaphore_t *sem);

#endif
