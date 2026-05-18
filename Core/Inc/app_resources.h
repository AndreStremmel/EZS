#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include "os_queue.h"
#include "os_mutex.h"
#include "app_messages.h"

extern OS_Queue_t g_sensorQueue;
extern OS_Queue_t g_processedQueue;

extern OS_Mutex_t g_i2cMutex;
extern OS_Mutex_t g_uartMutex;

void App_Resources_Init(void);

#endif