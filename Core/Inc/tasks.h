/*
 * tasks.h
 *  Endprojekt-Taskset (ersetzt die Task1/2/3-Version).
 *
 *  Index-Konvention (WICHTIG fuer Scheduler + TeSSLa-Auswertung):
 *    tasks[0] = SensorTask     (Prio 3)
 *    tasks[1] = ProcTask       (Prio 1)  \ Round-Robin-Paar
 *    tasks[2] = UartShellTask  (Prio 1)  /
 *    tasks[3] = IdleTask       (Prio 0)  - MUSS der letzte Eintrag sein!
 */

#ifndef TASKS_H
#define TASKS_H

#include "tcb.h"
#include "scheduler.h"

#define NUM_TASKS 4

extern TCB_sctTCB_t tasks[NUM_TASKS];

void SensorTask(void);
void ProcTask(void);
void UartShellTask(void);
void IdleTask(void);

/// Task-Array (Id, Prio, State) befuellen - VOR Stack_vInit aufrufen!
void Tasks_vInitTaskArray(void);

#endif
