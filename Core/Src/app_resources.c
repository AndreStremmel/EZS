#include "app_resources.h"

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

void App_Resources_Init(void)
{
    OS_Queue_Init(&g_sensorQueue,
                  sensorQueueBuffer,
                  sizeof(SensorData_t),
                  SENSOR_QUEUE_LENGTH);

    OS_Queue_Init(&g_processedQueue,
                  processedQueueBuffer,
                  sizeof(ProcessedData_t),
                  PROCESSED_QUEUE_LENGTH);

    OS_Mutex_Init(&g_uartMutex);
    OS_Mutex_Init(&g_configMutex);

    OS_Semaphore_Init(&g_echoDoneSemaphore, 0, 1);
    OS_Semaphore_Init(&g_uartRxSemaphore, 0, 1);
}