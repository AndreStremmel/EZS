/*
 * tasks.h
 *  Task Header
 *  Created on: May 2, 2026
 *      Author: André Stremmel
 */

#ifndef TASKS_H
#define TASKS_H

#include "tcb.h"

#define NUM_TASKS 2

extern TCB_sctTCB_t tasks[NUM_TASKS];

void Task1(void);
void Task2(void);

#endif