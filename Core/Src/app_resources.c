#include "app_resources.h"
#include "os_trace.h"

#define SENSOR_QUEUE_LENGTH     8
#define PROCESSED_QUEUE_LENGTH  8

static uint8_t sensorQueueBuffer[SENSOR_QUEUE_LENGTH * sizeof(SensorData_t)];
static uint8_t processedQueueBuffer[PROCESSED_QUEUE_LENGTH * sizeof(ProcessedData_t)];

OS_Queue_t g_sensorQueue;
OS_Queue_t g_processedQueue;

OS_Mutex_t g_uartMutex;
OS_Mutex_t g_configMutex;

OS_Semaphore_t g_echoDoneSemaphore;
OS_Semaphore_t g_uartRxSemaphore;

User_Data_t g_userConfig;

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

    OS_Semaphore_Init(&g_echoDoneSemaphore, 0u, 1u, OS_TRACE_SEM_ECHO);
    OS_Semaphore_Init(&g_uartRxSemaphore,   0u, 1u, OS_TRACE_SEM_UARTRX);

    /* Startkonfiguration: Kalibrierung neutral und aktiv */
    g_userConfig.active                    = 1u;
    g_userConfig.freq                      = 100u;   /* Messperiode in ms */
    g_userConfig.calibrationOffsetMm       = 0;
    g_userConfig.calibrationFactorPermille = 1000u;
}
