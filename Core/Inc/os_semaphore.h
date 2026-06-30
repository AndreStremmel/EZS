#ifndef OS_SEMAPHORE_H
#define OS_SEMAPHORE_H

#include <stdint.h>

typedef struct
{
    volatile uint8_t u8Count;
    uint8_t u8MaxCount;
} OS_Semaphore_t;

void OS_Semaphore_Init(OS_Semaphore_t *sem, uint8_t initialCount, uint8_t maxCount);
uint8_t OS_Semaphore_Take(OS_Semaphore_t *sem);
void OS_Semaphore_Give(OS_Semaphore_t *sem);

#endif