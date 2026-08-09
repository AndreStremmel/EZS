/*
 * stack.c
 *  Bereitet den initialen Stack-Frame eines Tasks so vor, als waere der
 *  Task gerade durch PendSV unterbrochen worden. Der erste "Ruecksprung"
 *  in den Task ist damit ein ganz normaler Kontextwechsel.
 *
 *  Layout (16 Woerter, von hohen zu niedrigen Adressen):
 *    - 8 Woerter Hardware-Frame: xPSR, PC, LR, R12, R3, R2, R1, R0
 *      (entstapelt die Hardware beim Exception-Return selbst)
 *    - 8 Woerter Software-Frame: R11..R4
 *      (poppt unser PendSV-Handler per Assembler)
 *
 *  Logik unveraendert zur Originalversion - nur die Kommentare wurden
 *  fachlich korrigiert.
 */

#include "stack.h"

void Stack_vInit(TCB_sctTCB_t* pTcb, void (*pvTaskPointer)(void))
{
    uint32_t u32Index = TCB_TASK_STACK_SIZE - 1;

    // xPSR: Thumb-Bit (Bit 24) MUSS gesetzt sein - Cortex-M kennt nur
    // Thumb-Code; ohne T-Bit wuerde der Exception-Return sofort faulten.
    pTcb->au32TaskStack[u32Index--] = 0x01000000;

    // PC: Einsprungadresse der Task-Funktion
    pTcb->au32TaskStack[u32Index--] = (uint32_t)pvTaskPointer;

    // LR des TASKS (nicht EXC_RETURN!): Dieser Slot wird beim Hardware-
    // Unstacking ins LR-Register des Tasks geladen und waere die
    // Ruecksprungadresse, falls die Task-Funktion je per 'return'
    // zurueckkehrt. Unsere Tasks sind Endlosschleifen - der Wert
    // 0xFFFFFFF9 dient als bewusste Absturz-Falle (Sprung dorthin im
    // Thread-Modus loest einen Fault aus, statt wild ins Nirwana zu
    // springen). Der echte EXC_RETURN liegt waehrend PendSV im
    // LR-REGISTER und kommt nie vom Task-Stack.
    pTcb->au32TaskStack[u32Index--] = 0xFFFFFFF9;

    // Hardware-Frame-Rest + Software-Frame, alle mit 0 initialisiert:
    // R12, R3, R2, R1, R0  (Hardware entstapelt sie)
    // R11, R10, R9, R8, R7, R6, R5, R4  (PendSV poppt sie)
    for (uint8_t i = 0; i < 13; i++) {
        pTcb->au32TaskStack[u32Index--] = 0x00000000;
    }

    // Gespeicherter SP zeigt auf das unterste Wort des Frames (R4) -
    // exakt der Zustand nach "PUSH {R4-R11}" in PendSV.
    pTcb->u32TaskSP = (uint32_t)(&pTcb->au32TaskStack[u32Index + 1]);
}
