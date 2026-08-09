/*
 * shell.c
 *  Interaktive UART-Shell mit Kalibrierroutine.
 *
 *  Kommandos:
 *    help          - Kommandouebersicht
 *    status        - aktuelle Kalibrierwerte anzeigen
 *    cal <mm>      - Kalibrierung starten: Objekt in bekannter Distanz
 *                    <mm> platzieren; es werden SHELL_CAL_SAMPLES rohe
 *                    Messwerte gemittelt und der Offset so gesetzt, dass
 *                    der Mittelwert auf <mm> abgebildet wird.
 *
 *  Waehrend der Kalibrierung setzt die Shell g_userConfig.active = 0 als
 *  "Roh-Modus"-Signal: der Proc-Task laesst Offset/Faktor dann unangewendet,
 *  damit auf unverfaelschten Werten kalibriert wird.
 */

#include "shell.h"
#include "app_resources.h"
#include "uart_driver.h"
#include "os_mutex.h"
#include "os_trace.h"
#include <string.h>

/* Kalibrier-Zustand (nur vom UartShell-Task benutzt) */
static uint8_t  s_u8CalRunning   = 0u;
static uint8_t  s_u8CalCollected = 0u;
static uint32_t s_u32CalSum      = 0u;
static uint16_t s_u16CalTarget   = 0u;

/// Dezimalstring -> uint16, gibt 1 bei Erfolg zurueck
static uint8_t prv_u8ParseUInt(const char *pc, uint16_t *pu16Out)
{
    uint32_t u32Val = 0u;
    uint8_t  u8Any  = 0u;

    while (*pc == ' ') { pc++; }
    while (*pc >= '0' && *pc <= '9')
    {
        u32Val = u32Val * 10u + (uint32_t)(*pc - '0');
        if (u32Val > 65535u) { return 0u; }
        pc++;
        u8Any = 1u;
    }
    *pu16Out = (uint16_t)u32Val;
    return (u8Any != 0u && (*pc == '\0' || *pc == ' ')) ? 1u : 0u;
}

static void prv_vPrintStatus(void)
{
    User_Data_t sCfg;

    OS_Mutex_LockBlocking(&g_configMutex);
    sCfg = g_userConfig;
    OS_Mutex_Unlock(&g_configMutex);

    OS_Mutex_LockBlocking(&g_uartMutex);
    UART_SendString("Kalibrierung: Offset=");
    if (sCfg.calibrationOffsetMm < 0)
    {
        UART_SendString("-");
        UART_SendUInt((uint16_t)(-sCfg.calibrationOffsetMm));
    }
    else
    {
        UART_SendUInt((uint16_t)sCfg.calibrationOffsetMm);
    }
    UART_SendString(" mm, Faktor=");
    UART_SendUInt(sCfg.calibrationFactorPermille);
    UART_SendString("/1000\r\n");
    OS_Mutex_Unlock(&g_uartMutex);
}

void Shell_vHandleLine(const char *pcLine)
{
    if (pcLine[0] == '\0')
    {
        return;   /* Leerzeile ignorieren */
    }

    if (strcmp(pcLine, "help") == 0)
    {
        OS_Mutex_LockBlocking(&g_uartMutex);
        UART_SendString("Kommandos:\r\n"
                        "  help      - diese Uebersicht\r\n"
                        "  status    - Kalibrierwerte anzeigen\r\n"
                        "  cal <mm>  - Kalibrierung mit Referenzdistanz starten\r\n");
        OS_Mutex_Unlock(&g_uartMutex);
    }
    else if (strcmp(pcLine, "status") == 0)
    {
        prv_vPrintStatus();
    }
    else if (strncmp(pcLine, "cal ", 4u) == 0)
    {
        uint16_t u16Target;
        if (prv_u8ParseUInt(&pcLine[4], &u16Target) == 0u || u16Target == 0u)
        {
            OS_Mutex_LockBlocking(&g_uartMutex);
            UART_SendString("Fehler: cal <mm> erwartet eine Distanz > 0\r\n");
            OS_Mutex_Unlock(&g_uartMutex);
            return;
        }

        /* Roh-Modus aktivieren, damit auf unkalibrierten Werten gemittelt wird */
        OS_Mutex_LockBlocking(&g_configMutex);
        g_userConfig.active = 0u;
        OS_Mutex_Unlock(&g_configMutex);

        s_u16CalTarget   = u16Target;
        s_u32CalSum      = 0u;
        s_u8CalCollected = 0u;
        s_u8CalRunning   = 1u;

        OS_TRACE_APP1(OS_TRACE_EVT_CAL_START, u16Target);

        OS_Mutex_LockBlocking(&g_uartMutex);
        UART_SendString("Kalibrierung laeuft: Objekt bei ");
        UART_SendUInt(u16Target);
        UART_SendString(" mm ruhig halten...\r\n");
        OS_Mutex_Unlock(&g_uartMutex);
    }
    else
    {
        OS_Mutex_LockBlocking(&g_uartMutex);
        UART_SendString("Unbekanntes Kommando - 'help' fuer Uebersicht\r\n");
        OS_Mutex_Unlock(&g_uartMutex);
    }
}

uint8_t Shell_u8FeedCalibration(const ProcessedData_t *psData)
{
    if (s_u8CalRunning == 0u)
    {
        return 0u;
    }

    if (psData->error != 0u)
    {
        return 1u;   /* Fehlmessungen verwerfen, aber Wert "verbrauchen" */
    }

    s_u32CalSum += psData->distance_mm;
    s_u8CalCollected++;

    if (s_u8CalCollected >= SHELL_CAL_SAMPLES)
    {
        uint32_t u32Avg    = s_u32CalSum / SHELL_CAL_SAMPLES;
        int16_t  i16Offset = (int16_t)((int32_t)s_u16CalTarget - (int32_t)u32Avg);

        OS_Mutex_LockBlocking(&g_configMutex);
        g_userConfig.calibrationOffsetMm       = i16Offset;
        g_userConfig.calibrationFactorPermille = 1000u;
        g_userConfig.active                    = 1u;   /* Kalibrierung wieder anwenden */
        OS_Mutex_Unlock(&g_configMutex);

        s_u8CalRunning = 0u;
        OS_TRACE_APP1(OS_TRACE_EVT_CAL_DONE, (uint32_t)i16Offset);

        OS_Mutex_LockBlocking(&g_uartMutex);
        UART_SendString("Kalibrierung fertig: Mittelwert=");
        UART_SendUInt((uint16_t)u32Avg);
        UART_SendString(" mm, Offset=");
        if (i16Offset < 0)
        {
            UART_SendString("-");
            UART_SendUInt((uint16_t)(-i16Offset));
        }
        else
        {
            UART_SendUInt((uint16_t)i16Offset);
        }
        UART_SendString(" mm\r\n");
        OS_Mutex_Unlock(&g_uartMutex);
    }

    return 1u;
}
