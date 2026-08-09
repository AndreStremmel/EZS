/**
 ******************************************************************************
 * @file    scheduler.h
 * @brief   Cooperative/preemptive priority scheduler with round-robin - API.
 * @author  __________
 * @date    Created on: May 3, 2026
 ******************************************************************************
 *
 * The scheduler picks the runnable task with the highest priority. If several
 * ready tasks share that priority, they are served round-robin starting from
 * the slot after the one that is currently running, so no task of that group
 * can starve the others.
 *
 * Task states are held in the TCB (see tcb.h):
 *   Ready   - runnable, waiting to be dispatched
 *   Running - currently executing (exactly one task at a time)
 *   Blocked - waiting for a delay to expire or for a kernel object
 *
 * The actual context switch is not performed here: Scheduler_pGetNextTask()
 * only selects the task and publishes it via g_pNextTask. The SysTick handler
 * then pends a PendSV exception, and the PendSV handler (see pendsv.s) does
 * the register save/restore at the lowest interrupt priority.
 *
 * This module also owns the per-task delay countdown, which is used both by
 * Scheduler_vNonBlockedDelay() and by the timeout variants of the kernel
 * objects (semaphore, mutex, queue).
 *
 ******************************************************************************
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "tcb.h"
#include "tasks.h"
#include "stm32l4xx_hal.h"


/** @brief Task that is currently executing. Read by PendSV to know where to
 *         save the outgoing context. NULL until Scheduler_vInit() ran. */
extern TCB_sctTCB_t* g_pCurrentTask;

/** @brief Task that has been selected to run next. Read by PendSV to know
 *         which context to restore. */
extern TCB_sctTCB_t* g_pNextTask;

/**
 * @brief  Select the next task to run and publish it in g_pNextTask.
 * @return Pointer to the TCB of the selected task.
 * @author __________
 *
 * Called from the SysTick handler on every tick. Picks the highest priority
 * among all Ready tasks, then round-robins within that priority level.
 * Also updates the task states and emits the corresponding trace events.
 */
TCB_sctTCB_t* Scheduler_pGetNextTask(void);

/**
 * @brief Bring the scheduler into a defined state so the first tick can
 *        dispatch tasks[0].
 * @author __________
 *
 * Must be called after the task table has been set up and before the first
 * context switch is expected.
 */
void Scheduler_vInit(void);

/**
 * @brief Decrement the delay counter of every blocked task by one tick and
 *        move tasks whose counter reaches zero back to Ready.
 * @author __________
 *
 * Called from the SysTick handler once per millisecond.
 */
void Scheduler_vCountdown(void);

/**
 * @brief Busy-wait delay: the calling task keeps the CPU for the whole time.
 * @param ticks Delay in SysTick ticks (milliseconds).
 * @author __________
 *
 * The task stays Running, so it can still be preempted by a higher priority
 * task, but it never yields voluntarily. Used to demonstrate the difference
 * to Scheduler_vNonBlockedDelay() in the trace.
 */
void Scheduler_vBlockedDelay(uint32_t ticks);

/**
 * @brief Blocking delay: the calling task goes to Blocked and releases the
 *        CPU for the whole time.
 * @param ticks Delay in SysTick ticks (milliseconds). 0 returns immediately.
 * @author __________
 */
void Scheduler_vNonBlockedDelay(uint32_t ticks);

/* ---- API used by the kernel objects (semaphore/mutex/queue) ------------- */

/**
 * @brief  Index of the currently running task within the tasks[] array.
 * @return Task index, usable as an ID for trace events and owner bookkeeping.
 * @author __________
 */
uint8_t Scheduler_u8GetCurrentTaskIdx(void);

/**
 * @brief Put the current task into the Blocked state.
 * @param u32TimeoutTicks OS_WAIT_FOREVER = wait indefinitely (only an explicit
 *                        unblock wakes the task up), otherwise the SysTick
 *                        countdown wakes it after the given number of ticks.
 * @param u32SysViewCause Reason code passed on to
 *                        SEGGER_SYSVIEW_OnTaskStopReady().
 * @author __________
 *
 * @warning MUST be called from within a critical section (interrupts
 *          disabled), otherwise an ISR could unblock the task between the
 *          state change and the caller's wait loop.
 */
void Scheduler_vBlockCurrentTask(uint32_t u32TimeoutTicks, uint32_t u32SysViewCause);

/**
 * @brief Wait until the current task has been made runnable again.
 * @author __________
 *
 * Call this after Scheduler_vBlockCurrentTask() and after leaving the critical
 * section: it spins until SysTick or an explicit unblock has moved the task
 * back to Ready/Running and the task has actually been dispatched again.
 */
void Scheduler_vWaitWhileBlocked(void);

/**
 * @brief Wake a task up (Blocked -> Ready). Does nothing if the task is not
 *        currently blocked.
 * @param u8TaskIdx Index of the task in the tasks[] array.
 * @author __________
 *
 * ISR-safe when called from within a critical section.
 */
void Scheduler_vUnblockTask(uint8_t u8TaskIdx);

/**
 * @brief  Remaining delay/timeout ticks of a task.
 * @param  u8TaskIdx Index of the task in the tasks[] array.
 * @return Remaining ticks, 0 if no countdown is running or the index is out
 *         of range. Used by the kernel objects to tell "woken up" from
 *         "timed out".
 * @author __________
 */
uint32_t Scheduler_u32GetRemainingDelay(uint8_t u8TaskIdx);

#endif
