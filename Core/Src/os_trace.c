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
/* --------------------------------------------------------------------------
 * SCHLANKE VARIANTE fuer einen gezielten Testlauf (z.B. nur mutex.tessla):
 * Beschreibt NUR die Mutex-Events (0-5) mit Klarnamen; alles ab Index 6
 * bleibt fuer SystemView unbenannt ("Function #NNN"). Das ist bewusst so -
 * fuer diesen Lauf braucht ihr nur MtxLock/MtxUnlock-Klarnamen, und ein
 * kuerzerer String belastet den RTT-Puffer deutlich weniger.
 *
 * WICHTIG: Die Event-NUMMERN (0-29) in os_trace.h bleiben unveraendert -
 * nur der Beschreibungstext wird gekuerzt. Ein Event wie SemTakeOk (Enum-
 * Wert 7) wird also weiterhin korrekt mit dem Wert 7 gesendet, taucht in
 * SystemView aber als "Function #<Offset+7>" statt "SemTakeOk" auf, weil
 * die Beschreibung dafuer fehlt. Fuer die reine Mutex-Pruefung unschaedlich.
 *
 * Zum Zurueckwechseln auf die volle Beschreibung (alle 30 Events, fuer
 * semaphore.tessla/queue.tessla/delay.tessla/sensor.tessla): den Block
 * weiter unten (VOLLSTAENDIGE VARIANTE) einkommentieren und diesen hier
 * auskommentieren.
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
    0,                     // EventOffset (wird von SystemView gesetzt)
    NULL,                  // pfSendModuleDesc
    NULL                   // pNext
};

#if 0
/* --------------------------------------------------------------------------
 * VOLLSTAENDIGE VARIANTE (alle 30 Events benannt) - braucht einen
 * groesseren RTT-Puffer (BUFFER_SIZE_UP) und SEGGER_SYSVIEW_MAX_STRING_LEN
 * >= ~900, sonst wird sie abgeschnitten oder erzeugt einen Overflow.
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
    0,                     // EventOffset (wird von SystemView gesetzt)
    NULL,                  // pfSendModuleDesc
    NULL                   // pNext
};
#endif


static uint8_t s_u8Registered = 0u;

/* Sendet die komplette Taskliste an SystemView. Wird beim Boot aufgerufen
 * UND als pfSendTaskList-Callback aus der SEGGER_SYSVIEW-Konfiguration -
 * SystemView fragt die Liste beim Verbinden/Record-Start aktiv an. Nur so
 * erscheinen die Tasknamen auch, wenn die Aufzeichnung erst NACH dem Boot
 * gestartet wird (sonst: "Task 0x11EC" statt "Idle"). */
void OS_Trace_vSendTaskList(void)
{
    /* WICHTIG: Muss zum Taskset in main.c / Tasks_vInitTaskArray()
     * (tasks.c) passen - sonst zeigt SystemView z.B. "Sensor" an,
     * waehrend in Wirklichkeit TestHighTask laeuft. Beide Zweige
     * haengen bewusst am selben Schalter wie main.c/tasks.c, damit sie
     * nie auseinanderlaufen koennen. */
#if (OS_RUN_INTEGRATION_TESTS != 0)
    static const char *apcNames[NUM_TASKS] = { "TestHigh", "TestMain", "TestPeer", "Idle" };
#else
    static const char *apcNames[NUM_TASKS] = { "Sensor", "Proc", "UartShell", "Idle" };
#endif
    for (uint8_t i = 0u; i < NUM_TASKS; i++)
    {
        SEGGER_SYSVIEW_TASKINFO sInfo = {0};

        /* Task-Objekt anlegen (dokumentierte Reihenfolge: erst Create,
         * dann Info) - sonst ignoriert SystemView den Namen u.U. */
        SEGGER_SYSVIEW_OnTaskCreate((uint32_t)&tasks[i]);

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

void OS_Trace_Init(void)
{
#if (OS_TRACE_ENABLED == 0)
    /* Instrumentierung komplett abgeschaltet (os_trace_config.h):
     * weder Modul noch Task-Infos werden gemeldet. */
    return;
#else
    SEGGER_SYSVIEW_RegisterModule(&s_sModule);
    s_u8Registered = 1u;

    OS_Trace_vSendTaskList();
#endif
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
