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
#include "stm32l4xx_hal.h"


extern TCB_sctTCB_t* g_pCurrentTask;
extern TCB_sctTCB_t* g_pNextTask;

TCB_sctTCB_t* Scheduler_pGetNextTask(void);
void Scheduler_vInit(void);
void Scheduler_vCountdown(void);
void Scheduler_vBlockedDelay(uint32_t ticks);
void Scheduler_vNonBlockedDelay(uint32_t ticks);

/* ---- API fuer Kernel-Objekte (Semaphore/Mutex/Queue) -------------------- */

/// Index des aktuell laufenden Tasks im tasks[]-Array
uint8_t Scheduler_u8GetCurrentTaskIdx(void);

/**
 * @brief Aktuellen Task in den Blocked-State versetzen.
 *        MUSS im kritischen Abschnitt (Interrupts gesperrt) aufgerufen werden!
 * @param u32TimeoutTicks  0 = unendlich warten (nur Unblock weckt auf),
 *                         sonst: SysTick-Countdown weckt nach Ablauf.
 * @param u32SysViewCause  Ursache fuer SEGGER_SYSVIEW_OnTaskStopReady()
 */
void Scheduler_vBlockCurrentTask(uint32_t u32TimeoutTicks, uint32_t u32SysViewCause);

/**
 * @brief Nach Scheduler_vBlockCurrentTask() + Verlassen des kritischen
 *        Abschnitts aufrufen: wartet (spinnt), bis der Task von SysTick/
 *        Unblock wieder auf Ready/Running gesetzt und eingelastet wurde.
 */
void Scheduler_vWaitWhileBlocked(void);

/**
 * @brief Task aufwecken (Blocked -> Ready). ISR-sicher, wenn im kritischen
 *        Abschnitt aufgerufen. Tut nichts, wenn der Task nicht Blocked ist.
 */
void Scheduler_vUnblockTask(uint8_t u8TaskIdx);

/// Verbleibende Delay-/Timeout-Ticks eines Tasks (fuer Timeout-Erkennung)
uint32_t Scheduler_u32GetRemainingDelay(uint8_t u8TaskIdx);

#endif
