/**
 ******************************************************************************
 * @file    os_trace.h
 * @brief   SystemView instrumentation for the TeSSLa verification - API.
 * @author  Berkay
 ******************************************************************************
 *
 * Registers a dedicated SystemView module "DOSRTOS" with named events for the
 * mutex, semaphore, queue, delay and application layers. The converter
 * (tools/sysview_to_tessla.py) turns the SystemView text export of these
 * events into TeSSLa input streams.
 *
 * The resource IDs below are what ties a runtime event back to a concrete
 * kernel object in the specification, so they must stay in sync with the
 * converter script.
 *
 ******************************************************************************
 */

#ifndef OS_TRACE_H
#define OS_TRACE_H

#include <stdint.h>
#include <stddef.h>

#include "SEGGER_SYSVIEW.h"
#include "os_trace_config.h"

/* --------------------------------------------------------------------------
 * Resource IDs (must match tools/sysview_to_tessla.py!)
 * -------------------------------------------------------------------------- */
#define OS_TRACE_MTX_UART      ( 1u )   ///< Trace ID of g_uartMutex
#define OS_TRACE_MTX_CONFIG    ( 2u )   ///< Trace ID of g_configMutex

#define OS_TRACE_SEM_ECHO      ( 1u )   ///< Trace ID of g_echoDoneSemaphore
#define OS_TRACE_SEM_UARTRX    ( 2u )   ///< Trace ID of g_uartRxSemaphore

#define OS_TRACE_Q_SENSOR      ( 1u )   ///< Trace ID of g_sensorQueue
#define OS_TRACE_Q_PROCESSED   ( 2u )   ///< Trace ID of g_processedQueue

/** @brief Pseudo task ID used for events raised from ISR context. */
#define OS_TRACE_TASK_ISR      ( 255u )

/* --------------------------------------------------------------------------
 * Module event indices (the runtime offset is assigned by SystemView)
 * The order MUST match the description string in os_trace.c!
 * -------------------------------------------------------------------------- */
/**
 * @brief Event IDs of the custom SystemView module.
 *
 * The comment after each entry lists the parameters that are logged with it.
 */
typedef enum
{
    OS_TRACE_EVT_MTX_LOCK_TRY = 0,   // Mtx, Task
    OS_TRACE_EVT_MTX_LOCK_OK,        // Mtx, Task
    OS_TRACE_EVT_MTX_LOCK_BLOCK,     // Mtx, Task, ticks (0xFFFFFFFF = forever)
    OS_TRACE_EVT_MTX_LOCK_TIMEOUT,   // Mtx, Task
    OS_TRACE_EVT_MTX_UNLOCK_OK,      // Mtx, Task
    OS_TRACE_EVT_MTX_UNLOCK_DENIED,  // Mtx, Task (caller is not the owner!)

    OS_TRACE_EVT_SEM_TAKE_TRY,       // Sem, Task
    OS_TRACE_EVT_SEM_TAKE_OK,        // Sem, Task, count afterwards
    OS_TRACE_EVT_SEM_TAKE_BLOCK,     // Sem, Task, ticks
    OS_TRACE_EVT_SEM_TAKE_TIMEOUT,   // Sem, Task
    OS_TRACE_EVT_SEM_GIVE_OK,        // Sem, Task, count afterwards
    OS_TRACE_EVT_SEM_GIVE_IGNORED,   // Sem, Task (semaphore was already full)

    OS_TRACE_EVT_Q_SEND_TRY,         // Q, Task
    OS_TRACE_EVT_Q_SEND_OK,          // Q, checksum, count afterwards
    OS_TRACE_EVT_Q_SEND_FULL,        // Q, Task
    OS_TRACE_EVT_Q_SEND_BLOCK,       // Q, Task, ticks
    OS_TRACE_EVT_Q_SEND_TIMEOUT,     // Q, Task
    OS_TRACE_EVT_Q_RECV_TRY,         // Q, Task
    OS_TRACE_EVT_Q_RECV_OK,          // Q, checksum, count afterwards
    OS_TRACE_EVT_Q_RECV_EMPTY,       // Q, Task
    OS_TRACE_EVT_Q_RECV_BLOCK,       // Q, Task, ticks
    OS_TRACE_EVT_Q_RECV_TIMEOUT,     // Q, Task

    OS_TRACE_EVT_DELAY_START,        // Task, ticks   (non-blocking delay)
    OS_TRACE_EVT_BUSY_DELAY_START,   // Task, ticks   (busy-wait delay)
    OS_TRACE_EVT_BUSY_DELAY_END,     // Task

    OS_TRACE_EVT_TASK_MAP,           // task index, TCB address (for the converter)
    OS_TRACE_EVT_UART_TX_DIST,       // distance in mm
    OS_TRACE_EVT_SENSOR_ERR,         // error code
    OS_TRACE_EVT_CAL_START,          // target distance in mm
    OS_TRACE_EVT_CAL_DONE,           // offset (cast to u32)

    OS_TRACE_EVT__COUNT              ///< Number of events, not an event itself
} OS_TraceEvent_t;

/** @brief Sensor error: no echo received within the timeout. */
#define SENSOR_ERR_NO_ECHO       ( 1u )
/** @brief Sensor error: pulse width outside the plausible range. */
#define SENSOR_ERR_OUT_OF_RANGE  ( 2u )

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */

/**
 * @brief Register the custom module with SystemView and send the task info.
 * @author Berkay
 *
 * @note Call AFTER SEGGER_SYSVIEW_Conf() and BEFORE Scheduler_vInit().
 */
void OS_Trace_Init(void);

/**
 * @brief Send the task list (create + info + task map) to SystemView.
 * @author Berkay
 *
 * @note Must also be registered as the pfSendTaskList callback in the
 *       SEGGER_SYSVIEW configuration, so a host connecting mid-run still
 *       receives the task names.
 */
void OS_Trace_vSendTaskList(void);

/**
 * @brief Emit a raw module event with one parameter.
 * @param evt Event ID.
 * @param p0  First parameter.
 * @author Berkay
 */
void OS_Trace_Record1(OS_TraceEvent_t evt, uint32_t p0);

/**
 * @brief Emit a raw module event with two parameters.
 * @param evt Event ID.
 * @param p0  First parameter.
 * @param p1  Second parameter.
 * @author Berkay
 */
void OS_Trace_Record2(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1);

/**
 * @brief Emit a raw module event with three parameters.
 * @param evt Event ID.
 * @param p0  First parameter.
 * @param p1  Second parameter.
 * @param p2  Third parameter.
 * @author Berkay
 */
void OS_Trace_Record3(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1, uint32_t p2);

/**
 * @brief  16-bit checksum over a memory range.
 * @param  pData Start of the range.
 * @param  len   Length of the range in bytes.
 * @return Checksum value.
 * @author Berkay
 *
 * Logged with every queue send and receive so the TeSSLa specification can
 * verify FIFO order and payload integrity end to end.
 */
uint16_t OS_Trace_u16Checksum(const void *pData, size_t len);

#endif /* OS_TRACE_H */
