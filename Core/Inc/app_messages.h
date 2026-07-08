#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool active;
    uint16_t freq;
    int16_t calibrationOffsetMm;
    uint16_t calibrationFactorPermille; // 1000 = Faktor 1.000
} User_Data_t;
typedef struct
{
    uint16_t distance_mm;
    bool error;
    uint32_t timestamp_ms;
} SensorData_t;

typedef struct
{
    uint16_t distance_mm;
    bool error;
    uint32_t timestamp_ms;
} ProcessedData_t;
#endif