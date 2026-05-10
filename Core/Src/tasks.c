/**
  ******************************************************************************
  * @file           : tasks.c
  * @brief          : Task definition
  * @author         : André Stremmel
  ******************************************************************************
*/

#include "tasks.h"
#include "stm32l4xx_hal.h"


TCB_sctTCB_t tasks[NUM_TASKS] =
{
    {
        .u8TaskId = 1,
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .pfTaskFunction = Task1,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 2,
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .pfTaskFunction = Task2,
        .u32TaskSP = 0
    }
};

void Task1(void)
{
    while (1)
    {
        // Task 1: LED LD2 leuchtet dauerhaft
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);

        // Nochmal eine LED toggeln, damit man sieht, dass die beiden Tasks funktionieren
        HAL_Delay(200);

        
    }
}

void Task2(void)
{
    while (1)
    {
		// LD2 toggeln
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);

		// Verzögerung
		HAL_Delay(200);
    }
}