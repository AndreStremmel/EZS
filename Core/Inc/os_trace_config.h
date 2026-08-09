/*
 * os_trace_config.h
 *  Zentrale Schalter fuer die SystemView-Instrumentierung.
 *
 *  ZWECK
 *  -----
 *  Der volle Trace (alle Gruppen an) erzeugt sehr viele Events und macht
 *  Aufzeichnung wie Analyse traege - bis hin zu SystemView-Overflows.
 *  Ueber die Schalter hier laesst sich pro Gruppe entscheiden, was
 *  aufgezeichnet wird. Abgeschaltete Gruppen kosten NICHTS: die
 *  OS_TRACE_*-Makros werden dann zu Leeranweisungen, der Compiler
 *  entfernt Aufruf UND Parameterberechnung vollstaendig.
 *
 *  BENUTZUNG
 *  ---------
 *  Fuer die einzelnen Verifikationslaeufe jeweils nur die Gruppe(n)
 *  einschalten, die die zu pruefende TeSSLa-Spec braucht:
 *
 *    tessla/scheduler_tasks.tessla -> nur SCHEDULER (+ ISR)
 *    tessla/mutex.tessla           -> SCHEDULER + MUTEX
 *    tessla/semaphore.tessla       -> SCHEDULER + SEMAPHORE
 *    tessla/queue.tessla           -> SCHEDULER + QUEUE
 *    tessla/delay.tessla           -> SCHEDULER + DELAY (+ alle Block-
 *                                    Events: MUTEX/SEMAPHORE/QUEUE, da
 *                                    die Spec Block-Ursachen unterscheidet)
 *    tessla/sensor.tessla          -> nur APP
 *
 *  Das reduziert die Eventrate drastisch und haelt die Exportdateien
 *  fuer den Konverter handhabbar.
 */

#ifndef OS_TRACE_CONFIG_H
#define OS_TRACE_CONFIG_H

/* --------------------------------------------------------------------------
 * Hauptschalter: 0 = komplette Instrumentierung aus (auch Task-Infos und
 * Modul-Registrierung entfallen). Nuetzlich fuer Release-/Messlaeufe ohne
 * jeden Trace-Overhead.
 * -------------------------------------------------------------------------- */
#ifndef OS_TRACE_ENABLED
#define OS_TRACE_ENABLED            ( 1 )
#endif

/* --------------------------------------------------------------------------
 * Gruppenschalter
 * -------------------------------------------------------------------------- */

/// Scheduler-/Task-Zustandswechsel (Ready/Running/Blocked) und Idle.
/// ACHTUNG: Basis fuer fast alle TeSSLa-Specs - im Zweifel anlassen.
#ifndef OS_TRACE_SCHEDULER
#define OS_TRACE_SCHEDULER          ( 1 )
#endif

/// Events aus Interrupt-Handlern (RecordEnterISR/RecordExitISR).
/// Der SysTick feuert jede Millisekunde -> das ist die groesste
/// Einzelquelle an Events. Zum Entlasten als Erstes abschalten,
/// solange keine ISR-bezogene Regel geprueft wird.
#ifndef OS_TRACE_ISR
#define OS_TRACE_ISR                ( 0 )
#endif

/// Mutex-Operationen (Lock/Unlock/Block/Timeout)
#ifndef OS_TRACE_MUTEX
#define OS_TRACE_MUTEX              ( 1 )
#endif

/// Semaphor-Operationen (Take/Give/Block/Timeout)
#ifndef OS_TRACE_SEMAPHORE
#define OS_TRACE_SEMAPHORE          ( 1 )
#endif

/// Message-Queue-Operationen (Send/Recv/Full/Empty/Block/Timeout)
#ifndef OS_TRACE_QUEUE
#define OS_TRACE_QUEUE              ( 1 )
#endif

/// Delay-Events (NonBlocked-Delay-Start, Busy-Delay-Start/Ende)
#ifndef OS_TRACE_DELAY
#define OS_TRACE_DELAY              ( 1 )
#endif

/// Applikationsevents (UartTxDist, SensorErr, CalStart/CalDone).
/// Sehr wenige Events (ca. 10/s) - kann fast immer anbleiben.
#ifndef OS_TRACE_APP
#define OS_TRACE_APP                ( 1 )
#endif

/* --------------------------------------------------------------------------
 * Feinschalter: "Try"-Events
 *
 * Jede Lock-/Take-/Send-/Recv-Operation meldet zusaetzlich zum Ergebnis
 * (Ok/Block/Timeout/Full/Empty) einen "Try" beim Eintritt. Fuer die
 * Verifikation sind die Try-Events NICHT noetig - keine der gelieferten
 * TeSSLa-Specs wertet sie aus. Sie verdoppeln aber grob die Eventzahl
 * der jeweiligen Gruppe.
 *
 * -> Standard: AUS. Nur einschalten, wenn ihr beim Debuggen sehen wollt,
 *    dass eine Operation ueberhaupt versucht wurde.
 * -------------------------------------------------------------------------- */
#ifndef OS_TRACE_TRY_EVENTS
#define OS_TRACE_TRY_EVENTS         ( 0 )
#endif


/* --------------------------------------------------------------------------
 * Integrationstests statt Applikation starten
 *
 * 1 = main() startet das Testset aus tests.c (Mutex-/Semaphore-/Queue-
 *     Integrationstests mit konkurrierendem und blockierendem Zugriff)
 *     statt Sensor/Proc/Shell. Der HC-SR04 wird dabei nicht benoetigt.
 * 0 = Normalbetrieb (Distanzmessung).
 *
 * Ergebnisse erscheinen auf der UART; der Trace laesst sich zusaetzlich
 * gegen mutex.tessla / semaphore.tessla / queue.tessla pruefen.
 * -------------------------------------------------------------------------- */
#ifndef OS_RUN_INTEGRATION_TESTS
#define OS_RUN_INTEGRATION_TESTS    ( 0 )
#endif

/* --------------------------------------------------------------------------
 * Abgeleitete Makros - hier nichts aendern.
 *
 * Muster: OS_TRACE_<GRUPPE>_REC<n>(...) ist entweder der echte
 * Record-Aufruf oder eine Leeranweisung. Die do{}while(0)-Huelle haelt
 * die Makros syntaktisch wie einen Funktionsaufruf (inkl. Semikolon).
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

/* --- Applikation --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_APP != 0)
  #define OS_TRACE_APP1(evt, p0)             OS_TRACE_REC1(evt, p0)
  #define OS_TRACE_APP2(evt, p0, p1)         OS_TRACE_REC2(evt, p0, p1)
#else
  #define OS_TRACE_APP1(evt, p0)             do { (void)0; } while (0)
  #define OS_TRACE_APP2(evt, p0, p1)         do { (void)0; } while (0)
#endif

/* --- "Try"-Varianten: zusaetzlich am Feinschalter haengend --- */
#if (OS_TRACE_TRY_EVENTS != 0)
  #define OS_TRACE_MTX_TRY2(evt, p0, p1)     OS_TRACE_MTX2(evt, p0, p1)
  #define OS_TRACE_SEM_TRY2(evt, p0, p1)     OS_TRACE_SEM2(evt, p0, p1)
  #define OS_TRACE_Q_TRY2(evt, p0, p1)       OS_TRACE_Q2(evt, p0, p1)
#else
  #define OS_TRACE_MTX_TRY2(evt, p0, p1)     do { (void)0; } while (0)
  #define OS_TRACE_SEM_TRY2(evt, p0, p1)     do { (void)0; } while (0)
  #define OS_TRACE_Q_TRY2(evt, p0, p1)       do { (void)0; } while (0)
#endif

/* --- ISR-Instrumentierung (SystemView-Basisevents) --- */
#if (OS_TRACE_ENABLED != 0) && (OS_TRACE_ISR != 0)
  #define OS_TRACE_ISR_ENTER()               SEGGER_SYSVIEW_RecordEnterISR()
  #define OS_TRACE_ISR_EXIT()                SEGGER_SYSVIEW_RecordExitISR()
#else
  #define OS_TRACE_ISR_ENTER()               do { (void)0; } while (0)
  #define OS_TRACE_ISR_EXIT()                do { (void)0; } while (0)
#endif

/* --- Scheduler-/Task-Zustandsevents (SystemView-Basisevents) --- */
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
