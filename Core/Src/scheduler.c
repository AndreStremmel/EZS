/*
 * scheduler.c
 *  Simple Scheduler 
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#include "scheduler.h"

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
            return g_pNextTask;
        }
    }

    // Nothing else ready → keep running current
    g_pCurrentTask->eTaskState = TaskState_Running;
    g_pNextTask = g_pCurrentTask;
    return g_pNextTask;
}

void Scheduler_vCountdown() {
	for (uint8_t i = 0; i < NUM_TASKS; i++) {
		if (tasks[i].eTaskState == TaskState_Blocked && g_au32TaskDelay[i] > 0) {
			g_au32TaskDelay[i]--;
			if (g_au32TaskDelay[i] == 0) {
				tasks[i].eTaskState = TaskState_Ready;
			}
		}
	}
}

void Scheduler_vBlockedDelay(uint32_t ticks)
{
	uint32_t u32Start = HAL_GetTick();
	while ((HAL_GetTick() - u32Start) < ticks) {}

}

void Scheduler_vNonBlockedDelay(uint32_t ticks)
{
    uint8_t u8Idx = (uint8_t)(g_pCurrentTask - &tasks[0]);
    g_au32TaskDelay[u8Idx] = ticks;
    g_pCurrentTask->eTaskState = TaskState_Blocked;

    // Warte bis SysTick den Kontext wechselt
	while (g_pCurrentTask->eTaskState == TaskState_Blocked) {}
}
