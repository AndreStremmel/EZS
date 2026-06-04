/*
 * tasks.c
 *  Task definition
 *  Created on: May 2, 2026
 *      Author: André Stremmel
 */

#include "tasks.h"
#include "stm32l4xx_hal.h"


TCB_sctTCB_t tasks[NUM_TASKS] =
{
    {
        .u8TaskId = 1,
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 2,
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 3,
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 4,
        .u8TaskPrio = 0,
        .eTaskState = TaskState_Ready,
        .u32TaskSP = 0
    }
};

void Task1(void)
{
    while (1)
    {
		HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_9);
		Scheduler_vBlockedDelay(500);
    }
}

void Task2(void)
{
    while (1)
    {
		HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_8);
		Scheduler_vNonBlockedDelay(200);
    }
}

void Task3(void)
{
    while (1)
    {
		HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_10);
		Scheduler_vNonBlockedDelay(700);
    }
}

void IdleTask(void)
{
	while (1) {}
}
