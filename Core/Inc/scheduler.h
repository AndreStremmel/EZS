/*
 * scheduler.h
 *  Simple Scheduler 
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "tcb.h"
#include "tasks.h"
#include "stm32f4xx_hal.h"

extern TCB_sctTCB_t* g_pCurrentTask;
extern TCB_sctTCB_t* g_pNextTask;

void Scheduler_vInit(void);
TCB_sctTCB_t* Scheduler_pGetNextTask(void);

#endif