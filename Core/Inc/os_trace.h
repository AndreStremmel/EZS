/*
 * os_trace.h
 *  SystemView-Instrumentierung fuer die TeSSLa-Verifikation.
 *
 *  Registriert ein eigenes SystemView-Modul "DOSRTOS" mit benannten
 *  Events fuer Mutex/Semaphore/Queue/Delay/Applikation. Der Konverter
 *  (tools/sysview_to_tessla.py) uebersetzt den SystemView-Textexport
 *  dieser Events in TeSSLa-Eingabestreams.
 */

#ifndef OS_TRACE_H
#define OS_TRACE_H

#include <stdint.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * Ressourcen-IDs (muessen zu tools/sysview_to_tessla.py passen!)
 * -------------------------------------------------------------------------- */
#define OS_TRACE_MTX_UART      ( 1u )
#define OS_TRACE_MTX_CONFIG    ( 2u )

#define OS_TRACE_SEM_ECHO      ( 1u )
#define OS_TRACE_SEM_UARTRX    ( 2u )

#define OS_TRACE_Q_SENSOR      ( 1u )
#define OS_TRACE_Q_PROCESSED   ( 2u )

/// Pseudo-Task-ID fuer Events aus ISR-Kontext
#define OS_TRACE_TASK_ISR      ( 255u )

/* --------------------------------------------------------------------------
 * Modul-Event-Indizes (Offset kommt zur Laufzeit von SystemView)
 * Reihenfolge MUSS zur Beschreibung in os_trace.c passen!
 * -------------------------------------------------------------------------- */
typedef enum
{
    OS_TRACE_EVT_MTX_LOCK_TRY = 0,   // Mtx, Task
    OS_TRACE_EVT_MTX_LOCK_OK,        // Mtx, Task
    OS_TRACE_EVT_MTX_LOCK_BLOCK,     // Mtx, Task, Ticks (0xFFFFFFFF = unendlich)
    OS_TRACE_EVT_MTX_LOCK_TIMEOUT,   // Mtx, Task
    OS_TRACE_EVT_MTX_UNLOCK_OK,      // Mtx, Task
    OS_TRACE_EVT_MTX_UNLOCK_DENIED,  // Mtx, Task (Nicht-Besitzer!)

    OS_TRACE_EVT_SEM_TAKE_TRY,       // Sem, Task
    OS_TRACE_EVT_SEM_TAKE_OK,        // Sem, Task, CountDanach
    OS_TRACE_EVT_SEM_TAKE_BLOCK,     // Sem, Task, Ticks
    OS_TRACE_EVT_SEM_TAKE_TIMEOUT,   // Sem, Task
    OS_TRACE_EVT_SEM_GIVE_OK,        // Sem, Task, CountDanach
    OS_TRACE_EVT_SEM_GIVE_IGNORED,   // Sem, Task (Semaphore war schon voll)

    OS_TRACE_EVT_Q_SEND_TRY,         // Q, Task
    OS_TRACE_EVT_Q_SEND_OK,          // Q, Chk, CountDanach
    OS_TRACE_EVT_Q_SEND_FULL,        // Q, Task
    OS_TRACE_EVT_Q_SEND_BLOCK,       // Q, Task, Ticks
    OS_TRACE_EVT_Q_SEND_TIMEOUT,     // Q, Task
    OS_TRACE_EVT_Q_RECV_TRY,         // Q, Task
    OS_TRACE_EVT_Q_RECV_OK,          // Q, Chk, CountDanach
    OS_TRACE_EVT_Q_RECV_EMPTY,       // Q, Task
    OS_TRACE_EVT_Q_RECV_BLOCK,       // Q, Task, Ticks
    OS_TRACE_EVT_Q_RECV_TIMEOUT,     // Q, Task

    OS_TRACE_EVT_DELAY_START,        // Task, Ticks   (Non-Blocking Delay)
    OS_TRACE_EVT_BUSY_DELAY_START,   // Task, Ticks   (Blocking Delay)
    OS_TRACE_EVT_BUSY_DELAY_END,     // Task

    OS_TRACE_EVT_TASK_MAP,           // TaskIdx, TCB-Adresse (fuer den Konverter)
    OS_TRACE_EVT_UART_TX_DIST,       // Distanz in mm
    OS_TRACE_EVT_SENSOR_ERR,         // Fehlercode
    OS_TRACE_EVT_CAL_START,          // Zieldistanz mm
    OS_TRACE_EVT_CAL_DONE,           // Offset (als u32 gecastet)

    OS_TRACE_EVT__COUNT
} OS_TraceEvent_t;

/// Sensor-Fehlercodes (fuer OS_TRACE_EVT_SENSOR_ERR und UART-Ausgabe)
#define SENSOR_ERR_NO_ECHO       ( 1u )  ///< Kein Echo innerhalb des Timeouts
#define SENSOR_ERR_OUT_OF_RANGE  ( 2u )  ///< Pulsdauer ausserhalb plausibler Grenzen

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */

/**
 * @brief Modul bei SystemView registrieren und Task-Infos senden.
 *        NACH SEGGER_SYSVIEW_Conf() und VOR Scheduler_vInit() aufrufen!
 */
void OS_Trace_Init(void);

/// Rohes Modul-Event senden (1-3 Parameter)
void OS_Trace_Record1(OS_TraceEvent_t evt, uint32_t p0);
void OS_Trace_Record2(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1);
void OS_Trace_Record3(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1, uint32_t p2);

/// 16-bit-Pruefsumme ueber einen Speicherbereich (fuer Queue-FIFO/Integritaet)
uint16_t OS_Trace_u16Checksum(const void *pData, size_t len);

#endif /* OS_TRACE_H */
