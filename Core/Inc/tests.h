/**
 * @file    tests.h
 * @brief   Dedizierte Integrationstests fuer Mutexe, Semaphoren und Queues.
 * @author  TODO: Name eintragen
 *
 * ZWECK
 * -----
 * Im Normalbetrieb (Sensor -> Proc -> Shell) entstehen Konkurrenz- und
 * Grenzfaelle nur zufaellig. Fuer den Nachweis der Kernel-Eigenschaften
 * braucht es Szenarien, die diese Faelle **gezielt provozieren**:
 * konkurrierender Zugriff, blockierendes Acquire, volle/leere Queue,
 * Timeouts, Prioritaetsreihenfolge.
 *
 * Dieses Modul stellt dafuer ein alternatives Taskset bereit.
 *
 * AKTIVIERUNG
 * -----------
 * In os_trace_config.h (oder als Compiler-Define) setzen:
 * @code
 *   #define OS_RUN_INTEGRATION_TESTS  1
 * @endcode
 * Dann startet main() statt Sensor/Proc/Shell die drei Testtasks.
 * Der HC-SR04 wird dabei nicht benoetigt.
 *
 * ERGEBNISAUSWERTUNG - zwei Wege, die sich ergaenzen:
 *
 * 1. **UART** (115200 8N1): jeder Testfall meldet `[OK]` oder `[FAIL]`
 *    mit Klartextbeschreibung, am Ende eine Zusammenfassung.
 *    Das ist der schnelle Sichtnachweis.
 *
 * 2. **SystemView + TeSSLa**: die Tests benutzen bewusst dieselben
 *    Trace-IDs wie die Applikationsobjekte (MTX 1/2, SEM 1/2, Q 1/2).
 *    Dadurch greifen die vorhandenen Specs unveraendert - ein Trace des
 *    Testlaufs laesst sich direkt gegen mutex.tessla, semaphore.tessla
 *    und queue.tessla pruefen. Da die Tests die Grenzfaelle erzwingen,
 *    ist das der eigentliche Nachweis, dass die Regeln auch unter
 *    Konkurrenz halten (und nicht nur mangels Gelegenheit nie verletzt
 *    wurden).
 *
 * TASKSET IM TESTMODUS
 * --------------------
 * | Idx | Task          | Prio | Rolle                                  |
 * |-----|---------------|------|----------------------------------------|
 * | 0   | TestHighTask  | 3    | hochpriorer Konkurrent (Prio-Reihenfolge)|
 * | 1   | TestMainTask  | 1    | Testablauf, Auswertung, UART-Ausgabe    |
 * | 2   | TestPeerTask  | 1    | gleichpriorer Konkurrent (Round-Robin)  |
 * | 3   | IdleTask      | 0    | Idle                                    |
 */

#ifndef TESTS_H
#define TESTS_H

#include <stdint.h>

/**
 * @brief Testobjekte (Mutex, Semaphore, Queue) initialisieren.
 *
 * Anstelle von App_Resources_Init() aufrufen, wenn
 * #OS_RUN_INTEGRATION_TESTS aktiv ist. Legt zusaetzlich eine kleine
 * Test-Queue mit Kapazitaet #TEST_QUEUE_CAPACITY an, damit sich das
 * Volllaufen in vertretbarer Zeit erzwingen laesst.
 */
void Tests_vInitResources(void);

/// Testablauf-Steuerung, hoechste Prioritaet (Task-Slot 0).
void TestHighTask(void);

/// Haupttask des Testlaufs mit Auswertung und UART-Ausgabe (Slot 1).
void TestMainTask(void);

/// Gleichpriorer Partner fuer Konkurrenzszenarien (Slot 2).
void TestPeerTask(void);

/// Kapazitaet der Test-Queue - bewusst klein, damit "voll" schnell eintritt.
#define TEST_QUEUE_CAPACITY   ( 4u )

/// Anzahl Nachrichten im Datenintegritaetstest.
#define TEST_MSG_COUNT        ( 16u )

#endif /* TESTS_H */
