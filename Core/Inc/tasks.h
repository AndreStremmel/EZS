/*
 * tasks.h
 *  Task Header
 *  Created on: May 2, 2026
 *      Author: André Stremmel
 */

#ifndef TASKS_H
#define TASKS_H

#include "tcb.h"

#define NUM_TASKS 3

extern TCB_sctTCB_t tasks[NUM_TASKS];

void Task_Sensor(void);
void Task_Processing(void);
void Task_Output(void);

#endif