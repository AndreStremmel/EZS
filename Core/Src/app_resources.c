/**
 ******************************************************************************
 * @file    app_resources.c
 * @brief   Definition and initialisation of all shared kernel objects.
 * @author  __________
 ******************************************************************************
 *
 * Central place where the application's queues, mutexes, semaphores and the
 * user configuration are defined. The queue storage is provided as static
 * buffers here, so the kernel itself needs no dynamic memory allocation.
 *
 * The trace IDs handed to each object come from os_trace.h and are what links
 * a runtime event back to a specific object in the TeSSLa specifications.
 *
 * @see app_resources.h for the declarations.
 *
 ******************************************************************************
 */

#include "app_resources.h"
#include "os_trace.h"

/** @brief Number of raw measurements the sensor queue can buffer. */
#define SENSOR_QUEUE_LENGTH     8
/** @brief Number of processed measurements the result queue can buffer. */
#define PROCESSED_QUEUE_LENGTH  8

/** @brief Static storage backing g_sensorQueue. */
static uint8_t sensorQueueBuffer[SENSOR_QUEUE_LENGTH * sizeof(SensorData_t)];
/** @brief Static storage backing g_processedQueue. */
static uint8_t processedQueueBuffer[PROCESSED_QUEUE_LENGTH * sizeof(ProcessedData_t)];

OS_Queue_t g_sensorQueue;
OS_Queue_t g_processedQueue;

OS_Mutex_t g_uartMutex;
OS_Mutex_t g_configMutex;

OS_Semaphore_t g_echoDoneSemaphore;
OS_Semaphore_t g_uartRxSemaphore;

User_Data_t g_userConfig;

/**
 * @brief Initialise all shared queues, mutexes, semaphores and the default
 *        user configuration.
 * @author __________
 *
 * @note Must run before the scheduler starts, i.e. before any task can access
 *       one of these objects.
 */
void App_Resources_Init(void)
{
    OS_Queue_Init(&g_sensorQueue,
                  sensorQueueBuffer,
                  sizeof(SensorData_t),
                  SENSOR_QUEUE_LENGTH,
                  OS_TRACE_Q_SENSOR);

    OS_Queue_Init(&g_processedQueue,
                  processedQueueBuffer,
                  sizeof(ProcessedData_t),
                  PROCESSED_QUEUE_LENGTH,
                  OS_TRACE_Q_PROCESSED);

    OS_Mutex_Init(&g_uartMutex,   OS_TRACE_MTX_UART);
    OS_Mutex_Init(&g_configMutex, OS_TRACE_MTX_CONFIG);

    /* Both semaphores are used for signalling, hence initial count 0 and
     * max count 1 (binary): the ISR signals, the task waits. */
    OS_Semaphore_Init(&g_echoDoneSemaphore, 0u, 1u, OS_TRACE_SEM_ECHO);
    OS_Semaphore_Init(&g_uartRxSemaphore,   0u, 1u, OS_TRACE_SEM_UARTRX);

    /* Start-up configuration: calibration neutral, measuring enabled */
    g_userConfig.active                    = 1u;
    g_userConfig.freq                      = 100u;   /* measurement period in ms */
    g_userConfig.calibrationOffsetMm       = 0;
    g_userConfig.calibrationFactorPermille = 1000u;  /* 1000 = factor 1.000 */
}
