#include "os_semaphore.h"

void OS_Semaphore_Init(OS_Semaphore_t *sem, uint8_t initialCount, uint8_t maxCount)
{
    sem->u8Count = initialCount;
    sem->u8MaxCount = maxCount;
}

uint8_t OS_Semaphore_Take(OS_Semaphore_t *sem)
{
    if (sem->u8Count > 0)
    {
        sem->u8Count--;
        return 1;
    }

    return 0;
}

void OS_Semaphore_Give(OS_Semaphore_t *sem)
{
    if (sem->u8Count < sem->u8MaxCount)
    {
        sem->u8Count++;
    }
}