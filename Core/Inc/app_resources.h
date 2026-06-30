#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include "os_queue.h"
#include "os_mutex.h"
#include "os_semaphore.h"
#include "app_messages.h"

extern OS_Queue_t g_sensorQueue;
extern OS_Queue_t g_processedQueue;

extern OS_Mutex_t g_uartMutex;
extern OS_Mutex_t g_configMutex;

extern OS_Semaphore_t g_echoDoneSemaphore;
extern OS_Semaphore_t g_uartRxSemaphore;

void App_Resources_Init(void);

#endif