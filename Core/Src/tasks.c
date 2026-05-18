/**
  ******************************************************************************
  * @file           : tasks.c
  * @brief          : Task definition
  * @author         : André Stremmel
  ******************************************************************************
*/
#include "tasks.h"
#include "app_messages.h"
#include "os_queue.h"
#include "os_mutex.h"

#include "stm32l4xx_hal.h"

TCB_sctTCB_t tasks[NUM_TASKS] =
{
    {
        .u8TaskId = 1,
        .u8TaskPrio = 3,
        .eTaskState = TaskState_Ready,
        .pfTaskFunction = Task_Sensor,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 2,
        .u8TaskPrio = 2,
        .eTaskState = TaskState_Ready,
        .pfTaskFunction = Task_Processing,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 3,
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .pfTaskFunction = Task_Output,
        .u32TaskSP = 0
    }
};

void Task_Sensor(void)
{
    SensorData_t data;

    while (1)
    {
        OS_Mutex_Lock(&g_i2cMutex);

        data.temperature = 23.5f;
        data.humidity = 45.0f;
        data.timestamp_ms = HAL_GetTick();

        OS_Mutex_Unlock(&g_i2cMutex);

        OS_Queue_Send(&g_sensorQueue, &data);

        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);

        OS_TaskDelay(1000);
    }
}
void Task_Processing(void)
{
    SensorData_t input;
    ProcessedData_t output;

    float temp_sum = 0.0f;
    float hum_sum = 0.0f;
    uint32_t count = 0;

    while (1)
    {
        if (OS_Queue_Receive(&g_sensorQueue, &input) == OS_QUEUE_OK)
        {
            temp_sum += input.temperature;
            hum_sum += input.humidity;
            count++;

            output.temperature_avg = temp_sum / count;
            output.humidity_avg = hum_sum / count;
            output.timestamp_ms = input.timestamp_ms;

            OS_Queue_Send(&g_processedQueue, &output);
        }
        else
        {
            OS_TaskDelay(10);
        }
    }
}

void Task_Output(void)
{
    ProcessedData_t data;

    while (1)
    {
        OS_Queue_Receive(&g_processedQueue, &data);

        OS_Mutex_Lock(&g_uartMutex);

        // TODO: später UART-Ausgabe
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);

        OS_Mutex_Unlock(&g_uartMutex);
    }
}