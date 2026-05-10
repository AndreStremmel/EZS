/*
 * stack.c
 *  Stack operations
 *  Created on: May 3, 2026
 *      Author: Berkay
 */

#include "stack.h"

void Stack_vInit(TCB_sctTCB_t* pTcb, void (*pvTaskPointer)(void))
{
    uint32_t u32Index = TCB_TASK_STACK_SIZE - 1;
    pTcb->au32TaskStack[u32Index--] = 0x01000000; // xPSR
    pTcb->au32TaskStack[u32Index--] = (uint32_t)pvTaskPointer; // PC
    pTcb->au32TaskStack[u32Index--] = 0xFFFFFFFD; // LR, D for PSP

    // R12, R3, R2, R1, R0, R11, R10, R9, R8, R7, R6, R5, R4
    for (uint8_t i = 0; i < 13; i++) {
        pTcb->au32TaskStack[u32Index--] = 0x00000000;
    }    

    pTcb->u32TaskSP = (uint32_t)(&pTcb->au32TaskStack[u32Index+1]);
}
