/**
 ******************************************************************************
 * @file    scheduler.c
 * @brief   Priority scheduler with round-robin - implementation.
 * @author  Berkay
 * @date    Created on: May 3, 2026
 ******************************************************************************
 *
 * The scheduler itself never switches context. Scheduler_pGetNextTask() only
 * selects a task and publishes it in g_pNextTask; the SysTick handler then
 * pends PendSV, and pendsv.s performs the register save/restore.
 *
 * Boot guard: SysTick already runs from HAL_Init() onwards, while the
 * scheduler only becomes valid after Scheduler_vInit(). Both entry points that
 * SysTick can reach therefore check g_pCurrentTask for NULL and do nothing
 * until the scheduler has been initialised.
 *
 * @see scheduler.h for the API description.
 *
 ******************************************************************************
 */

#include "scheduler.h"
#include "os_common.h"
#include "SEGGER_SYSVIEW.h"
#include "os_trace.h"

/** @brief Remaining delay/timeout ticks per task. 0 = no countdown running.
 *  Used both by Scheduler_vNonBlockedDelay() and by the timeout variants of
 *  the kernel objects. */
static uint32_t g_au32TaskDelay[NUM_TASKS];

TCB_sctTCB_t* g_pCurrentTask;  // Task that is currently executing
TCB_sctTCB_t* g_pNextTask;     // Task selected to run next

/**
 * @brief Bring the scheduler into a defined state for the first dispatch.
 * @author Berkay
 *
 * g_pCurrentTask is set to the last slot (the idle task) so that the
 * round-robin search in Scheduler_pGetNextTask() starts at index 0.
 */
void Scheduler_vInit(void)
{
    // SysTick is already running - set both pointers atomically so that the
    // boot guard does not open on a half-initialised state.
    __disable_irq();
    g_pNextTask    = &tasks[0];
    g_pCurrentTask = &tasks[NUM_TASKS - 1];
    __enable_irq();
}

/**
 * @brief  Select the next task to run and publish it in g_pNextTask.
 * @return Pointer to the TCB of the selected task.
 * @author Berkay
 *
 * Two-stage selection: first find the highest priority among all Ready tasks,
 * then round-robin within that priority level starting at the slot after the
 * current one. Starting after the current slot is what prevents a task from
 * immediately re-electing itself and starving its equal-priority peers.
 */
TCB_sctTCB_t* Scheduler_pGetNextTask(void)
{
    // Boot guard (see Scheduler_vCountdown): before Scheduler_vInit() this
    // returns NULL, so the SysTick handler compares NULL != NULL and does not
    // trigger a PendSV.
    if (g_pCurrentTask == (TCB_sctTCB_t*)0) {
        return g_pNextTask;
    }
    uint8_t u8HighestPrio = 0;

    // Mark current task ready (it's about to be preempted)
    if (g_pCurrentTask->eTaskState == TaskState_Running) {
        g_pCurrentTask->eTaskState = TaskState_Ready;
        OS_TRACE_TASK_STOP_EXEC();
        OS_TRACE_TASK_START_READY(g_pCurrentTask);
    }

    // Find highest priority among Ready tasks
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].eTaskState == TaskState_Ready
            && tasks[i].u8TaskPrio > u8HighestPrio) {
            u8HighestPrio = tasks[i].u8TaskPrio;
        }
    }

    // Round-robin starting from the slot after current
    uint8_t u8CurIdx = (uint8_t)(g_pCurrentTask - &tasks[0]);
    for (uint8_t j = 1; j <= NUM_TASKS; j++) {
        uint8_t u8Idx = (u8CurIdx + j) % NUM_TASKS;
        if (tasks[u8Idx].eTaskState == TaskState_Ready
            && tasks[u8Idx].u8TaskPrio == u8HighestPrio) {
            tasks[u8Idx].eTaskState = TaskState_Running;
            g_pNextTask = &tasks[u8Idx];
            OS_TRACE_TASK_START_EXEC(g_pNextTask);
            return g_pNextTask;
        }
    }

    // Nothing else ready -> keep running current.
    // Note: if the current task is Blocked it must NOT continue to run. In
    // practice this cannot happen because the idle task (prio 0) never blocks
    // and is always Ready - the guard is purely defensive.
    if (g_pCurrentTask->eTaskState != TaskState_Blocked) {
        g_pCurrentTask->eTaskState = TaskState_Running;
        g_pNextTask = g_pCurrentTask;
    } else {
        // Should the guard ever trigger: do not stay on a stale g_pNextTask,
        // but explicitly dispatch the idle task (which by convention never
        // blocks).
        g_pNextTask = &tasks[NUM_TASKS - 1];
        g_pNextTask->eTaskState = TaskState_Running;
    }
    OS_TRACE_TASK_START_EXEC(g_pNextTask);
    return g_pNextTask;
}

/**
 * @brief Decrement the delay counter of every blocked task and wake those
 *        whose counter reaches zero.
 * @author Berkay
 *
 * Called from the SysTick handler once per tick.
 */
void Scheduler_vCountdown(void) {
    // Boot guard: SysTick already runs from HAL_Init() onwards, but the
    // scheduler only from Scheduler_vInit(). Before that: do nothing.
    if (g_pCurrentTask == (TCB_sctTCB_t*)0) {
        return;
    }
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].eTaskState == TaskState_Blocked && g_au32TaskDelay[i] > 0) {
            g_au32TaskDelay[i]--;
            if (g_au32TaskDelay[i] == 0) {
                tasks[i].eTaskState = TaskState_Ready;
                OS_TRACE_TASK_START_READY(&tasks[i]);
            }
        }
    }
}

/**
 * @brief Busy-wait delay: the calling task keeps the CPU for the whole time.
 * @param ticks Delay in SysTick ticks (milliseconds).
 * @author Berkay
 *
 * The task stays Running and can still be preempted by a higher priority task,
 * but it never yields voluntarily. Kept alongside the blocking variant so the
 * difference between the two shows up in the trace.
 */
void Scheduler_vBlockedDelay(uint32_t ticks)
{
    uint8_t u8Idx = Scheduler_u8GetCurrentTaskIdx();
    (void)u8Idx;   /* used by the trace macros only - see os_trace_config.h */
    OS_TRACE_DLY2(OS_TRACE_EVT_BUSY_DELAY_START, u8Idx, ticks);

    uint32_t u32Start = HAL_GetTick();
    while ((HAL_GetTick() - u32Start) < ticks) {}

    OS_TRACE_DLY1(OS_TRACE_EVT_BUSY_DELAY_END, u8Idx);
}

/**
 * @brief Blocking delay: the calling task releases the CPU for the whole time.
 * @param ticks Delay in SysTick ticks (milliseconds).
 * @author Berkay
 */
void Scheduler_vNonBlockedDelay(uint32_t ticks)
{
    // A tick count of 0 would set the delay to 0, which the countdown reads as
    // "no countdown running" - the task would never be woken again. Therefore:
    // return immediately.
    if (ticks == 0u)
    {
        return;
    }

    uint32_t u32PriMask = OS_u32EnterCritical();

    OS_TRACE_DLY2(OS_TRACE_EVT_DELAY_START, Scheduler_u8GetCurrentTaskIdx(), ticks);
    Scheduler_vBlockCurrentTask(ticks, 0u);

    OS_vExitCritical(u32PriMask);

    // Wait until SysTick switches the context and the task is dispatched again
    Scheduler_vWaitWhileBlocked();
}

/* ==========================================================================
 * API used by the kernel objects
 * ========================================================================== */

/**
 * @brief  Index of the currently running task within the tasks[] array.
 * @return Task index, usable as an ID for trace events and owner bookkeeping.
 * @author Berkay
 */
uint8_t Scheduler_u8GetCurrentTaskIdx(void)
{
    return (uint8_t)(g_pCurrentTask - &tasks[0]);
}

/**
 * @brief Put the current task into the Blocked state.
 * @param u32TimeoutTicks OS_WAIT_FOREVER = wait indefinitely, otherwise the
 *                        number of ticks after which SysTick wakes the task.
 * @param u32SysViewCause Reason code for SEGGER_SYSVIEW_OnTaskStopReady().
 * @author Berkay
 *
 * @warning Must be called from within a critical section.
 */
void Scheduler_vBlockCurrentTask(uint32_t u32TimeoutTicks, uint32_t u32SysViewCause)
{
    (void)u32SysViewCause;   /* used by the trace macros only - see os_trace_config.h */
    uint8_t u8Idx = Scheduler_u8GetCurrentTaskIdx();

    // 0 means "no countdown" here -> the task waits indefinitely until someone
    // wakes it via Scheduler_vUnblockTask().
    g_au32TaskDelay[u8Idx] = (u32TimeoutTicks == OS_WAIT_FOREVER) ? 0u : u32TimeoutTicks;

    g_pCurrentTask->eTaskState = TaskState_Blocked;
    OS_TRACE_TASK_STOP_READY(g_pCurrentTask, u32SysViewCause);
}

/**
 * @brief Wait until the current task has been made runnable again.
 * @author Berkay
 */
void Scheduler_vWaitWhileBlocked(void)
{
    // volatile access so the compiler does not optimise the loop away at
    // -O1/-O2 (the state is changed "from outside" by SysTick or an ISR).
    volatile TCB_eTaskStates_t* peState = &g_pCurrentTask->eTaskState;
    while (*peState == TaskState_Blocked) {
        __NOP();
    }
}

/**
 * @brief Wake a task up (Blocked -> Ready).
 * @param u8TaskIdx Index of the task in the tasks[] array.
 * @author Berkay
 *
 * Does nothing if the index is out of range or the task is not blocked.
 */
void Scheduler_vUnblockTask(uint8_t u8TaskIdx)
{
    if (u8TaskIdx < NUM_TASKS && tasks[u8TaskIdx].eTaskState == TaskState_Blocked) {
        // Note: g_au32TaskDelay is deliberately NOT cleared. If the woken task
        // loses the race for the resource, it blocks again with the remaining
        // time. The countdown pauses automatically while the task is not
        // Blocked.
        tasks[u8TaskIdx].eTaskState = TaskState_Ready;
        OS_TRACE_TASK_START_READY(&tasks[u8TaskIdx]);
    }
}

/**
 * @brief  Remaining delay/timeout ticks of a task.
 * @param  u8TaskIdx Index of the task in the tasks[] array.
 * @return Remaining ticks, or 0 if no countdown runs or the index is invalid.
 * @author Berkay
 */
uint32_t Scheduler_u32GetRemainingDelay(uint8_t u8TaskIdx)
{
    return (u8TaskIdx < NUM_TASKS) ? g_au32TaskDelay[u8TaskIdx] : 0u;
}
