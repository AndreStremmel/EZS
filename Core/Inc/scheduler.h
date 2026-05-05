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

void Scheduler_vInit(TCB_sctTCB_t* pFirstTask);
TCB_sctTCB_t* Scheduler_pGetNextTask(void);

#endif