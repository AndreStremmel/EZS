/*
 * tasks.c
 *  Endprojekt-Taskset: HC-SR04-Distanzmessung mit UART-Ausgabe und Shell.
 *
 *  Datenfluss:
 *    SensorTask --SensorData_t--> g_sensorQueue --> ProcTask
 *    ProcTask   --ProcessedData_t--> g_processedQueue --> UartShellTask
 *
 *  Prioritaeten:
 *    Sensor    = 3  (hoechste - haelt das 100ms-Raster)
 *    Proc      = 1  \  gleiche Prioritaet -> Round-Robin-Paar
 *    UartShell = 1  /  (fuer die RR-Verifikationsregel)
 *    Idle      = 0
 */

#include "tasks.h"
#include "scheduler.h"
#include "app_resources.h"
#include "os_queue.h"
#include "os_mutex.h"
#include "os_semaphore.h"
#include "os_trace.h"
#include "hcsr04.h"
#include "uart_driver.h"
#include "shell.h"
#include "SEGGER_SYSVIEW.h"
#include "stm32f3xx_hal.h"

TCB_sctTCB_t tasks[NUM_TASKS];

/* --------------------------------------------------------------------------
 * SensorTask (Prio 3)
 *  Stoesst alle 100ms (absolutes Raster!) eine HC-SR04-Messung an, wartet
 *  per Semaphore+Timeout auf die Echo-ISR und schickt das Ergebnis
 *  (oder den Fehler) in die Sensor-Queue.
 * -------------------------------------------------------------------------- */
void SensorTask(void)
{
    uint32_t u32NextWake = HAL_GetTick();

    for (;;)
    {
        /* Absolutes Zeitraster: Periode haengt NICHT von der Messdauer ab */
        u32NextWake += 100u;

        SensorData_t sData;
        sData.distance_mm = 0u;
        sData.error       = false;

        HCSR04_vStartMeasurement();

        if (OS_Semaphore_TakeTimeout(&g_echoDoneSemaphore,
                                     HCSR04_ECHO_TIMEOUT_TICKS) == OS_OK)
        {
            if (HCSR04_u8IsOutOfRange() != 0u)
            {
                sData.error = true;
                OS_Trace_Record1(OS_TRACE_EVT_SENSOR_ERR, SENSOR_ERR_OUT_OF_RANGE);
            }
            else
            {
                sData.distance_mm = (uint16_t)HCSR04_u32GetDistanceMmRaw();
            }
        }
        else
        {
            /* Kein Echo innerhalb des Timeouts -> Sensor-/Verkabelungsfehler */
            sData.error = true;
            OS_Trace_Record1(OS_TRACE_EVT_SENSOR_ERR, SENSOR_ERR_NO_ECHO);
        }

        sData.timestamp_ms = HAL_GetTick();

        /* Non-Blocking: der Sensor-Task darf sein Zeitraster nicht an einer
         * vollen Queue verlieren - ein verlorener Messwert ist verkraftbar,
         * eine verschobene Messung nicht. */
        (void)OS_Queue_SendNonBlocking(&g_sensorQueue, &sData);

        /* Bis zum naechsten Rasterpunkt schlafen */
        uint32_t u32Now = HAL_GetTick();
        if ((int32_t)(u32NextWake - u32Now) > 0)
        {
            Scheduler_vNonBlockedDelay(u32NextWake - u32Now);
        }
        else
        {
            u32NextWake = u32Now;   /* Raster verpasst -> neu aufsetzen */
        }
    }
}

/* --------------------------------------------------------------------------
 * ProcTask (Prio 1)
 *  Holt Rohdaten aus der Sensor-Queue, wendet die Kalibrierung an
 *  (unter g_configMutex) und reicht das Ergebnis weiter.
 * -------------------------------------------------------------------------- */
void ProcTask(void)
{
    for (;;)
    {
        SensorData_t sRaw;

        /* Blockierend: ohne Daten gibt es nichts zu tun */
        OS_Queue_ReceiveBlocking(&g_sensorQueue, &sRaw);

        ProcessedData_t sOut;
        sOut.error        = sRaw.error;
        sOut.timestamp_ms = sRaw.timestamp_ms;
        sOut.distance_mm  = sRaw.distance_mm;

        if (!sRaw.error)
        {
            OS_Mutex_LockBlocking(&g_configMutex);
            bool     bApply      = g_userConfig.active;
            uint16_t u16Factor   = g_userConfig.calibrationFactorPermille;
            int16_t  i16OffsetMm = g_userConfig.calibrationOffsetMm;
            OS_Mutex_Unlock(&g_configMutex);

            if (bApply)
            {
                /* mm = raw * Faktor/1000 + Offset (auf >= 0 begrenzt) */
                int32_t i32Mm = (int32_t)(((uint32_t)sRaw.distance_mm * u16Factor) / 1000u)
                                + i16OffsetMm;
                if (i32Mm < 0)      { i32Mm = 0; }
                if (i32Mm > 65535)  { i32Mm = 65535; }
                sOut.distance_mm = (uint16_t)i32Mm;
            }
        }

        OS_Queue_SendBlocking(&g_processedQueue, &sOut);
    }
}

/* --------------------------------------------------------------------------
 * UartShellTask (Prio 1)
 *  Gibt jede verarbeitete Messung ueber UART aus (alle 100ms), meldet
 *  Sensorfehler und bedient die interaktive Shell (help/cal/status).
 * -------------------------------------------------------------------------- */
void UartShellTask(void)
{
    char acLine[64];

    OS_Mutex_LockBlocking(&g_uartMutex);
    UART_SendString("\r\nDOS-RTOS Distanzmessung bereit - 'help' fuer Kommandos\r\n");
    OS_Mutex_Unlock(&g_uartMutex);

    for (;;)
    {
        ProcessedData_t sData;

        /* Timeout statt Blocking, damit die Shell auch ohne Messdaten
         * (z.B. Sensor abgezogen und Queue leer) reaktiv bleibt */
        if (OS_Queue_ReceiveTimeout(&g_processedQueue, &sData, 50u) == OS_OK)
        {
            /* Laufende Kalibrierung? Dann Wert dorthin fuettern */
            if (Shell_u8FeedCalibration(&sData) == 0u)
            {
                OS_Mutex_LockBlocking(&g_uartMutex);
                if (sData.error)
                {
                    UART_SendString("SENSORFEHLER: keine gueltige Messung\r\n");
                }
                else
                {
                    UART_SendString("Distanz: ");
                    UART_SendUInt(sData.distance_mm);
                    UART_SendString(" mm\r\n");
                }
                OS_Mutex_Unlock(&g_uartMutex);

                /* Verifikationsanker: genau EIN Event pro UART-Uebertragung -
                 * auch Fehlerframes zaehlen als Uebertragung (Wert 0xFFFF).
                 * TeSSLa prueft das 100ms-Raster auf diesem Stream. */
                OS_Trace_Record1(OS_TRACE_EVT_UART_TX_DIST,
                                 sData.error ? 0xFFFFu : sData.distance_mm);
            }
        }

        /* Shell: hat die RX-ISR eine komplette Zeile gemeldet? */
        if (OS_Semaphore_TakeNonBlocking(&g_uartRxSemaphore) == OS_OK)
        {
            while (UART_LineAvailable())
            {
                UART_ReadLine(acLine, sizeof(acLine));
                Shell_vHandleLine(acLine);
            }
        }
    }
}

/* --------------------------------------------------------------------------
 * IdleTask (Prio 0)
 *  Siehe README: main() sollte als letzte Zeile IdleTask() aufrufen,
 *  damit dieser Code tatsaechlich der Idle-Kontext ist.
 * -------------------------------------------------------------------------- */
void IdleTask(void)
{
    for (;;)
    {
        SEGGER_SYSVIEW_OnIdle();
        __WFI();   /* Bis zum naechsten Interrupt schlafen */
    }
}

void Tasks_vInitTaskArray(void)
{
    /* Idx 0: Sensor - hoechste Prioritaet (Zeitraster!) */
    tasks[0].u8TaskId   = 1u;
    tasks[0].u8TaskPrio = 3u;
    tasks[0].eTaskState = TaskState_Ready;

    /* Idx 1: Verarbeitung - Round-Robin-Paar mit der Shell */
    tasks[1].u8TaskId   = 2u;
    tasks[1].u8TaskPrio = 1u;
    tasks[1].eTaskState = TaskState_Ready;

    /* Idx 2: UART/Shell */
    tasks[2].u8TaskId   = 3u;
    tasks[2].u8TaskPrio = 1u;
    tasks[2].eTaskState = TaskState_Ready;

    /* Idx 3: Idle - MUSS der letzte Eintrag sein (Scheduler-Konvention) */
    tasks[3].u8TaskId   = 4u;
    tasks[3].u8TaskPrio = 0u;
    tasks[3].eTaskState = TaskState_Ready;
}
