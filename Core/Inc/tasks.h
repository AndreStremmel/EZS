#ifndef TASKS_H
#define TASKS_H

#include "tcb.h"
#include "scheduler.h"

#define NUM_TASKS 4

extern TCB_sctTCB_t tasks[NUM_TASKS];

void Task1(void);
void Task2(void);
void Task3(void);
void IdleTask(void);

#endif
