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
    CPSID   I                @ Disable interrupts

    @ ---- Save the CURRENT task's context ----
    MRS     R0, PSP          @ R0 = current task's stack pointer (PSP).
    STMDB   R0!, {R4-R11}    @ Push R4-R11 onto the task's stack.
                             @ STMDB = "Store Multiple, Decrement Before"
                             @ The "!" updates R0 to the new top of stack.

    LDR     R1, =g_pCurrentTask  @ R1 = address of the g_pCurrentTask variable
    LDR     R1, [R1]             @ R1 = value of g_pCurrentTask (= pointer to TCB)

    @ NOTE: u32TaskSP is the FIRST field of the TCB struct, so its offset is 0.
    @ If u32TaskSP were the last field, we'd need: ADD R1, R1, #520
    STR     R0, [R1]         @ Save the updated stack pointer into the TCB.

    @ ---- Load the NEXT task's context ----
    LDR     R1, =g_pNextTask     @ R1 = address of g_pNextTask
    LDR     R1, [R1]             @ R1 = pointer to next task's TCB
    LDR     R0, [R1]             @ R0 = next task's saved stack pointer

    LDMIA   R0!, {R4-R11}    @ Pop R4-R11 from the next task's stack.
                             @ LDMIA = "Load Multiple, Increment After"
    MSR     PSP, R0          @ Update PSP to point at the hardware-saved frame.
                             @ When we return, hardware will pop R0-R3, R12, LR,
                             @ PC, xPSR from this address automatically.

    LDR     R2, =g_pCurrentTask
    STR     R1, [R2]         @ g_pCurrentTask = g_pNextTask

    CPSIE   I                @ Re-enable interrupts.
    BX      LR               @ Exception return
