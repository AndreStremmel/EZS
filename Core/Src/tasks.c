/**
 ******************************************************************************
 * @file    tasks.c
 * @brief   Application task set: HC-SR04 distance measurement with UART output
 *          and interactive shell - implementation.
 * @author  Andre
 ******************************************************************************
 *
 * Data flow:
 *   SensorTask --SensorData_t-->    g_sensorQueue    --> ProcTask
 *   ProcTask   --ProcessedData_t--> g_processedQueue --> UartShellTask
 *
 * Priorities:
 *   Sensor    = 3  (highest - it has to keep the 100 ms grid)
 *   Proc      = 1  \  equal priority -> round-robin pair
 *   UartShell = 1  /  (this is what the RR verification rule is checked on)
 *   Idle      = 0
 *
 * The pipeline is deliberately split into three tasks rather than one: the
 * measurement has a hard timing requirement, the calibration is pure
 * computation, and the UART output is slow. Separating them keeps the slow
 * parts from interfering with the timed part.
 *
 * @see tasks.h for the task index convention.
 *
 ******************************************************************************
 */

#include "tasks.h"
#include "os_trace_config.h"
#include "tests.h"
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
#include "stm32l4xx_hal.h"

/** @brief Task control blocks of all tasks. */
TCB_sctTCB_t tasks[NUM_TASKS];

/* --------------------------------------------------------------------------
 * SensorTask (prio 3)
 * -------------------------------------------------------------------------- */

/**
 * @brief Triggers a HC-SR04 measurement every 100 ms and publishes the result.
 * @author Andre
 *
 * Waits for the echo ISR through a semaphore with timeout and sends the result
 * (or the error) into the sensor queue. Uses an absolute time grid, so the
 * measurement period does not drift with the duration of a measurement.
 */
void SensorTask(void)
{
    uint32_t u32NextWake = HAL_GetTick();

    for (;;)
    {
        /* Absolute time grid: the period does NOT depend on how long the
         * measurement itself took */
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
                OS_TRACE_APP1(OS_TRACE_EVT_SENSOR_ERR, SENSOR_ERR_OUT_OF_RANGE);
            }
            else
            {
                sData.distance_mm = (uint16_t)HCSR04_u32GetDistanceMmRaw();
            }
        }
        else
        {
            /* No echo within the timeout -> sensor or wiring fault */
            sData.error = true;
            OS_TRACE_APP1(OS_TRACE_EVT_SENSOR_ERR, SENSOR_ERR_NO_ECHO);
        }

        sData.timestamp_ms = HAL_GetTick();

        /* Non-blocking on purpose: the sensor task must not lose its time grid
         * waiting on a full queue - dropping a sample is acceptable, shifting
         * the measurement schedule is not. */
        (void)OS_Queue_SendNonBlocking(&g_sensorQueue, &sData);

        /* Sleep until the next grid point */
        uint32_t u32Now = HAL_GetTick();
        if ((int32_t)(u32NextWake - u32Now) > 0)
        {
            Scheduler_vNonBlockedDelay(u32NextWake - u32Now);
        }
        else
        {
            u32NextWake = u32Now;   /* Grid point missed -> resynchronise */
        }
    }
}

/* --------------------------------------------------------------------------
 * ProcTask (prio 1)
 * -------------------------------------------------------------------------- */

/**
 * @brief Applies the calibration to raw measurements and forwards them.
 * @author Andre
 *
 * Reads from the sensor queue, applies offset and factor from g_userConfig
 * (under g_configMutex) and sends the result to the processed queue.
 */
void ProcTask(void)
{
    for (;;)
    {
        SensorData_t sRaw;

        /* Blocking: without data there is nothing to do */
        OS_Queue_ReceiveBlocking(&g_sensorQueue, &sRaw);

        ProcessedData_t sOut;
        sOut.error        = sRaw.error;
        sOut.timestamp_ms = sRaw.timestamp_ms;
        sOut.distance_mm  = sRaw.distance_mm;

        if (!sRaw.error)
        {
            /* Copy the config out under the mutex and release it immediately -
             * holding it across the arithmetic would block the shell for no
             * reason. */
            OS_Mutex_LockBlocking(&g_configMutex);
            bool     bApply      = g_userConfig.active;
            uint16_t u16Factor   = g_userConfig.calibrationFactorPermille;
            int16_t  i16OffsetMm = g_userConfig.calibrationOffsetMm;
            OS_Mutex_Unlock(&g_configMutex);

            if (bApply)
            {
                /* mm = raw * factor/1000 + offset, clamped to the uint16 range */
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
 * UartShellTask (prio 1)
 * -------------------------------------------------------------------------- */

/**
 * @brief Prints every processed measurement over UART and serves the shell.
 * @author Andre
 *
 * Outputs one line per measurement (every 100 ms), reports sensor errors and
 * handles the interactive commands (help/cal/status).
 */
void UartShellTask(void)
{
    char acLine[64];

    OS_Mutex_LockBlocking(&g_uartMutex);
    UART_SendString("\r\nDOS-RTOS Distanzmessung bereit - 'help' fuer Kommandos\r\n");
    OS_Mutex_Unlock(&g_uartMutex);

    for (;;)
    {
        ProcessedData_t sData;

        /* Timeout instead of blocking, so the shell stays responsive even when
         * no measurements arrive (e.g. sensor unplugged and queue empty) */
        if (OS_Queue_ReceiveTimeout(&g_processedQueue, &sData, 50u) == OS_OK)
        {
            /* Calibration running? Then feed the value into it instead */
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

                /* Verification anchor: exactly ONE event per UART transmission -
                 * error frames count as a transmission too (value 0xFFFF).
                 * TeSSLa checks the 100 ms grid on this stream. */
                OS_TRACE_APP1(OS_TRACE_EVT_UART_TX_DIST,
                                 sData.error ? 0xFFFFu : sData.distance_mm);
            }
        }

        /* Shell: has the RX ISR signalled a complete line? */
        if (OS_Semaphore_TakeNonBlocking(&g_uartRxSemaphore) == OS_OK)
        {
            /* Drain everything that arrived - the binary semaphore may have
             * collapsed several line endings into a single signal */
            while (UART_LineAvailable())
            {
                UART_ReadLine(acLine, sizeof(acLine));
                Shell_vHandleLine(acLine);
            }
        }
    }
}

/* --------------------------------------------------------------------------
 * IdleTask (prio 0)
 * -------------------------------------------------------------------------- */

/**
 * @brief Lowest priority fallback task; runs when nothing else is ready.
 * @author Andre
 *
 * @note See README: main() should call IdleTask() as its last statement, so
 *       that this code really is the idle context.
 */
void IdleTask(void)
{
    for (;;)
    {
        //OS_TRACE_IDLE();
//        __WFI();   /* Sleep until the next interrupt */
//    	SEGGER_SYSVIEW_OnIdle();
    }
}

/**
 * @brief Populate the tasks[] array with IDs, priorities and initial states.
 * @author Andre
 *
 * Two variants selected at compile time: the normal application task set, and
 * the test task set used when #OS_RUN_INTEGRATION_TESTS is active. Both keep
 * the same priority shape (one high-priority slot, an equal-priority pair,
 * idle last), so the scheduler behaves identically in either mode.
 *
 * @note Must be called BEFORE Stack_vInit().
 */
void Tasks_vInitTaskArray(void)
{
#if (OS_RUN_INTEGRATION_TESTS != 0)
    /* Test set: priorities chosen so that slot 0 provides the high-priority
     * contender and slots 1/2 form an equal-priority pair. */
    tasks[0].u8TaskId   = 1u;
    tasks[0].u8TaskPrio = 3u;
    tasks[0].eTaskState = TaskState_Ready;

    tasks[1].u8TaskId   = 2u;
    tasks[1].u8TaskPrio = 1u;
    tasks[1].eTaskState = TaskState_Ready;

    tasks[2].u8TaskId   = 3u;
    tasks[2].u8TaskPrio = 1u;
    tasks[2].eTaskState = TaskState_Ready;

    tasks[3].u8TaskId   = 4u;
    tasks[3].u8TaskPrio = 0u;
    tasks[3].eTaskState = TaskState_Ready;
    return;
#else
    /* Idx 0: sensor - highest priority (time grid!) */
    tasks[0].u8TaskId   = 1u;
    tasks[0].u8TaskPrio = 3u;
    tasks[0].eTaskState = TaskState_Ready;

    /* Idx 1: processing - round-robin pair with the shell */
    tasks[1].u8TaskId   = 2u;
    tasks[1].u8TaskPrio = 1u;
    tasks[1].eTaskState = TaskState_Ready;

    /* Idx 2: UART/shell */
    tasks[2].u8TaskId   = 3u;
    tasks[2].u8TaskPrio = 1u;
    tasks[2].eTaskState = TaskState_Ready;

    /* Idx 3: idle - MUST be the last entry (scheduler convention) */
    tasks[3].u8TaskId   = 4u;
    tasks[3].u8TaskPrio = 0u;
    tasks[3].eTaskState = TaskState_Ready;
#endif
}
