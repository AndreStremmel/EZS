#include "os_mutex.h"

void OS_Mutex_Init(OS_Mutex_t *mutex)
{
    mutex->u8Locked = 0;
    mutex->u8OwnerTaskId = 0;
}

uint8_t OS_Mutex_Lock(OS_Mutex_t *mutex, uint8_t taskId)
{
    if (mutex->u8Locked == 0)
    {
        mutex->u8Locked = 1;
        mutex->u8OwnerTaskId = taskId;
        return 1;
    }

    return 0;
}

void OS_Mutex_Unlock(OS_Mutex_t *mutex, uint8_t taskId)
{
    if (mutex->u8Locked && mutex->u8OwnerTaskId == taskId)
    {
        mutex->u8Locked = 0;
        mutex->u8OwnerTaskId = 0;
    }
}