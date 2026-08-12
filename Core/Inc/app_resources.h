/**
 ******************************************************************************
 * @file    app_resources.h
 * @brief   Central declaration of all shared kernel objects of the application.
 * @author  Andre
 ******************************************************************************
 *
 * Every queue, mutex and semaphore used by more than one module is declared
 * here and defined once in app_resources.c. This keeps ownership of the shared
 * state in a single place instead of scattering globals across the modules.
 *
 * @see app_resources.c for the definitions, buffer sizes and trace IDs.
 *
 ******************************************************************************
 */

#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include "os_queue.h"
#include "os_mutex.h"
#include "os_semaphore.h"
#include "app_messages.h"

/** @brief Raw measurements: SensorTask -> ProcTask. Carries SensorData_t. */
extern OS_Queue_t g_sensorQueue;
/** @brief Calibrated results: ProcTask -> UartShellTask. Carries ProcessedData_t. */
extern OS_Queue_t g_processedQueue;

/** @brief Guards the UART so output from different tasks cannot interleave. */
extern OS_Mutex_t g_uartMutex;
/** @brief Guards g_userConfig against concurrent read/modify access. */
extern OS_Mutex_t g_configMutex;

/** @brief Signalled by the HC-SR04 echo ISR when a pulse has been measured. */
extern OS_Semaphore_t g_echoDoneSemaphore;
/** @brief Signalled by the UART RX ISR when a complete input line is ready. */
extern OS_Semaphore_t g_uartRxSemaphore;

/** @brief User and calibration configuration.
 *  @warning Access ONLY while holding g_configMutex! */
extern User_Data_t g_userConfig;

/**
 * @brief Initialise all shared queues, mutexes and semaphores.
 * @author Andre
 *
 * @note Must run before the scheduler starts, i.e. before any task can touch
 *       one of these objects.
 */
void App_Resources_Init(void);

#endif
