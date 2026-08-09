/**
 ******************************************************************************
 * @file    os_trace_config.h
 * @brief   Central switches for the SystemView instrumentation.
 * @author  __________
 ******************************************************************************
 *
 * PURPOSE
 * -------
 * A full trace (all groups enabled) produces a very large number of events and
 * makes both recording and analysis sluggish, up to and including SystemView
 * overflows. The switches below decide per group what gets recorded. Disabled
 * groups cost NOTHING: the OS_TRACE_* macros collapse into empty statements,
 * so the compiler removes the call AND the parameter computation entirely.
 *
 * USAGE
 * -----
 * For each verification run, enable only the group(s) the TeSSLa spec under
 * test actually needs:
 *
 *   tessla/scheduler_tasks.tessla -> SCHEDULER only (+ ISR)
 *   tessla/mutex.tessla           -> SCHEDULER + MUTEX
 *   tessla/semaphore.tessla       -> SCHEDULER + SEMAPHORE
 *   tessla/queue.tessla           -> SCHEDULER + QUEUE
 *   tessla/delay.tessla           -> SCHEDULER + DELAY (+ all blocking events:
 *                                    MUTEX/SEMAPHORE/QUEUE, since the spec
 *                                    distinguishes the blocking causes)
 *   tessla/sensor.tessla          -> APP only
 *
 * This cuts the event rate drastically and keeps the export files manageable
 * for the converter.
 *
 ******************************************************************************
 */

#ifndef OS_TRACE_CONFIG_H
#define OS_TRACE_CONFIG_H

/* --------------------------------------------------------------------------
 * Master switch: 0 disables the complete instrumentation (including the task
 * info and the module registration). Useful for release or timing runs
 * without any trace overhead at all.
 * -------------------------------------------------------------------------- */
#ifndef OS_TRACE_ENABLED
#define OS_TRACE_ENABLED            ( 1 )
#endif

/* --------------------------------------------------------------------------
 * Group switches
 * -------------------------------------------------------------------------- */

/** @brief Scheduler and task state transitions (ready/running/blocked) and idle.
 *  @note  This is the basis for nearly every TeSSLa spec - leave it on when
 *         in doubt. */
#ifndef OS_TRACE_SCHEDULER
#define OS_TRACE_SCHEDULER          ( 1 )
#endif

/** @brief Events from interrupt handlers (RecordEnterISR/RecordExitISR).
 *  @note  SysTick fires every millisecond, making this the single largest
 *         source of events. Turn it off first to reduce the load, as long as
 *         no ISR-related rule is being checked. */
#ifndef OS_TRACE_ISR
#define OS_TRACE_ISR                ( 0 )
#endif

/** @brief Mutex operations (lock/unlock/block/timeout). */
#ifndef OS_TRACE_MUTEX
#define OS_TRACE_MUTEX              ( 1 )
#endif

/** @brief Semaphore operations (take/give/block/timeout). */
#ifndef OS_TRACE_SEMAPHORE
#define OS_TRACE_SEMAPHORE          ( 1 )
#endif

/** @brief Message queue operations (send/recv/full/empty/block/timeout). */
#ifndef OS_TRACE_QUEUE
#define OS_TRACE_QUEUE              ( 1 )
#endif

/** @brief Delay events (non-blocking delay start, busy delay start/end). */
#ifndef OS_TRACE_DELAY
#define OS_TRACE_DELAY              ( 1 )
#endif

/** @brief Application events (UartTxDist, SensorErr, CalStart/CalDone).
 *  @note  Very few events (roughly 10/s), so this can stay on almost always. */
#ifndef OS_TRACE_APP
#define OS_TRACE_APP                ( 1 )
#endif

/* --------------------------------------------------------------------------
 * Fine-grained switch: "try" events
 *
 * Every lock/take/send/recv operation reports a "try" on entry in addition to
 * its outcome (ok/block/timeout/full/empty). The try events are NOT needed for
 * the verification - none of the supplied TeSSLa specs evaluates them - but
 * they roughly double the event count of their group.
 *
 * -> Default: OFF. Only enable them when debugging, to see that an operation
 *    was attempted at all.
 * -------------------------------------------------------------------------- */
#ifndef OS_TRACE_TRY_EVENTS
#define OS_TRACE_TRY_EVENTS         ( 0 )
#endif


/* --------------------------------------------------------------------------
 * Run the integration tests instead of the application
 *
 * 1 = main() starts the test set from tests.c (mutex/semaphore/queue
 *     integration tests with concurrent and blocking access) instead of
 *     sensor/proc/shell. The HC-SR04 is not needed in this mode.
 * 0 = normal operation (distance measurement).
 *
 * Results appear on the UART; the trace can additionally be checked against
 * mutex.tessla / semaphore.tessla / queue.tessla.
 * -------------------------------------------------------------------------- */
#ifndef OS_RUN_INTEGRATION_TESTS
#define OS_RUN_INTEGRATION_TESTS    ( 0 )
#endif

/* --------------------------------------------------------------------------
 * Derived macros - do not change anything below this line.
 *
 * Pattern: OS_TRACE_<GROUP>_REC<n>(...) is either the real record call or an
 * empty statement. The do{}while(0) wrapper keeps the macros syntactically
 * equivalent to a function call (including the trailing semicolon).
 * -------------------------------------------------------------------------- */

#if (OS_TRACE_ENABLED != 0)
  #define OS_TRACE_REC1(evt, p0)             OS_Trace_Record1((evt), (p0))
  #define OS_TRACE_REC2(evt, p0, p1)         OS_Trace_Record2((evt), (p0), (p1))
  #define OS_TRACE_REC3(evt, p0, p1, p2)     OS_Trace_Record3((evt), (p0), (p1), (p2))
#else
  #define OS_TRACE_REC1(evt, p0)             do { (void)0; } while (0)
  #define OS_TRACE_REC2(evt, p0, p1)         do { (void)0; } while (0)
  #define OS_TRACE_REC3(evt, p0, p1, p2)     do { (void)0; } while (0)
#endif

/* --- Mutex --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_MUTEX != 0)
  #define OS_TRACE_MTX2(evt, p0, p1)         OS_TRACE_REC2(evt, p0, p1)
  #define OS_TRACE_MTX3(evt, p0, p1, p2)     OS_TRACE_REC3(evt, p0, p1, p2)
#else
  #define OS_TRACE_MTX2(evt, p0, p1)         do { (void)0; } while (0)
  #define OS_TRACE_MTX3(evt, p0, p1, p2)     do { (void)0; } while (0)
#endif

/* --- Semaphore --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_SEMAPHORE != 0)
  #define OS_TRACE_SEM2(evt, p0, p1)         OS_TRACE_REC2(evt, p0, p1)
  #define OS_TRACE_SEM3(evt, p0, p1, p2)     OS_TRACE_REC3(evt, p0, p1, p2)
#else
  #define OS_TRACE_SEM2(evt, p0, p1)         do { (void)0; } while (0)
  #define OS_TRACE_SEM3(evt, p0, p1, p2)     do { (void)0; } while (0)
#endif

/* --- Queue --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_QUEUE != 0)
  #define OS_TRACE_Q2(evt, p0, p1)           OS_TRACE_REC2(evt, p0, p1)
  #define OS_TRACE_Q3(evt, p0, p1, p2)       OS_TRACE_REC3(evt, p0, p1, p2)
#else
  #define OS_TRACE_Q2(evt, p0, p1)           do { (void)0; } while (0)
  #define OS_TRACE_Q3(evt, p0, p1, p2)       do { (void)0; } while (0)
#endif

/* --- Delay --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_DELAY != 0)
  #define OS_TRACE_DLY1(evt, p0)             OS_TRACE_REC1(evt, p0)
  #define OS_TRACE_DLY2(evt, p0, p1)         OS_TRACE_REC2(evt, p0, p1)
#else
  #define OS_TRACE_DLY1(evt, p0)             do { (void)0; } while (0)
  #define OS_TRACE_DLY2(evt, p0, p1)         do { (void)0; } while (0)
#endif

/* --- Application --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_APP != 0)
  #define OS_TRACE_APP1(evt, p0)             OS_TRACE_REC1(evt, p0)
  #define OS_TRACE_APP2(evt, p0, p1)         OS_TRACE_REC2(evt, p0, p1)
#else
  #define OS_TRACE_APP1(evt, p0)             do { (void)0; } while (0)
  #define OS_TRACE_APP2(evt, p0, p1)         do { (void)0; } while (0)
#endif

/* --- "Try" variants: additionally gated by the fine-grained switch --- */
#if (OS_TRACE_TRY_EVENTS != 0)
  #define OS_TRACE_MTX_TRY2(evt, p0, p1)     OS_TRACE_MTX2(evt, p0, p1)
  #define OS_TRACE_SEM_TRY2(evt, p0, p1)     OS_TRACE_SEM2(evt, p0, p1)
  #define OS_TRACE_Q_TRY2(evt, p0, p1)       OS_TRACE_Q2(evt, p0, p1)
#else
  #define OS_TRACE_MTX_TRY2(evt, p0, p1)     do { (void)0; } while (0)
  #define OS_TRACE_SEM_TRY2(evt, p0, p1)     do { (void)0; } while (0)
  #define OS_TRACE_Q_TRY2(evt, p0, p1)       do { (void)0; } while (0)
#endif

/* --- ISR instrumentation (SystemView base events) --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_ISR != 0)
  #define OS_TRACE_ISR_ENTER()               SEGGER_SYSVIEW_RecordEnterISR()
  #define OS_TRACE_ISR_EXIT()                SEGGER_SYSVIEW_RecordExitISR()
#else
  #define OS_TRACE_ISR_ENTER()               do { (void)0; } while (0)
  #define OS_TRACE_ISR_EXIT()                do { (void)0; } while (0)
#endif

/* --- Scheduler and task state events (SystemView base events) --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_SCHEDULER != 0)
  #define OS_TRACE_TASK_START_EXEC(tcb)      SEGGER_SYSVIEW_OnTaskStartExec((uint32_t)(tcb))
  #define OS_TRACE_TASK_STOP_EXEC()          SEGGER_SYSVIEW_OnTaskStopExec()
  #define OS_TRACE_TASK_START_READY(tcb)     SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)(tcb))
  #define OS_TRACE_TASK_STOP_READY(tcb, c)   SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)(tcb), (c))
  #define OS_TRACE_IDLE()                    SEGGER_SYSVIEW_OnIdle()
#else
  #define OS_TRACE_TASK_START_EXEC(tcb)      do { (void)0; } while (0)
  #define OS_TRACE_TASK_STOP_EXEC()          do { (void)0; } while (0)
  #define OS_TRACE_TASK_START_READY(tcb)     do { (void)0; } while (0)
  #define OS_TRACE_TASK_STOP_READY(tcb, c)   do { (void)0; } while (0)
  #define OS_TRACE_IDLE()                    do { (void)0; } while (0)
#endif

#endif /* OS_TRACE_CONFIG_H */
