/*
 * scheduler.c
 *  Simple Scheduler
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#include "scheduler.h"
#include "os_common.h"
#include "SEGGER_SYSVIEW.h"
#include "os_trace.h"

/// Restliche Delay-/Timeout-Ticks pro Task. 0 = kein laufender Countdown.
/// Wird sowohl von Scheduler_vNonBlockedDelay als auch von den
/// Timeout-Varianten der Kernel-Objekte benutzt.
static uint32_t g_au32TaskDelay[NUM_TASKS];

TCB_sctTCB_t* g_pCurrentTask;  // Old task
TCB_sctTCB_t* g_pNextTask;     // New task

// Start the dummy task and set the first task as next
void Scheduler_vInit(void)
{
    g_pCurrentTask = &tasks[NUM_TASKS - 1];
    g_pNextTask    = &tasks[0];

    __enable_irq();
}

TCB_sctTCB_t* Scheduler_pGetNextTask(void)
{
    uint8_t u8HighestPrio = 0;

    // Mark current task ready (it's about to be preempted)
    if (g_pCurrentTask->eTaskState == TaskState_Running) {
        g_pCurrentTask->eTaskState = TaskState_Ready;
        SEGGER_SYSVIEW_OnTaskStopExec();
        SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)g_pCurrentTask);
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
            SEGGER_SYSVIEW_OnTaskStartExec((uint32_t)g_pNextTask);
            return g_pNextTask;
        }
    }

    // Nothing else ready -> keep running current.
    // Achtung: Wenn der aktuelle Task Blocked ist, darf er NICHT weiterlaufen.
    // Das kann praktisch nicht passieren, weil der Idle-Task (Prio 0) nie
    // blockiert und immer Ready ist - der Guard ist reine Defensive.
    if (g_pCurrentTask->eTaskState != TaskState_Blocked) {
        g_pCurrentTask->eTaskState = TaskState_Running;
        g_pNextTask = g_pCurrentTask;
    } else {
        // Sollte der Guard je greifen: nicht auf einem veralteten
        // g_pNextTask sitzen bleiben, sondern explizit den Idle-Task
        // (blockiert per Konvention nie) einlasten.
        g_pNextTask = &tasks[NUM_TASKS - 1];
        g_pNextTask->eTaskState = TaskState_Running;
    }
    SEGGER_SYSVIEW_OnTaskStartExec((uint32_t)g_pNextTask);
    return g_pNextTask;
}

void Scheduler_vCountdown(void) {
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].eTaskState == TaskState_Blocked && g_au32TaskDelay[i] > 0) {
            g_au32TaskDelay[i]--;
            if (g_au32TaskDelay[i] == 0) {
                tasks[i].eTaskState = TaskState_Ready;
                SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)&tasks[i]);
            }
        }
    }
}

void Scheduler_vBlockedDelay(uint32_t ticks)
{
    uint8_t u8Idx = Scheduler_u8GetCurrentTaskIdx();
    OS_Trace_Record2(OS_TRACE_EVT_BUSY_DELAY_START, u8Idx, ticks);

    uint32_t u32Start = HAL_GetTick();
    while ((HAL_GetTick() - u32Start) < ticks) {}

    OS_Trace_Record1(OS_TRACE_EVT_BUSY_DELAY_END, u8Idx);
}

void Scheduler_vNonBlockedDelay(uint32_t ticks)
{
    // ticks == 0 wuerde Delay 0 setzen -> der Countdown weckt den Task
    // nie wieder auf (Endlos-Block). Daher: sofort zurueckkehren.
    if (ticks == 0u)
    {
        return;
    }

    uint32_t u32PriMask = OS_u32EnterCritical();

    OS_Trace_Record2(OS_TRACE_EVT_DELAY_START, Scheduler_u8GetCurrentTaskIdx(), ticks);
    Scheduler_vBlockCurrentTask(ticks, 0u);

    OS_vExitCritical(u32PriMask);

    // Warte bis SysTick den Kontext wechselt und der Task spaeter
    // wieder eingelastet wird
    Scheduler_vWaitWhileBlocked();
}

/* ==========================================================================
 * API fuer Kernel-Objekte
 * ========================================================================== */

uint8_t Scheduler_u8GetCurrentTaskIdx(void)
{
    return (uint8_t)(g_pCurrentTask - &tasks[0]);
}

void Scheduler_vBlockCurrentTask(uint32_t u32TimeoutTicks, uint32_t u32SysViewCause)
{
    uint8_t u8Idx = Scheduler_u8GetCurrentTaskIdx();

    // 0 heisst hier "kein Countdown" -> Task wartet unendlich,
    // bis ihn jemand per Scheduler_vUnblockTask() weckt.
    g_au32TaskDelay[u8Idx] = (u32TimeoutTicks == OS_WAIT_FOREVER) ? 0u : u32TimeoutTicks;

    g_pCurrentTask->eTaskState = TaskState_Blocked;
    SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)g_pCurrentTask, u32SysViewCause);
}

void Scheduler_vWaitWhileBlocked(void)
{
    // volatile-Zugriff, damit der Compiler die Schleife bei -O1/-O2
    // nicht wegoptimiert (der State wird "von aussen" durch SysTick/ISR
    // veraendert).
    volatile TCB_eTaskStates_t* peState = &g_pCurrentTask->eTaskState;
    while (*peState == TaskState_Blocked) {
        __NOP();
    }
}

void Scheduler_vUnblockTask(uint8_t u8TaskIdx)
{
    if (u8TaskIdx < NUM_TASKS && tasks[u8TaskIdx].eTaskState == TaskState_Blocked) {
        // Hinweis: g_au32TaskDelay wird bewusst NICHT geloescht. Verliert der
        // geweckte Task das Rennen um die Ressource, blockiert er mit der
        // Restzeit erneut. Der Countdown pausiert automatisch, solange der
        // Task nicht Blocked ist.
        tasks[u8TaskIdx].eTaskState = TaskState_Ready;
        SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)&tasks[u8TaskIdx]);
    }
}

uint32_t Scheduler_u32GetRemainingDelay(uint8_t u8TaskIdx)
{
    return (u8TaskIdx < NUM_TASKS) ? g_au32TaskDelay[u8TaskIdx] : 0u;
}
