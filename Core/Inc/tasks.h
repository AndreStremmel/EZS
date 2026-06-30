/*
 * tasks.h
 *  Task Header
 *  Created on: May 2, 2026
 *      Author: André Stremmel
 */

#ifndef TASKS_H
#define TASKS_H

#include "tcb.h"

#define NUM_TASKS 4

extern TCB_sctTCB_t tasks[NUM_TASKS];

void Ultrasonic_Task(void);
void Processing_Task(void);
void UART_Task(void);
void Shell_Task(void);

#endif