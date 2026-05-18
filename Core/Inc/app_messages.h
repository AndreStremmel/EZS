#ifndef APP_MESSAGES_H
#define APP_MESSAGES_H

#include <stdint.h>

typedef struct
{
    float temperature;
    float humidity;
    uint32_t timestamp_ms;
} SensorData_t;

typedef struct
{
    float temperature_avg;
    float humidity_avg;
    uint32_t timestamp_ms;
} ProcessedData_t;

#endif