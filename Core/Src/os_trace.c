/*
 * os_trace.c
 *  SystemView-Instrumentierung fuer die TeSSLa-Verifikation.
 */

#include "os_trace.h"
#include "tasks.h"
#include "SEGGER_SYSVIEW.h"

/* Die Beschreibung definiert die Event-Namen und Parameter-Formate.
 * Reihenfolge MUSS exakt zum Enum in os_trace.h passen!
 * SystemView zeigt die Events dann benannt an; der Textexport enthaelt
 * Name + "Key=Wert"-Paare, die der Python-Konverter parst. */
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
    0,                     // EventOffset (wird von SystemView gesetzt)
    NULL,                  // pfSendModuleDesc
    NULL                   // pNext
};

static uint8_t s_u8Registered = 0u;

void OS_Trace_Init(void)
{
    SEGGER_SYSVIEW_RegisterModule(&s_sModule);
    s_u8Registered = 1u;

    /* Task-Namen fuer die SystemView-Anzeige registrieren */
    static const char *apcNames[NUM_TASKS] = { "Sensor", "Proc", "UartShell", "Idle" };
    for (uint8_t i = 0u; i < NUM_TASKS; i++)
    {
        SEGGER_SYSVIEW_TASKINFO sInfo;
        sInfo.TaskID    = (uint32_t)&tasks[i];
        sInfo.sName     = apcNames[i];
        sInfo.Prio      = tasks[i].u8TaskPrio;
        sInfo.StackBase = (uint32_t)&tasks[i].au32TaskStack[0];
        sInfo.StackSize = sizeof(tasks[i].au32TaskStack);
        SEGGER_SYSVIEW_SendTaskInfo(&sInfo);

        /* Mapping TaskIdx <-> TCB-Adresse fuer den TeSSLa-Konverter:
         * SystemView-Task-Events tragen die TCB-Adresse, unsere
         * Modul-Events den Index - der Konverter braucht beides. */
        OS_Trace_Record2(OS_TRACE_EVT_TASK_MAP, i, (uint32_t)&tasks[i]);
    }
}

void OS_Trace_Record1(OS_TraceEvent_t evt, uint32_t p0)
{
    if (s_u8Registered)
    {
        SEGGER_SYSVIEW_RecordU32(s_sModule.EventOffset + (unsigned)evt, p0);
    }
}

void OS_Trace_Record2(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1)
{
    if (s_u8Registered)
    {
        SEGGER_SYSVIEW_RecordU32x2(s_sModule.EventOffset + (unsigned)evt, p0, p1);
    }
}

void OS_Trace_Record3(OS_TraceEvent_t evt, uint32_t p0, uint32_t p1, uint32_t p2)
{
    if (s_u8Registered)
    {
        SEGGER_SYSVIEW_RecordU32x3(s_sModule.EventOffset + (unsigned)evt, p0, p1, p2);
    }
}

uint16_t OS_Trace_u16Checksum(const void *pData, size_t len)
{
    const uint8_t *p = (const uint8_t *)pData;
    uint16_t u16Sum = (uint16_t)len;

    for (size_t i = 0u; i < len; i++)
    {
        u16Sum = (uint16_t)((u16Sum << 1) | (u16Sum >> 15));  // rotieren
        u16Sum = (uint16_t)(u16Sum + p[i]);
    }
    return u16Sum;
}
