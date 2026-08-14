/**
 ******************************************************************************
 * @file    stack.h
 * @brief   Initial task stack frame setup - API.
 * @author  Berkay
 ******************************************************************************
 *
 * Before a task can be dispatched for the very first time, its stack has to
 * look exactly as if the task had already been running and was interrupted by
 * PendSV. Stack_vInit() builds that artificial frame, which makes the first
 * entry into a task an ordinary context switch instead of a special case.
 *
 * @see stack.c for the exact frame layout.
 *
 ******************************************************************************
 */

#ifndef STACK_H
#define STACK_H

#include "tcb.h"

/**
 * @brief Prepare the initial stack frame of a task.
 * @param pTcb           TCB of the task; its stack pointer field is updated to
 *                       point at the top of the prepared frame.
 * @param pvTaskPointer  Entry function of the task, placed in the PC slot.
 * @author Berkay
 *
 * @note Call this after Tasks_vInitTaskArray() and before Scheduler_vInit().
 */
void Stack_vInit(TCB_sctTCB_t* pTcb, void (*pvTaskPointer)(void));

#endif
