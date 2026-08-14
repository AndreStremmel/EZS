/**
 ******************************************************************************
 * @file    tasks.h
 * @brief   Application task set of the final project - declarations.
 * @author  Andre
 ******************************************************************************
 *
 * Index convention (IMPORTANT for both the scheduler and the TeSSLa analysis):
 *   tasks[0] = SensorTask     (prio 3)
 *   tasks[1] = ProcTask       (prio 1)  \ round-robin pair
 *   tasks[2] = UartShellTask  (prio 1)  /
 *   tasks[3] = IdleTask       (prio 0)  - MUST remain the last entry!
 *
 * The scheduler relies on the idle task being the last slot: it is the
 * fallback that is dispatched when nothing else is ready, and by convention it
 * never blocks.
 *
 * The two priority-1 tasks exist as a pair on purpose - they are what the
 * round-robin verification rule is checked against.
 *
 ******************************************************************************
 */

#ifndef TASKS_H
#define TASKS_H

#include "tcb.h"
#include "scheduler.h"

/** @brief Number of tasks in the system, including the idle task. */
#define NUM_TASKS 4

/** @brief Task control blocks of all tasks, indexed as documented above. */
extern TCB_sctTCB_t tasks[NUM_TASKS];

/**
 * @brief Periodically triggers the HC-SR04 and publishes raw measurements.
 * @author Andre
 *
 * Sends SensorData_t messages into g_sensorQueue.
 */
void SensorTask(void);

/**
 * @brief Converts raw measurements into calibrated distances.
 * @author Andre
 *
 * Receives from g_sensorQueue, sends ProcessedData_t into g_processedQueue.
 */
void ProcTask(void);

/**
 * @brief Prints processed measurements over UART and services the shell.
 * @author Andre
 *
 * Receives from g_processedQueue and polls the shell for user input.
 */
void UartShellTask(void);

/**
 * @brief Lowest priority fallback task that runs when nothing else is ready.
 * @author Andre
 *
 * @note Never blocks - the scheduler depends on it always being runnable.
 */
void IdleTask(void);

/**
 * @brief Populate the tasks[] array with IDs, priorities and initial states.
 * @author Andre
 *
 * @note Must be called BEFORE Stack_vInit(), since the stack setup relies on
 *       the TCB fields filled in here.
 */
void Tasks_vInitTaskArray(void);

#endif
