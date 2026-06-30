/*
 * tcb.h
 *  Task control block
 *  Created on: Mar 31, 2025
 *      Author: David
 */

#ifndef DOS_INC_TCB_H_
#define DOS_INC_TCB_H_

#include <stdint.h>

/// Stack size of a single task
#define TCB_TASK_STACK_SIZE                 ( 128u )    ///< TODO: Must be adapted if a task requires a lot of stack!

/// Enum for task states
typedef enum
{
    // TODO: For the future
    TaskState_Created = 0U, ///< [FUTURE] Indicates that a task is created

    // General task states during normal operation
    TaskState_Ready,        ///< Indicates that a task is ready and could be selected by the scheduler.
    TaskState_Running,      ///< Indicates that a task is currently running and using CPU resources.
    TaskState_Blocked,      ///< Indicates that a task is blocked and still not ready to be selected by scheduler.

    // TODO: For the future...
    TaskState_Deleted,      ///< [FUTURE] Indicates that a task was deleted and will never run again.
    TaskState_MAX_STATE,    ///< Used for validity checks
} TCB_eTaskStates_t;

/// Structure for the task control block (TCB)
typedef struct
{
    uint32_t u32TaskSP;                             ///< To store the current stack pointer. As an alternative, we could use an index 'u8IdxSP' which points to the current cell on the stack. The address the corresponds to &au32TaskStack[u8IdxSP]
    uint8_t u8TaskId;             ///< Identification of a task
    uint8_t u8TaskPrio;           ///< priority of the task
    TCB_eTaskStates_t eTaskState; ///< the tasks state

    uint32_t au32TaskStack[TCB_TASK_STACK_SIZE];    ///< Tasks stack. We will push our registers R4-R11 on the stack because PendSV pushes the rest of the registers on the stack too.
} TCB_sctTCB_t;

#endif /* DOS_INC_TCB_H_ */
