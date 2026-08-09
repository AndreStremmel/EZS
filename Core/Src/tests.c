/**
 ******************************************************************************
 * @file    tests.c
 * @brief   Dedicated integration tests for mutexes, semaphores and queues -
 *          implementation.
 * @author  __________
 ******************************************************************************
 *
 * Sequencing: TestMainTask advances a phase variable; TestHighTask and
 * TestPeerTask each wait for "their" phase and play their role in it. The
 * synchronisation deliberately uses only non-blocking delays and the phase
 * variable - no additional kernel objects - so that the tests do not depend on
 * the very mechanisms they are meant to verify.
 *
 * @see tests.h for activation and for how to evaluate the results.
 *
 ******************************************************************************
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
 * Test objects
 *
 * The trace IDs are deliberately identical to those of the application
 * objects (see tests.h), so the existing TeSSLa specs apply unchanged.
 * ========================================================================== */

/// Mutex for the contention/blocking/timeout tests (trace ID as g_configMutex)
static OS_Mutex_t     s_testMutex;
/// Mutex guarding the UART output of the test results (trace ID as g_uartMutex)
static OS_Mutex_t     s_testUartMutex;
/// Binary semaphore for the blocking/timeout/double-give tests
static OS_Semaphore_t s_testSem;
/// Small queue so that "full" is reachable in reasonable time
static OS_Queue_t     s_testQueue;
/// Storage backing the test queue (uint32_t messages)
static uint32_t       s_au32QueueBuf[TEST_QUEUE_CAPACITY];

/* ==========================================================================
 * Sequencing and results
 * ========================================================================== */

/// Current test phase; only ever incremented by TestMainTask.
static volatile uint8_t  s_u8Phase      = 0u;
/// Completion flags reported back by the partner tasks to TestMainTask.
static volatile uint8_t  s_u8PeerDone   = 0u;
static volatile uint8_t  s_u8HighDone   = 0u;
/// Result of the sub-steps that are executed inside the partner tasks.
static volatile OS_Result_t s_ePeerResult = OS_OK;
/// Order log for the priority test (1 = High, 2 = Peer).
static volatile uint8_t  s_au8LockOrder[4];
static volatile uint8_t  s_u8LockOrderIdx = 0u;

static uint16_t s_u16TestsRun    = 0u;   ///< Number of test cases executed
static uint16_t s_u16TestsPassed = 0u;   ///< Number of test cases that passed

/* ==========================================================================
 * Helper functions
 * ========================================================================== */

/**
 * @brief Record the result of a test case (UART output + counters).
 * @param pcName Short description of the test case.
 * @param u8Ok   1 = passed, 0 = failed.
 * @author __________
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
 * @brief Wait until a given test phase is reached.
 * @param u8Phase Phase number to wait for.
 * @author __________
 *
 * Polls via Scheduler_vNonBlockedDelay() so the waiting task passes through
 * the BLOCKED state and releases the CPU instead of spinning on it.
 */
static void prv_vWaitForPhase(uint8_t u8Phase)
{
    while (s_u8Phase != u8Phase)
    {
        Scheduler_vNonBlockedDelay(1u);
    }
}

/**
 * @brief  Wait until a partner task has set its completion flag.
 * @param  pu8Flag     Flag to observe.
 * @param  u32MaxTicks Upper bound, so that a failing test cannot hang the run.
 * @return 1 if the flag was set, 0 on timeout.
 * @author __________
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
 * Initialisation
 * ========================================================================== */

/**
 * @brief Initialise the test objects (mutexes, semaphore, queue).
 * @author __________
 *
 * Call instead of App_Resources_Init() when #OS_RUN_INTEGRATION_TESTS is set.
 */
void Tests_vInitResources(void)
{
    OS_Mutex_Init(&s_testMutex,     OS_TRACE_MTX_CONFIG);
    OS_Mutex_Init(&s_testUartMutex, OS_TRACE_MTX_UART);

    /* Binary semaphore: initial count 0 (empty), maximum 1 */
    OS_Semaphore_Init(&s_testSem, 0u, 1u, OS_TRACE_SEM_ECHO);

    OS_Queue_Init(&s_testQueue,
                  (uint8_t *)s_au32QueueBuf,
                  sizeof(uint32_t),
                  TEST_QUEUE_CAPACITY,
                  OS_TRACE_Q_SENSOR);
}

/* ==========================================================================
 * TestPeerTask - equal-priority contender (slot 2, prio 1)
 * ========================================================================== */

/**
 * @brief Equal-priority partner task used in the contention scenarios.
 * @author __________
 *
 * Waits for its phases and performs the counterpart of whatever TestMainTask
 * is currently testing, reporting back through s_ePeerResult and s_u8PeerDone.
 */
void TestPeerTask(void)
{
    for (;;)
    {
        /* --- Phase 2: the mutex is held by Main -> the non-blocking lock
         *              must fail and the timeout must expire. ----------- */
        prv_vWaitForPhase(2u);
        s_ePeerResult = OS_Mutex_LockNonBlocking(&s_testMutex);
        s_u8PeerDone  = 1u;

        prv_vWaitForPhase(3u);
        /* Timeout variant: wait 20 ticks while the mutex stays held */
        s_ePeerResult = OS_Mutex_LockTimeout(&s_testMutex, 20u);
        s_u8PeerDone  = 1u;

        /* --- Phase 4: concurrent blocking acquire.
         * Main still holds the mutex; Peer blocks here and is only woken
         * after the release. High does the same -> the resulting order
         * shows whether the priority is honoured. --------------------- */
        prv_vWaitForPhase(4u);
        (void)OS_Mutex_LockBlocking(&s_testMutex);
        if (s_u8LockOrderIdx < 4u)
        {
            s_au8LockOrder[s_u8LockOrderIdx++] = 2u;   /* Peer got it */
        }
        OS_Mutex_Unlock(&s_testMutex);
        s_u8PeerDone = 1u;

        /* --- Phase 6: blocking take on the (empty) semaphore ---------- */
        prv_vWaitForPhase(6u);
        s_ePeerResult = OS_Semaphore_TakeBlocking(&s_testSem);
        s_u8PeerDone  = 1u;

        /* --- Phase 9: receiver side of the data integrity test -------- */
        prv_vWaitForPhase(9u);
        {
            uint8_t  u8Ok = 1u;
            uint32_t u32Expected;
            uint32_t u32Got;

            for (u32Expected = 0u; u32Expected < TEST_MSG_COUNT; u32Expected++)
            {
                /* Blocking: the queue is smaller than the message count,
                 * so it runs empty in between -> exactly the case under
                 * test. */
                if (OS_Queue_ReceiveBlocking(&s_testQueue, &u32Got) != OS_OK)
                {
                    u8Ok = 0u;
                    break;
                }
                /* Checks FIFO order AND payload integrity at once: the
                 * messages carry a known pattern. */
                if (u32Got != (0xA5A50000u | u32Expected))
                {
                    u8Ok = 0u;
                    break;
                }
            }
            s_ePeerResult = u8Ok ? OS_OK : OS_WOULD_BLOCK;
            s_u8PeerDone  = 1u;
        }

        /* --- Phase 11: receiver that drains one slot of a full queue -- */
        prv_vWaitForPhase(11u);
        {
            uint32_t u32Dummy;
            /* Wait a little so Main is definitely inside the blocking send */
            Scheduler_vNonBlockedDelay(10u);
            (void)OS_Queue_ReceiveNonBlocking(&s_testQueue, &u32Dummy);
            s_u8PeerDone = 1u;
        }

        /* Afterwards just keep the task alive without doing anything */
        for (;;)
        {
            Scheduler_vNonBlockedDelay(50u);
        }
    }
}

/* ==========================================================================
 * TestHighTask - high-priority contender (slot 0, prio 3)
 * ========================================================================== */

/**
 * @brief High-priority partner task used in the priority-ordering scenarios.
 * @author __________
 */
void TestHighTask(void)
{
    for (;;)
    {
        /* --- Phase 4: blocking acquire like Peer, but at priority 3.
         * Both wait on the same mutex; after the release the
         * high-priority task must get it FIRST. ---------------------- */
        prv_vWaitForPhase(4u);
        /* Wait briefly so Peer issues its acquire first - this guarantees
         * that both are really waiting at the same time and that the
         * priority, not the arrival order, decides the outcome. */
        Scheduler_vNonBlockedDelay(2u);
        (void)OS_Mutex_LockBlocking(&s_testMutex);
        if (s_u8LockOrderIdx < 4u)
        {
            s_au8LockOrder[s_u8LockOrderIdx++] = 1u;   /* High got it */
        }
        OS_Mutex_Unlock(&s_testMutex);
        s_u8HighDone = 1u;

        /* --- Phase 7: give the semaphore that Peer is waiting on ------ */
        prv_vWaitForPhase(7u);
        OS_Semaphore_Give(&s_testSem);
        s_u8HighDone = 1u;

        for (;;)
        {
            Scheduler_vNonBlockedDelay(50u);
        }
    }
}

/* ==========================================================================
 * TestMainTask - sequencing and evaluation (slot 1, prio 1)
 * ========================================================================== */

/**
 * @brief Main test task: drives the phases, evaluates the results and prints
 *        them over UART.
 * @author __________
 *
 * Runs the test cases T1..T18 in order, then prints a summary and goes idle so
 * the trace can be closed cleanly in SystemView.
 */
void TestMainTask(void)
{
    /* Short run-up so the UART and the other tasks are ready */
    Scheduler_vNonBlockedDelay(200u);

    OS_Mutex_LockBlocking(&s_testUartMutex);
    UART_SendString("\r\n=== RTOS-Integrationstests ===\r\n");
    OS_Mutex_Unlock(&s_testUartMutex);

    /* ---------------------------------------------------------------- *
     * T1: Mutex - simple lock/unlock by a single task
     * ---------------------------------------------------------------- */
    {
        OS_Result_t e1 = OS_Mutex_LockNonBlocking(&s_testMutex);
        OS_Mutex_Unlock(&s_testMutex);
        prv_vReport("T1  Mutex: Lock/Unlock durch einen Task",
                    (e1 == OS_OK) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T2: Mutex - a competing non-blocking lock must fail
     * ---------------------------------------------------------------- */
    (void)OS_Mutex_LockBlocking(&s_testMutex);   /* Main holds it */
    s_u8PeerDone = 0u;
    s_u8Phase    = 2u;
    (void)prv_u8WaitFlag(&s_u8PeerDone, 100u);
    prv_vReport("T2  Mutex: NonBlocking bei belegtem Mutex -> WOULD_BLOCK",
                (s_ePeerResult == OS_WOULD_BLOCK) ? 1u : 0u);

    /* ---------------------------------------------------------------- *
     * T3: Mutex - the timeout expires because Main never releases
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 3u;
    (void)prv_u8WaitFlag(&s_u8PeerDone, 200u);
    prv_vReport("T3  Mutex: LockTimeout bei belegtem Mutex -> TIMEOUT",
                (s_ePeerResult == OS_TIMEOUT) ? 1u : 0u);

    /* ---------------------------------------------------------------- *
     * T4: Mutex - two blocking waiters, the priority decides the order.
     *     Main still holds the mutex from T2/T3 and only releases it once
     *     both waiters are definitely blocked.
     * ---------------------------------------------------------------- */
    s_u8PeerDone     = 0u;
    s_u8HighDone     = 0u;
    s_u8LockOrderIdx = 0u;
    s_u8Phase        = 4u;

    Scheduler_vNonBlockedDelay(20u);   /* both are BLOCKED by now */
    OS_Mutex_Unlock(&s_testMutex);     /* start the race */

    (void)prv_u8WaitFlag(&s_u8HighDone, 200u);
    (void)prv_u8WaitFlag(&s_u8PeerDone, 200u);
    prv_vReport("T4  Mutex: blockierendes Acquire weckt Wartende",
                (s_u8LockOrderIdx >= 2u) ? 1u : 0u);
    prv_vReport("T5  Mutex: hochpriorer Wartender kommt zuerst",
                ((s_u8LockOrderIdx >= 2u) && (s_au8LockOrder[0] == 1u)) ? 1u : 0u);

    /* ---------------------------------------------------------------- *
     * T6: Semaphore - a non-blocking take on an empty semaphore must fail
     * ---------------------------------------------------------------- */
    prv_vReport("T6  Semaphore: Take auf leerer Semaphore -> WOULD_BLOCK",
                (OS_Semaphore_TakeNonBlocking(&s_testSem) == OS_WOULD_BLOCK) ? 1u : 0u);

    /* ---------------------------------------------------------------- *
     * T7: Semaphore - the timeout expires on an empty semaphore
     * ---------------------------------------------------------------- */
    {
        uint32_t u32Before = 0u;
        OS_Result_t e = OS_Semaphore_TakeTimeout(&s_testSem, 20u);
        (void)u32Before;
        prv_vReport("T7  Semaphore: TakeTimeout laeuft ab -> TIMEOUT",
                    (e == OS_TIMEOUT) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T8: Semaphore - a blocking take is woken by a give.
     *     Peer blocks (phase 6), High gives (phase 7).
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 6u;
    Scheduler_vNonBlockedDelay(20u);   /* Peer is BLOCKED by now */

    s_u8HighDone = 0u;
    s_u8Phase    = 7u;
    (void)prv_u8WaitFlag(&s_u8HighDone, 100u);
    {
        uint8_t u8Woke = prv_u8WaitFlag(&s_u8PeerDone, 200u);
        prv_vReport("T8  Semaphore: Give weckt blockierten Task",
                    (u8Woke && (s_ePeerResult == OS_OK)) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T9: Semaphore - a double give must not raise the count above 1
     * ---------------------------------------------------------------- */
    OS_Semaphore_Give(&s_testSem);
    OS_Semaphore_Give(&s_testSem);   /* the second give must be ignored */
    {
        OS_Result_t e1 = OS_Semaphore_TakeNonBlocking(&s_testSem);
        OS_Result_t e2 = OS_Semaphore_TakeNonBlocking(&s_testSem);
        prv_vReport("T9  Semaphore: binaer - Doppel-Give erhoeht nicht",
                    ((e1 == OS_OK) && (e2 == OS_WOULD_BLOCK)) ? 1u : 0u);
    }

    /* ---------------------------------------------------------------- *
     * T10: Queue - a non-blocking receive on an empty queue must fail
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
     * T12: Queue - fill it up, then a non-blocking send must fail
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

        /* Timeout variant on the full queue */
        {
            uint32_t u32Val = 0xDEADBEEFu;
            prv_vReport("T15 Queue: SendTimeout an voller Queue -> TIMEOUT",
                        (OS_Queue_SendTimeout(&s_testQueue, &u32Val, 20u)
                            == OS_TIMEOUT) ? 1u : 0u);
        }
    }

    /* ---------------------------------------------------------------- *
     * T16: Queue - a blocking send on a full queue is woken by a receiver
     *      (Peer drains one entry in phase 11).
     * ---------------------------------------------------------------- */
    s_u8PeerDone = 0u;
    s_u8Phase    = 11u;
    {
        uint32_t u32Val = 0xFEED0001u;
        OS_Result_t e = OS_Queue_SendBlocking(&s_testQueue, &u32Val);
        prv_vReport("T16 Queue: blockierender Send wird durch Receive geweckt",
                    (e == OS_OK) ? 1u : 0u);
    }

    /* Drain the queue for the next test */
    {
        uint32_t u32Dummy;
        while (OS_Queue_ReceiveNonBlocking(&s_testQueue, &u32Dummy) == OS_OK)
        {
            /* drain */
        }
    }

    /* ---------------------------------------------------------------- *
     * T17: Queue - task communication and data integrity.
     *      Main sends TEST_MSG_COUNT messages carrying a known pattern,
     *      Peer receives them and checks order and content. Since the
     *      queue is smaller than the message count, both sides block in
     *      between - exactly the case under test.
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
     * Summary
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

    /* Test run finished - the task stays asleep so the trace winds down
     * quietly and can be closed cleanly in SystemView. */
    for (;;)
    {
        Scheduler_vNonBlockedDelay(100u);
    }
}
