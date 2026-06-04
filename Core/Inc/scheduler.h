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

TCB_sctTCB_t* Scheduler_pGetNextTask(void);
void Scheduler_vInit(void);
void Scheduler_vBlockedDelay(uint32_t ticks);
void Scheduler_vNonBlockedDelay(uint32_t ticks);

#endif
