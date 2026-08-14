/**
 ******************************************************************************
 * @file    os_common.h
 * @brief   Shared types and critical-section helpers for the kernel objects
 *          (semaphore, mutex, queue).
 * @author  Berkay
 ******************************************************************************
 *
 * Every kernel object in this RTOS reports its outcome through the same
 * OS_Result_t enum, so callers can handle blocking, non-blocking and timeout
 * variants uniformly.
 *
 * The critical-section helpers are inline because they are used on the hot
 * path of every take/give operation. They save and restore PRIMASK rather
 * than blindly re-enabling interrupts, which makes them safe to nest.
 *
 ******************************************************************************
 */

#ifndef OS_COMMON_H
#define OS_COMMON_H

#include <stdint.h>
#include "stm32l4xx_hal.h"   /* for __get_PRIMASK / __disable_irq */

/** @brief Common return codes shared by all kernel objects. */
typedef enum
{
    OS_OK = 0,       ///< Operation completed successfully
    OS_WOULD_BLOCK,  ///< Non-blocking call: resource busy / queue full or empty
    OS_TIMEOUT,      ///< Timeout expired before the resource became available
} OS_Result_t;

/** @brief Timeout value meaning "wait indefinitely" (used internally). */
#define OS_WAIT_FOREVER  ( 0xFFFFFFFFu )

/**
 * @brief  Enter a critical section by disabling interrupts.
 * @return Previous PRIMASK value; must be passed back to OS_vExitCritical().
 * @author Berkay
 *
 * PRIMASK is saved instead of simply enabling interrupts on exit, so that
 * nested critical sections (e.g. a call made from an ISR where interrupts are
 * already masked) do not corrupt the interrupt state of the outer section.
 */
static inline uint32_t OS_u32EnterCritical(void)
{
    uint32_t u32PriMask = __get_PRIMASK();
    __disable_irq();
    return u32PriMask;
}

/**
 * @brief  Leave a critical section by restoring the previous interrupt state.
 * @param  u32PriMask PRIMASK value returned by OS_u32EnterCritical().
 * @author Berkay
 */
static inline void OS_vExitCritical(uint32_t u32PriMask)
{
    __set_PRIMASK(u32PriMask);
}

/**
 * @brief  Check whether the code is currently executing in ISR context.
 * @return 1 if running inside an exception handler (IPSR != 0), 0 otherwise.
 * @author Berkay
 *
 * Used among other things to tag trace events raised from ISRs correctly,
 * since there is no "current task" to attribute them to.
 */
static inline uint8_t OS_bInIsrContext(void)
{
    return (__get_IPSR() != 0u) ? 1u : 0u;
}

#endif /* OS_COMMON_H */
