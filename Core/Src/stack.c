/**
 ******************************************************************************
 * @file    stack.c
 * @brief   Initial task stack frame setup - implementation.
 * @author  Berkay
 ******************************************************************************
 *
 * Prepares the initial stack frame of a task so that it looks exactly as if
 * the task had just been interrupted by PendSV. The first "return" into the
 * task is therefore an ordinary context switch, with no special-casing needed
 * anywhere in the scheduler or the PendSV handler.
 *
 * Layout (16 words, from high to low addresses):
 *   - 8 words hardware frame: xPSR, PC, LR, R12, R3, R2, R1, R0
 *     (unstacked by the hardware itself on exception return)
 *   - 8 words software frame: R11..R4
 *     (popped by our PendSV handler in assembly, see pendsv.s)
 *
 ******************************************************************************
 */

#include "stack.h"

/**
 * @brief Prepare the initial stack frame of a task.
 * @param pTcb          TCB of the task; u32TaskSP is set to the top of the
 *                      prepared frame.
 * @param pvTaskPointer Entry function of the task, placed in the PC slot.
 * @author Berkay
 */
void Stack_vInit(TCB_sctTCB_t* pTcb, void (*pvTaskPointer)(void))
{
    uint32_t u32Index = TCB_TASK_STACK_SIZE - 1;

    // xPSR: the Thumb bit (bit 24) MUST be set - Cortex-M only executes Thumb
    // code, and without the T bit the exception return would fault immediately.
    pTcb->au32TaskStack[u32Index--] = 0x01000000;

    // PC: entry address of the task function
    pTcb->au32TaskStack[u32Index--] = (uint32_t)pvTaskPointer;

    // LR of the TASK (not EXC_RETURN!): this slot is loaded into the task's LR
    // register during hardware unstacking and would be the return address if
    // the task function ever returned. Our tasks are endless loops, so the
    // value 0xFFFFFFF9 acts as a deliberate crash trap - branching there in
    // thread mode raises a fault instead of jumping off into nowhere. The real
    // EXC_RETURN lives in the LR REGISTER during PendSV and never comes from
    // the task stack.
    pTcb->au32TaskStack[u32Index--] = 0xFFFFFFF9;

    // Remainder of the hardware frame plus the software frame, all zeroed:
    //   R12, R3, R2, R1, R0          (unstacked by the hardware)
    //   R11, R10, R9, R8, R7, R6, R5, R4  (popped by PendSV)
    for (uint8_t i = 0; i < 13; i++) {
        pTcb->au32TaskStack[u32Index--] = 0x00000000;
    }

    // The stored SP points at the lowest word of the frame (R4) - exactly the
    // state that "PUSH {R4-R11}" leaves behind in PendSV.
    pTcb->u32TaskSP = (uint32_t)(&pTcb->au32TaskStack[u32Index + 1]);
}
