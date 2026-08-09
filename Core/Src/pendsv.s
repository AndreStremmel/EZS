@ ******************************************************************************
@ @file    pendsv.s
@ @brief   PendSV exception handler - performs the actual context switch.
@ @author  __________
@ ******************************************************************************
@
@ The scheduler only selects the next task and publishes it in g_pNextTask;
@ the register save/restore happens here. PendSV runs at the lowest exception
@ priority, so the switch is deferred until all other pending interrupts have
@ been serviced and never preempts an ISR halfway through.
@
@ Only R4-R11 are handled explicitly: R0-R3, R12, LR, PC and xPSR are stacked
@ and unstacked by the hardware as part of exception entry and return.
@
@ ******************************************************************************

.syntax unified              @ ARM/Thumb syntax
.cpu cortex-m4               @ Target CPU
.thumb                       @ Use thumb instruction set

@ These pointers live in scheduler.c. We need them to know which task
@ is currently running and which task to switch to.
.extern g_pCurrentTask
.extern g_pNextTask

.global PendSV_Handler       @ Overriding the default one in stmxx_it.c file

.thumb_func                  @ Tell the assembler the next label is a Thumb function.
.type PendSV_Handler, %function

PendSV_Handler:
    CPSID   I               @ Disable interrupts - the switch must be atomic

    @ 1. Save registers R4-R11 onto the current stack (MSP)
    @    The hardware already stacked R0-R3, R12, LR, PC and xPSR on entry.
    PUSH {R4-R11}

    @ 2. Store the current SP in the TCB of the outgoing task
    @    u32TaskSP is the first member reached by the pointer, so no offset
    @    calculation is needed (see the commented-out ADD below).
    LDR R0, =g_pCurrentTask
    LDR R1, [R0]            @ R1 = g_pCurrentTask
    @ADD R1, R1, #520        @ Offset to u32TaskSP (u8Id(1)+u8Prio(1)+pad(2)+state(4)+stack(512))
    STR SP, [R1]

    @ 3. Update the current task pointer: the incoming task is now the current one
    LDR R2, =g_pNextTask
    LDR R2, [R2]            @ R2 = g_pNextTask
    STR R2, [R0]            @ g_pCurrentTask = g_pNextTask

    @ 4. Load the SP of the incoming task from its TCB
    @ADD R2, R2, #520
    LDR SP, [R2]

    @ 5. Restore R4-R11 from the new task's stack
    POP {R4-R11}

    CPSIE   I               @ Re-enable interrupts
    BX LR                   @ Exception return: LR holds EXC_RETURN, so the
                            @ hardware unstacks R0-R3/R12/LR/PC/xPSR of the
                            @ new task and resumes it
