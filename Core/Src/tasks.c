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
#include "app_resources.h"
#include "hc_sr04.h"
#include "scheduler.h"
#include "uart_driver.h"
#include "board_config.h"

#include <stdint.h>
#include <string.h>

static User_Data_t g_userConfig = {
    .active = true,
    .freq = 10,
    .calibrationOffsetMm = 0,
    .calibrationFactorPermille = 1000
};

TCB_sctTCB_t tasks[NUM_TASKS] =
{
    {
        .u8TaskId = 1,
        .u8TaskPrio = 3,
        .eTaskState = TaskState_Ready,
        .u32TaskSP = 0
    },
    {
        .u8TaskId = 2,
        .u8TaskPrio = 2,
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
        .u8TaskPrio = 1,
        .eTaskState = TaskState_Ready,
        .u32TaskSP = 0
    }
};

/* -------------------------------------------------------------------------- */

void Ultrasonic_Task(void)
{
    uint32_t last_ms = 0;   /* lebt auf dem Task-Stack – kein static nötig   */

    while (1)               /* Task darf NIE zurückkehren – while(1) ist Pflicht */
    {
        if ((HAL_GetTick() - last_ms) < 100)
        {
            continue;       /* noch nicht 100 ms vergangen – nächste Iteration */
        }
        last_ms = HAL_GetTick();

        /* active-Flag lesen */
        bool isActive = false;
        if (OS_Mutex_Lock(&g_configMutex, g_pCurrentTask->u8TaskId))
        {
            isActive = g_userConfig.active;
            OS_Mutex_Unlock(&g_configMutex, g_pCurrentTask->u8TaskId);
        }

        if (!isActive)
        {
            continue;   /* FIX: continue statt return – Task bleibt in while(1) */
        }

        SensorData_t data;
        data.timestamp_ms = HAL_GetTick();
        data.error        = false;
        data.distance_mm  = 0;

        HC_SR04_Trigger();

        uint32_t timeout_start = HAL_GetTick();
        while (!OS_Semaphore_Take(&g_echoDoneSemaphore))
        {
            if ((HAL_GetTick() - timeout_start) > 30)
            {
                data.error = true;
                break;
            }
        }

        if (!data.error)
        {
            uint32_t echo_us = HC_SR04_GetEchoPulseUs();
            data.distance_mm = (uint16_t)((echo_us * 343UL) / 2000UL);
        }

        OS_Queue_Send(&g_sensorQueue, &data);
    }
}

/* -------------------------------------------------------------------------- */

void Processing_Task(void)
{
    while (1)   /* FIX: while(1) fehlte – Task würde nach erstem Durchlauf zurückkehren */
    {
        SensorData_t sensorData;

        if (OS_Queue_Receive(&g_sensorQueue, &sensorData) != OS_QUEUE_OK)
        {
            continue;   /* Queue leer – nächste Runde */
        }

        ProcessedData_t processed;
        processed.timestamp_ms = sensorData.timestamp_ms;
        processed.error        = sensorData.error;
        processed.distance_mm  = sensorData.distance_mm;

        if (!sensorData.error)
        {
            if (OS_Mutex_Lock(&g_configMutex, g_pCurrentTask->u8TaskId))
            {
                float calibrated =
                    ((float)sensorData.distance_mm
                     * g_userConfig.calibrationFactorPermille / 1000.0f)
                    + g_userConfig.calibrationOffsetMm;

                if (calibrated < 0.0f) { calibrated = 0.0f; }

                processed.distance_mm = (uint16_t)calibrated;

                OS_Mutex_Unlock(&g_configMutex, g_pCurrentTask->u8TaskId);
            }
        }

        OS_Queue_Send(&g_processedQueue, &processed);
    }
}

/* -------------------------------------------------------------------------- */

void UART_Task(void)
{
    while (1)   /* FIX: while(1) fehlte */
    {
        ProcessedData_t data;

        if (OS_Queue_Receive(&g_processedQueue, &data) != OS_QUEUE_OK)
        {
            continue;
        }

        if (OS_Mutex_Lock(&g_uartMutex, g_pCurrentTask->u8TaskId))
        {
            if (data.error)
            {
                UART_SendString("ERROR: HC-SR04 communication failed\r\n");
            }
            else
            {
                UART_SendString("Distance: ");
                UART_SendUInt(data.distance_mm);
                UART_SendString(" mm\r\n");
            }

            OS_Mutex_Unlock(&g_uartMutex, g_pCurrentTask->u8TaskId);
        }
    }
}

/* -------------------------------------------------------------------------- */

static uint16_t ParseReferenceDistance(const char *cmd)
{
    const char *p = cmd + 9;
    while (*p == ' ') { p++; }

    uint16_t result = 0;
    while (*p >= '0' && *p <= '9')
    {
        result = (uint16_t)(result * 10u + (uint16_t)(*p - '0'));
        p++;
    }
    return result;
}

static void Calibration_Start(uint16_t referenceMm)
{
    if (referenceMm == 0u) { return; }

    HC_SR04_Trigger();

    uint32_t timeout_start = HAL_GetTick();
    while (!OS_Semaphore_Take(&g_echoDoneSemaphore))
    {
        if ((HAL_GetTick() - timeout_start) > 30u)
        {
            UART_SendString("Calibration ERROR: no echo\r\n");
            return;
        }
    }

    uint32_t echo_us = HC_SR04_GetEchoPulseUs();
    if (echo_us == 0u) { return; }

    uint32_t raw_mm = (echo_us * 343ul) / 2000ul;
    if (raw_mm == 0u) { return; }

    uint16_t factor = (uint16_t)((referenceMm * 1000ul) / raw_mm);

    if (OS_Mutex_Lock(&g_configMutex, g_pCurrentTask->u8TaskId))
    {
        g_userConfig.calibrationFactorPermille = factor;
        OS_Mutex_Unlock(&g_configMutex, g_pCurrentTask->u8TaskId);
    }
}

/* -------------------------------------------------------------------------- */

void Shell_Task(void)
{
    while (1)   /* FIX: while(1) fehlte */
    {
        if (!UART_LineAvailable())
        {
            continue;
        }

        char command[32];
        UART_ReadLine(command, sizeof(command));

        if (strcmp(command, "start") == 0)
        {
            if (OS_Mutex_Lock(&g_configMutex, g_pCurrentTask->u8TaskId))
            {
                g_userConfig.active = true;
                OS_Mutex_Unlock(&g_configMutex, g_pCurrentTask->u8TaskId);
            }
            UART_SendString("Measurement started\r\n");
        }
        else if (strcmp(command, "stop") == 0)
        {
            if (OS_Mutex_Lock(&g_configMutex, g_pCurrentTask->u8TaskId))
            {
                g_userConfig.active = false;
                OS_Mutex_Unlock(&g_configMutex, g_pCurrentTask->u8TaskId);
            }
            UART_SendString("Measurement stopped\r\n");
        }
        else if (strncmp(command, "calibrate", 9) == 0)
        {
            uint16_t referenceMm = ParseReferenceDistance(command);
            Calibration_Start(referenceMm);
            UART_SendString("Calibration done\r\n");
        }
        else
        {
            UART_SendString("Unknown command\r\n");
        }
    }
}
