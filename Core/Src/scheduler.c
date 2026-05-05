/*
 * scheduler.c
 *  Simple Scheduler 
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#include "scheduler.h"

extern TCB_sctTCB_t tasks[]; // Lists of tasks

static TCB_sctTCB_t* s_pNextTask; // Current running task

// Start the first task from the list
void Scheduler_vInit(TCB_sctTCB_t* pFirstTask) 
{
    pFirstTask->eTaskState = TaskState_Running;
    s_pNextTask = pFirstTask;
}

TCB_sctTCB_t* Scheduler_pGetNextTask(void) 
{
    uint8_t u8HighestPrio = 0;

    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].u8TaskPrio > u8HighestPrio && tasks[i].eTaskState == TaskState_Ready) {
            u8HighestPrio = tasks[i].u8TaskPrio;
        }
    }

    for (uint8_t i = 0; i < NUM_TASKS; i++) 
    {
        if (tasks[i].eTaskState == TaskState_Running) 
        {
            for (int j = 1; j <= NUM_TASKS; j++) 
            {
                uint8_t u8NextIndex = (i + j) % NUM_TASKS;

                if (tasks[u8NextIndex].eTaskState == TaskState_Ready && tasks[u8NextIndex].u8TaskPrio == u8HighestPrio) 
                {
                    s_pNextTask = &tasks[u8NextIndex];
                    tasks[i].eTaskState = TaskState_Ready;
                    tasks[u8NextIndex].eTaskState = TaskState_Running;
                    return s_pNextTask;
                }
            }
        }
    }
    return s_pNextTask;
}