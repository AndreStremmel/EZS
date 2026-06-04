// Initialize stack
#ifndef STACK_H
#define STACK_H

#include "tcb.h"

void Stack_vInit(TCB_sctTCB_t* pTcb, void (*pvTaskPointer)(void));

#endif
