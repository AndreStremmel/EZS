/**
 * @file    tests.c
 * @brief   Dedizierte Integrationstests fuer Mutexe, Semaphoren und Queues.
 * @author  TODO: Name eintragen
 *
 * Ablaufsteuerung: TestMainTask schaltet eine Phasenvariable weiter;
 * TestHighTask und TestPeerTask warten jeweils auf "ihre" Phase und
 * spielen darin ihre Rolle. Die Synchronisation laeuft ueber
 * NonBlockedDelays und die Phasenvariable - bewusst ohne zusaetzliche
 * Kernel-Objekte, damit die Tests nicht das pruefen, was sie benutzen.
 *
 * @see tests.h fuer Aktivierung und Auswertung.
 */

#include "tests.h"

#include <string.h>

#include "os_common.h"
#include "os_mutex.h"
#include "os_semaphore.h"
#include "os_queue.h"
#include "os_trace.h"
#include "scheduler.h"
#include "tasks.h"
#include "uart_driver.h"

/* ==========================================================================
 * Testobjekte
 *
 * Trace-IDs bewusst identisch zu den Applikationsobjekten (siehe
 * tests.h): so greifen die bestehenden TeSSLa-Specs unveraendert.
 * ========================================================================== */

/// Mutex fuer Konkurrenz-/Blocking-/Timeout-Tests (Trace-ID wie g_configMutex)
static OS_Mutex_t     s_testMutex;
/// Mutex fuer die UART-Ausgabe der Testergebnisse (Trace-ID wie g_uartMutex)
static OS_Mutex_t     s_testUartMutex;
/// Binaere Semaphore fuer Blocking-/Timeout-/Doppel-Give-Tests
static OS_Semaphore_t s_testSem;
/// Kleine Queue, damit "voll" in vertretbarer Zeit erreichbar ist
static OS_Queue_t     s_testQueue;
/// Puffer der Test-Queue (uint32_t-Nachrichten)
static uint32_t       s_au32QueueBuf[TEST_QUEUE_CAPACITY];

/* ==========================================================================
 * Ablaufsteuerung und Ergebnisse
 * ========================================================================== */

/// Aktuelle Testphase; wird nur von TestMainTask erhoeht.
static volatile uint8_t  s_u8Phase      = 0u;
/* Rundenzaehler: macht Phasenwerte ueber mehrere Testrunden hinweg
 * eindeutig. Ohne ihn kann ein Task, der frueh in der Schleife auf eine
 * niedrige Phasennummer wartet (z.B. TestHighTask auf Phase 4), nicht
 * unterscheiden, ob diese Phase gerade neu gesetzt wurde oder noch von
 * der VORHERIGEN Runde stammt - er pollt dann sofort wieder positiv an
 * und rennt der naechsten Runde davon, wodurch er als hochpriorer Task
 * TestMainTask kontinuierlich CPU-Zeit wegnimmt (Live-Lock unter Last,
 * insbesondere durch zusaetzlichen SystemView-Trace-Overhead). */
static volatile uint16_t s_u16Round     = 0u;
/// Rueckmeldungen der Partnertasks an TestMainTask.
static volatile uint8_t  s_u8PeerDone   = 0u;
static volatile uint8_t  s_u8HighDone   = 0u;
/// Ergebnisse einzelner Teilschritte, die in Partnertasks anfallen.
static volatile OS_Result_t s_ePeerResult = OS_OK;
/// Reihenfolge-Protokoll fuer den Prioritaetstest (1 = High, 2 = Peer).
static volatile uint8_t  s_au8LockOrder[4];
static volatile uint8_t  s_u8LockOrderIdx = 0u;

static uint16_t s_u16TestsRun    = 0u;
static uint16_t s_u16TestsPassed = 0u;

/* ==========================================================================
 * Hilfsfunktionen
 * ========================================================================== */

/**
 * @brief Ergebnis eines Testfalls protokollieren (UART + Zaehler).
 * @param pcName Kurzbeschreibung des Testfalls.
 * @param u8Ok   1 = bestanden, 0 = fehlgeschlagen.
 */
static void prv_vReport(const char *pcName, uint8_t u8Ok)
{
    s_u16TestsRun++;
    if (u8Ok) { s_u16TestsPassed++; }

    OS_Mutex_LockBlocking(&s_testUartMutex);
    UART_SendString(u8Ok ? "[OK]   " : "[FAIL] ");
    UART_SendString(pcName);
    UART_SendString("\r\n");
    OS_Mutex_Unlock(&s_testUartMutex);
}

/**
 * @brief Auf das Erreichen einer Phase warten (nicht-blockierend pollend).
 * @param u8Phase Erwartete Phasennummer.
 *
 * Nutzt Scheduler_vNonBlockedDelay(), damit der wartende Task den
 * BLOCKED-Zustand durchlaeuft und die CPU freigibt.
 */
/**
 * @brief Auf das Erreichen einer Phase IN DER AKTUELLEN RUNDE warten.
 * @param u8Phase        Erwartete Phasennummer.
 * @param pu16LastRound  Zeiger auf die zuletzt von DIESEM Task gesehene
 *                        Rundennummer (ein eigenes statisches uint16_t
 *                        je Aufrufer/Wartepunkt) - wird nach Rueckkehr
 *                        aktualisiert.
 *
 * Ohne den Rundenvergleich koennte ein Task auf eine Phase anspringen,
 * die noch von der VORHERIGEN Runde stammt (der Phasenwert wird pro
 * Runde wiederverwendet). Das fuehrt unter Last zu einem Live-Lock,
 * siehe Kommentar bei s_u16Round.
 */
static void prv_vWaitForPhase(uint8_t u8Phase, volatile uint16_t *pu16LastRound)
{
    while ((s_u8Phase != u8Phase) || (s_u16Round == *pu16LastRound))
    {
        Scheduler_vNonBlockedDelay(10u);
    }
    *pu16LastRound = s_u16Round;
}

/**
 * @brief Warten, bis ein Partnertask sein Fertig-Flag gesetzt hat.
 * @param pu8Flag Zeiger auf das zu beobachtende Flag.
 * @param u32MaxTicks Obergrenze, damit ein Fehler nicht zum Haenger fuehrt.
 * @return 1, wenn das Flag gesetzt wurde, 0 bei Zeitueberschreitung.
 */
static uint8_t prv_u8WaitFlag(volatile uint8_t *pu8Flag, uint32_t u32MaxTicks)
{
    uint32_t u32Waited = 0u;
    while ((*pu8Flag == 0u) && (u32Waited < u32MaxTicks))
    {
        Scheduler_vNonBlockedDelay(1u);
        u32Waited++;
    }
    return (*pu8Flag != 0u) ? 1u : 0u;
}

/* ==========================================================================
 * Initialisierung
 * ========================================================================== */

void Tests_vInitResources(void)
{
    OS_Mutex_Init(&s_testMutex,     OS_TRACE_MTX_CONFIG);
    OS_Mutex_Init(&s_testUartMutex, OS_TRACE_MTX_UART);

    /* Binaere Semaphore: Startwert 0 (leer), Maximum 1 */
    OS_Semaphore_Init(&s_testSem, 0u, 1u, OS_TRACE_SEM_ECHO);

    OS_Queue_Init(&s_testQueue,
                  (uint8_t *)s_au32QueueBuf,
                  sizeof(uint32_t),
                  TEST_QUEUE_CAPACITY,
                  OS_TRACE_Q_SENSOR);
}

/* ==========================================================================
 * TestPeerTask - gleichpriorer Konkurrent (Slot 2, Prio 1)
 * ========================================================================== */

void TestPeerTask(void)
{
    for (;;)
    {
        /* --- Phase 2: Mutex ist von Main gehalten -> NonBlocking muss
         *              fehlschlagen, Timeout muss ablaufen. ------------- */
        { static volatile uint16_t s_u16LastR_peer2 = 0u; prv_vWaitForPhase(2u, &s_u16LastR_peer2); }
        s_ePeerResult = OS_Mutex_LockNonBlocking(&s_testMutex);
        s_u8PeerDone  = 1u;

        { static volatile uint16_t s_u16LastR_peer3 = 0u; prv_vWaitForPhase(3u, &s_u16LastR_peer3); }
        /* Timeout-Variante: 20 Ticks warten, Mutex bleibt belegt */
        s_ePeerResult = OS_Mutex_LockTimeout(&s_testMutex, 20u);
        s_u8PeerDone  = 1u;

        /* --- Phase 4: konkurrierendes blockierendes Acquire.
         * Main haelt den Mutex noch; Peer blockiert hier und wird erst
         * nach der Freigabe geweckt. High macht dasselbe -> die
         * Reihenfolge zeigt, ob die Prioritaet eingehalten wird. ------- */
        { static volatile uint16_t s_u16LastR_peer4 = 0u; prv_vWaitForPhase(4u, &s_u16LastR_peer4); }
        (void)OS_Mutex_LockBlocking(&s_testMutex);
        if (s_u8LockOrderIdx < 4u)
        {
            s_au8LockOrder[s_u8LockOrderIdx++] = 2u;   /* Peer war dran */
        }
        OS_Mutex_Unlock(&s_testMutex);
        s_u8PeerDone = 1u;

        /* --- Phase 6: Semaphore blockierend nehmen (ist leer) --------- */
        { static volatile uint16_t s_u16LastR_peer6 = 0u; prv_vWaitForPhase(6u, &s_u16LastR_peer6); }
        s_ePeerResult = OS_Semaphore_TakeBlocking(&s_testSem);
        s_u8PeerDone  = 1u;

        /* --- Phase 11: Empfaenger, der eine volle Queue leert ---------
         * REIHENFOLGE BEACHTEN: TestMainTask setzt Phase 11 (T16) VOR
         * Phase 9 (T17). Dieser Block muss deshalb ebenfalls vor dem
         * Phase-9-Block stehen - sonst wartet Peer auf 9, waehrend Main
         * schon 11 gesetzt hat und in OS_Queue_SendBlocking() blockiert:
         * klassischer Deadlock, keiner der beiden kommt weiter. */
        { static volatile uint16_t s_u16LastR_peer11 = 0u; prv_vWaitForPhase(11u, &s_u16LastR_peer11); }
        {
            uint32_t u32Dummy;
            /* Etwas warten, damit Main sicher im blockierenden Send steht */
            Scheduler_vNonBlockedDelay(10u);
            (void)OS_Queue_ReceiveNonBlocking(&s_testQueue, &u32Dummy);
            s_u8PeerDone = 1u;
        }

        /* --- Phase 9: Empfaenger im Datenintegritaetstest ------------- */
        { static volatile uint16_t s_u16LastR_peer9 = 0u; prv_vWaitForPhase(9u, &s_u16LastR_peer9); }
        {
            uint8_t  u8Ok = 1u;
            uint32_t u32Expected;
            uint32_t u32Got;

            for (u32Expected = 0u; u32Expected < TEST_MSG_COUNT; u32Expected++)
            {
                /* Blockierend: die Queue ist kleiner als die Nachrichten-
                 * zahl, also laeuft sie zwischendurch leer -> genau der
                 * zu pruefende Fall. */
                if (OS_Queue_ReceiveBlocking(&s_testQueue, &u32Got) != OS_OK)
                {
                    u8Ok = 0u;
                    break;
                }
                /* FIFO-Reihenfolge UND unveraenderter Inhalt in einem:
                 * die Nachrichten tragen ein bekanntes Muster. */
                if (u32Got != (0xA5A50000u | u32Expected))
                {
                    u8Ok = 0u;
                    break;
                }
            }
            s_ePeerResult = u8Ok ? OS_OK : OS_WOULD_BLOCK;
            s_u8PeerDone  = 1u;
        }

        /* Runde beendet - zurueck zum ersten Wartepunkt der for(;;)-
         * Schleife, damit der naechste Testdurchlauf von TestMainTask
         * wieder bedient wird. KEINE eigene Endlosschleife hier: die
         * wuerde den Task dauerhaft blockieren und die Wiederholung
         * der Tests verhindern. */
    }
}

/* ==========================================================================
 * TestHighTask - hochpriorer Konkurrent (Slot 0, Prio 3)
 * ========================================================================== */

void TestHighTask(void)
{
    for (;;)
    {
        /* --- Phase 4: blockierendes Acquire wie Peer, aber Prio 3.
         * Beide warten auf denselben Mutex; nach der Freigabe muss der
         * hochpriore Task ZUERST drankommen. -------------------------- */
        { static volatile uint16_t s_u16LastR_high4 = 0u; prv_vWaitForPhase(4u, &s_u16LastR_high4); }
        /* Kurz warten, damit Peer sein Acquire zuerst absetzt - so ist
         * sichergestellt, dass wirklich beide gleichzeitig warten und
         * die Prioritaet (nicht die Ankunftsreihenfolge) entscheidet. */
        Scheduler_vNonBlockedDelay(2u);
        (void)OS_Mutex_LockBlocking(&s_testMutex);
        if (s_u8LockOrderIdx < 4u)
        {
            s_au8LockOrder[s_u8LockOrderIdx++] = 1u;   /* High war dran */
        }
        OS_Mutex_Unlock(&s_testMutex);
        s_u8HighDone = 1u;

        /* --- Phase 7: Semaphore freigeben, auf die Peer wartet -------- */
        { static volatile uint16_t s_u16LastR_high7 = 0u; prv_vWaitForPhase(7u, &s_u16LastR_high7); }
        OS_Semaphore_Give(&s_testSem);
        s_u8HighDone = 1u;

        /* Runde beendet - zurueck zum ersten Wartepunkt (Phase 4) der
         * for(;;)-Schleife, damit der naechste Testdurchlauf bedient
         * wird. KEINE eigene Endlosschleife hier - die wuerde die
         * Wiederholung der Tests verhindern. */
    }
}

/* ==========================================================================
 * TestMainTask - Ablauf und Auswertung (Slot 1, Prio 1)
 * ========================================================================== */

void TestMainTask(void)
{
    /* Etwas Anlauf, damit UART und die anderen Tasks bereit sind */
    Scheduler_vNonBlockedDelay(200u);

    /* --------------------------------------------------------------------
     * DAUERSCHLEIFE: Der komplette Testdurchlauf wiederholt sich endlos.
     * Damit muss beim Aufzeichnen nicht mehr das schmale Zeitfenster
     * direkt nach dem Boot getroffen werden - SystemView kann jederzeit
     * verbunden werden und faengt garantiert einen vollstaendigen
     * Durchlauf mit allen Blocking-/Timeout-/Konkurrenz-Ereignissen ein.
     * TestPeerTask und TestHighTask laufen ohnehin schon in eigenen
     * for(;;)-Schleifen und kehren von selbst zu ihren Wartepunkten
     * zurueck.
     * -------------------------------------------------------------------- */
    for (;;)
    {
    /* Neue Runde beginnt: Zaehler erhoehen, DAMIT sind alle Phasenwerte
     * dieser Runde von der vorherigen unterscheidbar (siehe
     * prv_vWaitForPhase). Muss vor der ersten Phasenumschaltung
     * passieren. */
    s_u16Round++;

    /* Zaehler fuer diesen Durchlauf zuruecksetzen */
    s_u16TestsRun    = 0u;
    s_u16TestsPassed = 0u;

    OS_Mutex_LockBlocking(&s_testUartMutex);
    UART_SendString("\r\n=== RTOS-Integrationstests ===\r\n");
    OS_Mutex_Unlock(&s_testUartMutex);

    /* ---------------------------------------------------------------- *
     * T1: Mutex - einfaches Lock/Unlock
     * ---------------------------------------------------------------- */
    {
        OS_Result_t e1 = OS_Mutex_LockNonBlocking(&s_testMutex);
        OS_Mutex_Unlock(&s_testMutex);
        prv_vReport("T1  Mutex: Lock/Unlock durch einen Task",
                    (e1 == OS_OK) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T2: Mutex - konkurrierender NonBlocking-Zugriff schlaegt fehl
     * ---------------------------------------------------------------- */
    (void)OS_Mutex_LockBlocking(&s_testMutex);   /* Main haelt ihn */
    s_u8PeerDone = 0u;
    s_u8Phase    = 2u;
    {
        /* Rueckgabewert AUSWERTEN: nur wenn der Peer sein Fertig-Flag in
         * DIESER Runde gesetzt hat, ist s_ePeerResult frisch. Ohne diese
         * Pruefung wuerde ein haengender Peer in Runde 2+ den Wert der
         * VORrunde auswerten -> faelschlich [OK]. */
        uint8_t u8Done = prv_u8WaitFlag(&s_u8PeerDone, 100u);
        prv_vReport("T2  Mutex: NonBlocking bei belegtem Mutex -> WOULD_BLOCK",
                    (u8Done && (s_ePeerResult == OS_WOULD_BLOCK)) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T3: Mutex - Timeout laeuft ab, weil Main nicht freigibt
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 3u;
    {
        uint8_t u8Done = prv_u8WaitFlag(&s_u8PeerDone, 200u);
        prv_vReport("T3  Mutex: LockTimeout bei belegtem Mutex -> TIMEOUT",
                    (u8Done && (s_ePeerResult == OS_TIMEOUT)) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T4: Mutex - zwei blockierende Wartende, Prioritaet entscheidet
     *     Main haelt den Mutex noch aus T2/T3 und gibt ihn erst frei,
     *     wenn beide sicher blockiert warten.
     * ---------------------------------------------------------------- */
    s_u8PeerDone     = 0u;
    s_u8HighDone     = 0u;
    s_u8LockOrderIdx = 0u;
    s_u8Phase        = 4u;

    Scheduler_vNonBlockedDelay(30u);   /* beide sind jetzt BLOCKED */
    OS_Mutex_Unlock(&s_testMutex);     /* Rennen freigeben */

    {
        uint8_t u8H = prv_u8WaitFlag(&s_u8HighDone, 200u);
        uint8_t u8P = prv_u8WaitFlag(&s_u8PeerDone, 200u);
        prv_vReport("T4  Mutex: blockierendes Acquire weckt Wartende",
                    (u8H && u8P && (s_u8LockOrderIdx >= 2u)) ? 1u : 0u);
    }
    prv_vReport("T5  Mutex: hochpriorer Wartender kommt zuerst",
                ((s_u8LockOrderIdx >= 2u) && (s_au8LockOrder[0] == 1u)) ? 1u : 0u);

    /* ---------------------------------------------------------------- *
     * T6: Semaphore - leere Semaphore, NonBlocking schlaegt fehl
     * ---------------------------------------------------------------- */
    prv_vReport("T6  Semaphore: Take auf leerer Semaphore -> WOULD_BLOCK",
                (OS_Semaphore_TakeNonBlocking(&s_testSem) == OS_WOULD_BLOCK) ? 1u : 0u);

    /* ---------------------------------------------------------------- *
     * T7: Semaphore - Timeout auf leerer Semaphore
     * ---------------------------------------------------------------- */
    {
        OS_Result_t e = OS_Semaphore_TakeTimeout(&s_testSem, 20u);
        prv_vReport("T7  Semaphore: TakeTimeout laeuft ab -> TIMEOUT",
                    (e == OS_TIMEOUT) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T8: Semaphore - blockierendes Take wird durch Give geweckt
     *     Peer blockiert (Phase 6), High gibt frei (Phase 7).
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 6u;
    Scheduler_vNonBlockedDelay(20u);   /* Peer ist jetzt BLOCKED */

    s_u8HighDone = 0u;
    s_u8Phase    = 7u;
    {
        uint8_t u8Given = prv_u8WaitFlag(&s_u8HighDone, 100u);
        uint8_t u8Woke  = prv_u8WaitFlag(&s_u8PeerDone, 200u);
        prv_vReport("T8  Semaphore: Give weckt blockierten Task",
                    (u8Given && u8Woke && (s_ePeerResult == OS_OK)) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T9: Semaphore - Doppel-Give haelt den Wert bei 1 (binaer!)
     * ---------------------------------------------------------------- */
    OS_Semaphore_Give(&s_testSem);
    OS_Semaphore_Give(&s_testSem);   /* zweites Give muss ignoriert werden */
    {
        OS_Result_t e1 = OS_Semaphore_TakeNonBlocking(&s_testSem);
        OS_Result_t e2 = OS_Semaphore_TakeNonBlocking(&s_testSem);
        prv_vReport("T9  Semaphore: binaer - Doppel-Give erhoeht nicht",
                    ((e1 == OS_OK) && (e2 == OS_WOULD_BLOCK)) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T10: Queue - leer, NonBlocking-Receive schlaegt fehl
     * ---------------------------------------------------------------- */
    {
        uint32_t u32Dummy;
        prv_vReport("T10 Queue: Receive aus leerer Queue -> WOULD_BLOCK",
                    (OS_Queue_ReceiveNonBlocking(&s_testQueue, &u32Dummy)
                        == OS_WOULD_BLOCK) ? 1u : 0u);
        prv_vReport("T11 Queue: IsEmpty meldet leere Queue",
                    OS_Queue_IsEmpty(&s_testQueue) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T12: Queue - voll laufen lassen, NonBlocking-Send schlaegt fehl
     * ---------------------------------------------------------------- */
    {
        uint32_t i;
        uint8_t  u8AllOk = 1u;
        for (i = 0u; i < TEST_QUEUE_CAPACITY; i++)
        {
            uint32_t u32Val = 0xC0DE0000u | i;
            if (OS_Queue_SendNonBlocking(&s_testQueue, &u32Val) != OS_OK)
            {
                u8AllOk = 0u;
            }
        }
        prv_vReport("T12 Queue: Fuellen bis zur Kapazitaet", u8AllOk);
        prv_vReport("T13 Queue: IsFull meldet volle Queue",
                    OS_Queue_IsFull(&s_testQueue) ? 1u : 0u);

        {
            uint32_t u32Val = 0xDEADBEEFu;
            prv_vReport("T14 Queue: Send in volle Queue -> WOULD_BLOCK",
                        (OS_Queue_SendNonBlocking(&s_testQueue, &u32Val)
                            == OS_WOULD_BLOCK) ? 1u : 0u);
        }

        /* Timeout-Variante an voller Queue */
        {
            uint32_t u32Val = 0xDEADBEEFu;
            prv_vReport("T15 Queue: SendTimeout an voller Queue -> TIMEOUT",
                        (OS_Queue_SendTimeout(&s_testQueue, &u32Val, 20u)
                            == OS_TIMEOUT) ? 1u : 0u);
        }
    }

    /* ---------------------------------------------------------------- *
     * T16: Queue - blockierender Send an voller Queue wird durch einen
     *      Empfaenger geweckt (Peer leert in Phase 11 einen Eintrag).
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 11u;
    {
        uint32_t u32Val = 0xFEED0001u;
        OS_Result_t e = OS_Queue_SendBlocking(&s_testQueue, &u32Val);
        prv_vReport("T16 Queue: blockierender Send wird durch Receive geweckt",
                    (e == OS_OK) ? 1u : 0u);
    }

    /* Queue fuer den naechsten Test leeren */
    {
        uint32_t u32Dummy;
        while (OS_Queue_ReceiveNonBlocking(&s_testQueue, &u32Dummy) == OS_OK)
        {
            /* leeren */
        }
    }

    /* ---------------------------------------------------------------- *
     * T17: Queue - Task-Kommunikation und Datenintegritaet
     *      Main sendet TEST_MSG_COUNT Nachrichten mit bekanntem Muster,
     *      Peer empfaengt und prueft Reihenfolge und Inhalt. Da die
     *      Queue kleiner ist als die Nachrichtenzahl, blockieren beide
     *      Seiten zwischendurch - genau der zu pruefende Fall.
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 9u;
    {
        uint32_t i;
        uint8_t  u8SendOk = 1u;
        for (i = 0u; i < TEST_MSG_COUNT; i++)
        {
            uint32_t u32Val = 0xA5A50000u | i;
            if (OS_Queue_SendBlocking(&s_testQueue, &u32Val) != OS_OK)
            {
                u8SendOk = 0u;
                break;
            }
        }
        prv_vReport("T17 Queue: blockierendes Senden ueber Kapazitaet hinaus",
                    u8SendOk);

        {
            uint8_t u8Done = prv_u8WaitFlag(&s_u8PeerDone, 500u);
            prv_vReport("T18 Queue: FIFO-Reihenfolge und Datenintegritaet",
                        (u8Done && (s_ePeerResult == OS_OK)) ? 1u : 0u);
        }
    }

    /* ---------------------------------------------------------------- *
     * Zusammenfassung
     * ---------------------------------------------------------------- */
    OS_Mutex_LockBlocking(&s_testUartMutex);
    UART_SendString("--- Ergebnis: ");
    UART_SendUInt(s_u16TestsPassed);
    UART_SendString(" von ");
    UART_SendUInt(s_u16TestsRun);
    UART_SendString(" Tests bestanden ---\r\n");
    if (s_u16TestsPassed == s_u16TestsRun)
    {
        UART_SendString("ALLE TESTS BESTANDEN\r\n");
    }
    else
    {
        UART_SendString("!!! FEHLGESCHLAGENE TESTS VORHANDEN !!!\r\n");
    }
    OS_Mutex_Unlock(&s_testUartMutex);

    /* Pause zwischen zwei Durchlaeufen: gibt TestPeerTask und
     * TestHighTask genug Zeit, ihre letzten Phasenbloecke abzuschliessen
     * und sauber zu ihren jeweils ersten Wartepunkten zurueckzukehren,
     * bevor die naechste Runde die Phasen neu setzt. */
    Scheduler_vNonBlockedDelay(500u);
    }   /* Ende der Dauerschleife - naechster Durchlauf */
}
