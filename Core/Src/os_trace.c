/**
 ******************************************************************************
 * @file    os_trace.c
 * @brief   SystemView instrumentation for the TeSSLa verification -
 *          implementation.
 * @author  Berkay
 ******************************************************************************
 *
 * Registers a custom SystemView module so that the kernel events show up under
 * readable names instead of raw IDs, and exports the task list so the analysis
 * can map TCB addresses to task names.
 *
 * @see os_trace.h for the event IDs and the API description.
 *
 ******************************************************************************
 */

#include "os_trace.h"
#include "tasks.h"
#include "SEGGER_SYSVIEW.h"

/* The description string defines the event names and parameter formats.
 * Its order MUST match the enum in os_trace.h exactly! SystemView then shows
 * the events by name, and the text export contains the name plus "key=value"
 * pairs, which is what the Python converter parses.
 *
 * --------------------------------------------------------------------------
 * SLIM VARIANT for a targeted test run (e.g. mutex.tessla only):
 * names ONLY the mutex events (0-5); everything from index 6 on stays
 * unnamed in SystemView ("Function #NNN"). That is deliberate - this run
 * only needs readable MtxLock/MtxUnlock names, and a shorter string puts
 * considerably less pressure on the RTT buffer.
 *
 * IMPORTANT: the event NUMBERS (0-29) in os_trace.h are unchanged - only the
 * description text is shortened. An event such as SemTakeOk (enum value 7) is
 * therefore still sent correctly with the value 7; it merely appears in
 * SystemView as "Function #<offset+7>" instead of "SemTakeOk" because no
 * description exists for it. Harmless for a pure mutex check.
 *
 * To switch back to the full description (all 30 events, needed for
 * semaphore.tessla / queue.tessla / delay.tessla / sensor.tessla): enable the
 * FULL VARIANT block below and disable this one.
 * -------------------------------------------------------------------------- */
static SEGGER_SYSVIEW_MODULE s_sModule =
{
    "M=DOSRTOS,"
    "0 MtxLockTry Mtx=%u Task=%u,"
    "1 MtxLockOk Mtx=%u Task=%u,"
    "2 MtxLockBlock Mtx=%u Task=%u Ticks=%u,"
    "3 MtxLockTimeout Mtx=%u Task=%u,"
    "4 MtxUnlockOk Mtx=%u Task=%u,"
    "5 MtxUnlockDenied Mtx=%u Task=%u",
    OS_TRACE_EVT__COUNT,   // NumEvents
    0,                     // EventOffset (assigned by SystemView)
    NULL,                  // pfSendModuleDesc
    NULL                   // pNext
};

#if 0
/* --------------------------------------------------------------------------
 * FULL VARIANT (all 30 events named) - requires a larger RTT buffer
 * (BUFFER_SIZE_UP) and SEGGER_SYSVIEW_MAX_STRING_LEN >= ~900, otherwise the
 * string is truncated or causes an overflow.
 * -------------------------------------------------------------------------- */
static SEGGER_SYSVIEW_MODULE s_sModule =
{
    "M=DOSRTOS,"
    "0 MtxLockTry Mtx=%u Task=%u,"
    "1 MtxLockOk Mtx=%u Task=%u,"
    "2 MtxLockBlock Mtx=%u Task=%u Ticks=%u,"
    "3 MtxLockTimeout Mtx=%u Task=%u,"
    "4 MtxUnlockOk Mtx=%u Task=%u,"
    "5 MtxUnlockDenied Mtx=%u Task=%u,"
    "6 SemTakeTry Sem=%u Task=%u,"
    "7 SemTakeOk Sem=%u Task=%u Cnt=%u,"
    "8 SemTakeBlock Sem=%u Task=%u Ticks=%u,"
    "9 SemTakeTimeout Sem=%u Task=%u,"
    "10 SemGiveOk Sem=%u Task=%u Cnt=%u,"
    "11 SemGiveIgnored Sem=%u Task=%u,"
    "12 QSendTry Q=%u Task=%u,"
    "13 QSendOk Q=%u Chk=%u Cnt=%u,"
    "14 QSendFull Q=%u Task=%u,"
    "15 QSendBlock Q=%u Task=%u Ticks=%u,"
    "16 QSendTimeout Q=%u Task=%u,"
    "17 QRecvTry Q=%u Task=%u,"
    "18 QRecvOk Q=%u Chk=%u Cnt=%u,"
    "19 QRecvEmpty Q=%u Task=%u,"
    "20 QRecvBlock Q=%u Task=%u Ticks=%u,"
    "21 QRecvTimeout Q=%u Task=%u,"
    "22 DelayStart Task=%u Ticks=%u,"
    "23 BusyDelayStart Task=%u Ticks=%u,"
    "24 BusyDelayEnd Task=%u,"
    "25 TaskMap Idx=%u Addr=%x,"
    "26 UartTxDist Dist=%u,"
    "27 SensorErr Code=%u,"
    "28 CalStart Target=%u,"
    "29 CalDone Offset=%d",
    OS_TRACE_EVT__COUNT,   // NumEvents
    0,                     // EventOffset (assigned by SystemView)
    NULL,                  // pfSendModuleDesc
    NULL                   // pNext
};
#endif


/** @brief Set once the module has been registered; the record functions stay
 *         silent before that so early events cannot corrupt the stream. */
static uint8_t s_u8Registered = 0u;

/**
 * @brief Send the complete task list to SystemView.
 * @author Berkay
 *
 * Called at boot AND registered as the pfSendTaskList callback in the
 * SEGGER_SYSVIEW configuration - SystemView actively requests the list when a
 * host connects or a Berkay starts. This is what makes the task names
 * appear even when Berkay begins AFTER boot (otherwise the display shows
 * e.g. "Task 0x11EC" instead of "Idle").
 */
void OS_Trace_vSendTaskList(void)
{
    /* IMPORTANT: must match the task set in main.c / Tasks_vInitTaskArray()
     * (tasks.c) - otherwise SystemView would show e.g. "Sensor" while
     * TestHighTask is actually running. Both branches deliberately hang off
     * the same switch as main.c and tasks.c, so they cannot drift apart. */
#if (OS_RUN_INTEGRATION_TESTS != 0)
    static const char *apcNames[NUM_TASKS] = { "TestHigh", "TestMain", "TestPeer", "Idle" };
#else
    static const char *apcNames[NUM_TASKS] = { "Sensor", "Proc", "UartShell", "Idle" };
#endif
    for (uint8_t i = 0u; i < NUM_TASKS; i++)
    {
        SEGGER_SYSVIEW_TASKINFO sInfo = {0};

        /* Create the task object first (documented order: create, then
         * info) - otherwise SystemView may ignore the name. */
        SEGGER_SYSVIEW_OnTaskCreate((uint32_t)&tasks[i]);

        sInfo.TaskID    = (uint32_t)&tasks[i];
        sInfo.sName     = apcNames[i];
        sInfo.Prio      = tasks[i].u8TaskPrio;
        sInfo.StackBase = (uint32_t)&tasks[i].au32TaskStack[0];
        sInfo.StackSize = sizeof(tasks[i].au32TaskStack);
        SEGGER_SYSVIEW_SendTaskInfo(&sInfo);

        /* Mapping of task index <-> TCB address for the TeSSLa converter:
         * SystemView task events carry the TCB address while our own module
         * events carry the index - the converter needs both to correlate
         * them. */
        OS_Trace_Record2(OS_TRACE_EVT_TASK_MAP, i, (uint32_t)&tasks[i]);
    }
}

/**
 * @brief Register the custom module with SystemView and send the task info.
 * @author Berkay
 *
 * @note Call AFTER SEGGER_SYSVIEW_Conf() and BEFORE Scheduler_vInit().
 */
void OS_Trace_Init(void)
{
#if (OS_TRACE_ENABLED == 0)
    /* Instrumentation completely disabled (os_trace_config.h): neither the
     * module nor the task info is reported. */
    return;
#else
    SEGGER_SYSVIEW_RegisterModule(&s_sModule);
    s_u8Registered = 1u;

    OS_Trace_vSendTaskList();
#endif
}

/**
 * @brief Emit a raw module event with one parameter.
 * @param evt Event ID.
 * @param p0  First parameter.
 * @author Berkay
 */
void OS_Trace_Record1(OS_TraceEvent_t evt, uint32_t p0)
{
    if (s_u8Registered)
    {
        SEGGER_SYSVIEW_RecordU32(s_sModule.EventOffset + (unsigned)evt, p0);
    }
}

/**
 * @brief Emit a raw module event with two parameters.
 * @param evt Event ID.
 * @param p0  First parameter.
 * @param p1  Second parameter.
 * @author Berkay
 */
void OS_Trace_Record2(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1)
{
    if (s_u8Registered)
    {
        SEGGER_SYSVIEW_RecordU32x2(s_sModule.EventOffset + (unsigned)evt, p0, p1);
    }
}

/**
 * @brief Emit a raw module event with three parameters.
 * @param evt Event ID.
 * @param p0  First parameter.
 * @param p1  Second parameter.
 * @param p2  Third parameter.
 * @author Berkay
 */
void OS_Trace_Record3(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1, uint32_t p2)
{
    if (s_u8Registered)
    {
        SEGGER_SYSVIEW_RecordU32x3(s_sModule.EventOffset + (unsigned)evt, p0, p1, p2);
    }
}

/**
 * @brief  16-bit checksum over a memory range.
 * @param  pData Start of the range.
 * @param  len   Length of the range in bytes.
 * @return Checksum value.
 * @author Berkay
 *
 * Seeded with the length and rotating between bytes, so that reordered or
 * truncated payloads produce a different result. Logged with every queue send
 * and receive to let the specification verify FIFO order and payload
 * integrity.
 */
uint16_t OS_Trace_u16Checksum(const void *pData, size_t len)
{
    const uint8_t *p = (const uint8_t *)pData;
    uint16_t u16Sum = (uint16_t)len;

    for (size_t i = 0u; i < len; i++)
    {
        u16Sum = (uint16_t)((u16Sum << 1) | (u16Sum >> 15));  // rotate left by 1
        u16Sum = (uint16_t)(u16Sum + p[i]);
    }
    return u16Sum;
}
