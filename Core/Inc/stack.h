/*
 * stack.h
 *  Stack operations
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#ifndef STACK_H
#define STACK_H

#include "tcb.h"

void Stack_vInit(TCB_sctTCB_t* pTcb, void (*pvTaskFunction)(void));

#endif