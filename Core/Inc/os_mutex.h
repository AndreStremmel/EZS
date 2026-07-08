#ifndef OS_MUTEX_H
#define OS_MUTEX_H

#include <stdint.h>
#include "os_common.h"

typedef struct
{
    volatile uint8_t u8Locked;
    uint8_t u8OwnerTaskId;        ///< TaskId (1-basiert) des Besitzers, 0 = frei
    uint8_t u8TraceId;            ///< ID fuer die TeSSLa-Instrumentierung

    /// Bitmaske der Tasks, die auf diesen Mutex warten
    volatile uint32_t u32WaitMask;
} OS_Mutex_t;

void OS_Mutex_Init(OS_Mutex_t *mutex, uint8_t u8TraceId);

/**
 * @brief Non-Blocking Lock: Versucht den Mutex zu nehmen, kehrt sofort zurueck.
 *        Besitzer wird automatisch der aktuell laufende Task.
 * @return OS_OK oder OS_WOULD_BLOCK
 */
OS_Result_t OS_Mutex_LockNonBlocking(OS_Mutex_t *mutex);

/**
 * @brief Blocking Lock: Task blockiert, bis der Mutex frei wird.
 * @return OS_OK
 */
OS_Result_t OS_Mutex_LockBlocking(OS_Mutex_t *mutex);

/**
 * @brief Lock mit Timeout: Task blockiert max. u32TimeoutTicks. Er wird
 *        geweckt, wenn der Mutex freigegeben wird ODER die Zeit ablaeuft.
 * @return OS_OK oder OS_TIMEOUT
 */
OS_Result_t OS_Mutex_LockTimeout(OS_Mutex_t *mutex, uint32_t u32TimeoutTicks);

/**
 * @brief Unlock: Nur der Besitzer darf freigeben. Weckt alle Wartenden.
 */
void OS_Mutex_Unlock(OS_Mutex_t *mutex);

#endif
