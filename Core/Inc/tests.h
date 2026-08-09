/**
 ******************************************************************************
 * @file    tests.h
 * @brief   Dedicated integration tests for mutexes, semaphores and queues.
 * @author  __________
 ******************************************************************************
 *
 * PURPOSE
 * -------
 * In normal operation (sensor -> proc -> shell) contention and edge cases only
 * occur by accident. Demonstrating the kernel properties requires scenarios
 * that provoke them deliberately: concurrent access, blocking acquire, full
 * and empty queues, timeouts and priority ordering.
 *
 * This module provides an alternative task set for exactly that.
 *
 * ACTIVATION
 * ----------
 * Set the following in os_trace_config.h (or as a compiler define):
 * @code
 *   #define OS_RUN_INTEGRATION_TESTS  1
 * @endcode
 * main() then starts the three test tasks instead of sensor/proc/shell. The
 * HC-SR04 is not needed in this mode.
 *
 * EVALUATING THE RESULTS - two complementary ways:
 *
 * 1. UART (115200 8N1): every test case reports `[OK]` or `[FAIL]` together
 *    with a plain-text description, followed by a summary at the end. This is
 *    the quick visual check.
 *
 * 2. SystemView + TeSSLa: the tests deliberately reuse the same trace IDs as
 *    the application objects (MTX 1/2, SEM 1/2, Q 1/2), so the existing specs
 *    apply unchanged - a trace of a test run can be checked directly against
 *    mutex.tessla, semaphore.tessla and queue.tessla. Since the tests force
 *    the edge cases, this is the actual evidence that the rules hold under
 *    contention, rather than merely never having been violated for lack of
 *    opportunity.
 *
 * TASK SET IN TEST MODE
 * ---------------------
 * | Idx | Task          | Prio | Role                                      |
 * |-----|---------------|------|-------------------------------------------|
 * | 0   | TestHighTask  | 3    | high-priority contender (priority order)   |
 * | 1   | TestMainTask  | 1    | test sequencing, evaluation, UART output   |
 * | 2   | TestPeerTask  | 1    | equal-priority contender (round-robin)     |
 * | 3   | IdleTask      | 0    | idle                                       |
 *
 ******************************************************************************
 */

#ifndef TESTS_H
#define TESTS_H

#include <stdint.h>

/**
 * @brief Initialise the test objects (mutex, semaphore, queue).
 * @author __________
 *
 * Call instead of App_Resources_Init() when #OS_RUN_INTEGRATION_TESTS is
 * active. Additionally creates a small test queue with capacity
 * #TEST_QUEUE_CAPACITY, so that filling it up can be forced in reasonable
 * time.
 */
void Tests_vInitResources(void);

/**
 * @brief High-priority contender used for the priority-ordering scenarios.
 * @author __________
 *
 * Runs in task slot 0 (priority 3).
 */
void TestHighTask(void);

/**
 * @brief Main test task: drives the test phases, evaluates results and prints
 *        them over UART.
 * @author __________
 *
 * Runs in task slot 1 (priority 1).
 */
void TestMainTask(void);

/**
 * @brief Equal-priority partner used for the contention and round-robin
 *        scenarios.
 * @author __________
 *
 * Runs in task slot 2 (priority 1).
 */
void TestPeerTask(void);

/** @brief Capacity of the test queue - deliberately small so that "full" is
 *         reached quickly. */
#define TEST_QUEUE_CAPACITY   ( 4u )

/** @brief Number of messages sent in the data integrity test. */
#define TEST_MSG_COUNT        ( 16u )

#endif /* TESTS_H */
