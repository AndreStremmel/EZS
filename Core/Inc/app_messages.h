/**
 ******************************************************************************
 * @file    app_messages.h
 * @brief   Message and configuration types exchanged between the tasks.
 * @author  Andre
 ******************************************************************************
 *
 * Kept in a separate header so that both the producers and the consumers of a
 * message can include the type definition without pulling in each other's
 * module headers.
 *
 * SensorData_t and ProcessedData_t currently have the same layout, but they
 * are deliberately kept as two distinct types: they travel through different
 * queues and carry different meanings (raw vs. calibrated distance), and
 * keeping them separate makes the compiler catch a mix-up.
 *
 ******************************************************************************
 */

#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Runtime configuration set by the user through the shell.
 *
 * @warning Shared between tasks - access ONLY while holding g_configMutex.
 */
typedef struct
{
    bool active;                        ///< Measurement output enabled
    uint16_t freq;                      ///< Measurement period / rate setting
    int16_t calibrationOffsetMm;        ///< Additive correction in mm, from `cal`
    uint16_t calibrationFactorPermille; ///< Multiplicative correction, 1000 = factor 1.000
} User_Data_t;

/**
 * @brief Raw measurement produced by SensorTask and sent through g_sensorQueue.
 */
typedef struct
{
    uint16_t distance_mm;   ///< Uncalibrated distance in mm
    bool error;             ///< true if the measurement failed (timeout / out of range)
    uint32_t timestamp_ms;  ///< HAL tick at which the measurement was taken
} SensorData_t;

/**
 * @brief Calibrated measurement produced by ProcTask and sent through
 *        g_processedQueue.
 */
typedef struct
{
    uint16_t distance_mm;   ///< Calibrated distance in mm
    bool error;             ///< true if the underlying measurement failed
    uint32_t timestamp_ms;  ///< Timestamp carried over from the raw measurement
} ProcessedData_t;
#endif
