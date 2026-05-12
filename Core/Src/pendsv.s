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
    CPSID   I               @ Interrupts sperren

    @ 1. Sichere die Register R4-R11 auf den aktuellen Stack (MSP)
    PUSH {R4-R11}

    @ 2. Speichere den aktuellen SP im TCB des alten Tasks
    LDR R0, =g_pCurrentTask
    LDR R1, [R0]            @ R1 = g_pCurrentTask
    @ADD R1, R1, #520        @ Offset zu u32TaskSP (u8Id(1)+u8Prio(1)+Pad(2)+State(4)+Stack(512))
    STR SP, [R1]

    @ 3. Aktuellen Task-Pointer aktualisieren
    LDR R2, =g_pNextTask
    LDR R2, [R2]            @ R2 = g_pNextTask
    STR R2, [R0]            @ g_pCurrentTask = g_pNextTask

    @ 4. Lade den SP des neuen Tasks aus dessen TCB
    @ADD R2, R2, #520
    LDR SP, [R2]

    @ 5. Stelle R4-R11 vom neuen Stack wieder her
    POP {R4-R11}

    CPSIE   I               @ Interrupts wieder erlauben
    BX LR                   @ Rücksprung (nutzt 0xFFFFFFF9 vom Stack), branch and exchange (return from subroutine)
