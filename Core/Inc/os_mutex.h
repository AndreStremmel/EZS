#ifndef OS_MUTEX_H
#define OS_MUTEX_H

#include <stdint.h>

typedef struct
{
    uint8_t u8Locked;
    uint8_t u8OwnerTaskId;
} OS_Mutex_t;

void OS_Mutex_Init(OS_Mutex_t *mutex);
uint8_t OS_Mutex_Lock(OS_Mutex_t *mutex, uint8_t taskId);
void OS_Mutex_Unlock(OS_Mutex_t *mutex, uint8_t taskId);

#endif