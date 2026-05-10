/*
 * scheduler.c
 *  Simple Scheduler 
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#include "scheduler.h"

//extern TCB_sctTCB_t tasks[]; // Lists of tasks

TCB_sctTCB_t* g_pCurrentTask;  // Old task
TCB_sctTCB_t* g_pNextTask;     // New task


// Start the first task from the list
void Scheduler_vInit(void)
{
    tasks[0].eTaskState = TaskState_Running;
    tasks[1].eTaskState = TaskState_Ready;
    g_pCurrentTask = &tasks[0];
    g_pNextTask    = &tasks[0];

    // We need to dispatch the first Task too, otherwise the program doesnt know where to start
    __set_PSP(tasks[0].u32TaskSP + 8 * 4);
    __set_CONTROL(0x02);
    __ISB();

    __enable_irq();
    Task1();   // never returns

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
