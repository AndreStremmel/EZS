/*
 * os_common.h
 *  Gemeinsame Typen & Critical-Section-Helfer fuer die Kernel-Objekte
 *  (Semaphore, Mutex, Queue)
 */

#ifndef OS_COMMON_H
#define OS_COMMON_H

#include <stdint.h>
#include "stm32f3xx_hal.h"   /* fuer __get_PRIMASK / __disable_irq */

/// Einheitliche Rueckgabewerte aller Kernel-Objekte
typedef enum
{
    OS_OK = 0,       ///< Operation erfolgreich
    OS_WOULD_BLOCK,  ///< Non-Blocking: Ressource nicht verfuegbar / Queue voll bzw. leer
    OS_TIMEOUT,      ///< Timeout abgelaufen, Ressource nicht bekommen
} OS_Result_t;

/// Warte-Ewigkeit fuer Blocking-Varianten (intern)
#define OS_WAIT_FOREVER  ( 0xFFFFFFFFu )

/**
 * @brief Kritischen Abschnitt betreten (Interrupts sperren).
 * @return alter PRIMASK-Wert, muss an OS_ExitCritical() uebergeben werden.
 *
 * PRIMASK wird gesichert, damit verschachtelte kritische Abschnitte
 * (z.B. Aufruf aus einer ISR, in der Interrupts schon gesperrt sind)
 * den Zustand nicht kaputt machen.
 */
static inline uint32_t OS_u32EnterCritical(void)
{
    uint32_t u32PriMask = __get_PRIMASK();
    __disable_irq();
    return u32PriMask;
}

/**
 * @brief Kritischen Abschnitt verlassen (alten Interrupt-Zustand wiederherstellen).
 */
static inline void OS_vExitCritical(uint32_t u32PriMask)
{
    __set_PRIMASK(u32PriMask);
}

/**
 * @brief Prueft, ob der Code gerade im ISR-Kontext laeuft (IPSR != 0).
 *        Wird u.a. genutzt, um Trace-Events aus ISRs korrekt zu markieren.
 */
static inline uint8_t OS_bInIsrContext(void)
{
    return (__get_IPSR() != 0u) ? 1u : 0u;
}

#endif /* OS_COMMON_H */
