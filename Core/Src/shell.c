/**
 ******************************************************************************
 * @file    shell.c
 * @brief   Interactive UART shell with calibration routine - implementation.
 * @author  __________
 ******************************************************************************
 *
 * Commands:
 *   help          - list the available commands
 *   status        - show the current calibration values
 *   cal <mm>      - start a calibration: place an object at the known distance
 *                   <mm>; SHELL_CAL_SAMPLES raw measurements are averaged and
 *                   the offset is set so that the average maps onto <mm>.
 *
 * During calibration the shell sets g_userConfig.active = 0 as a "raw mode"
 * signal: ProcTask then leaves offset and factor unapplied, so that the
 * averaging happens on undistorted values.
 *
 * The calibration is driven from the UartShell task rather than run as a loop
 * of its own: Shell_u8FeedCalibration() consumes one measurement per call, so
 * the shell stays responsive while a calibration is in progress.
 *
 * @see shell.h for the API description.
 *
 ******************************************************************************
 */

#include "shell.h"
#include "app_resources.h"
#include "uart_driver.h"
#include "os_mutex.h"
#include "os_trace.h"
#include <string.h>

/* Calibration state - only ever touched by the UartShell task, so it needs no
 * locking of its own */
static uint8_t  s_u8CalRunning   = 0u;   ///< 1 while a calibration is in progress
static uint8_t  s_u8CalCollected = 0u;   ///< Samples collected so far
static uint32_t s_u32CalSum      = 0u;   ///< Running sum of the collected samples
static uint16_t s_u16CalTarget   = 0u;   ///< Reference distance given by the user

/**
 * @brief  Parse a decimal string into a uint16.
 * @param  pc      String to parse; leading spaces are skipped.
 * @param  pu16Out Receives the parsed value.
 * @return 1 on success, 0 on overflow, on a missing digit or on trailing junk.
 * @author __________
 *
 * Hand-written instead of using strtoul to keep the C library dependency (and
 * its stack footprint) out of the task.
 */
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

/**
 * @brief Print the current calibration values over UART.
 * @author __________
 *
 * Copies the configuration out under g_configMutex first, then prints without
 * holding it - the two mutexes are never held at the same time, which rules
 * out a lock-order deadlock.
 */
static void prv_vPrintStatus(void)
{
    User_Data_t sCfg;

    OS_Mutex_LockBlocking(&g_configMutex);
    sCfg = g_userConfig;
    OS_Mutex_Unlock(&g_configMutex);

    OS_Mutex_LockBlocking(&g_uartMutex);
    UART_SendString("Kalibrierung: Offset=");
    /* UART_SendUInt has no signed variant, so print the sign separately */
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

/**
 * @brief Parse and execute one complete input line.
 * @param pcLine Null-terminated input line without the line ending.
 * @author __________
 *
 * @note The caller must NOT hold g_uartMutex - this function acquires it
 *       itself for its output.
 */
void Shell_vHandleLine(const char *pcLine)
{
    if (pcLine[0] == '\0')
    {
        return;   /* Ignore empty lines */
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

        /* Switch to raw mode so the averaging runs on uncalibrated values */
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

/**
 * @brief  Feed a processed measurement into a running calibration.
 * @param  psData Measurement record that was just received.
 * @return 1 if the value was consumed by the calibration (and must therefore
 *         NOT be printed as a distance), 0 otherwise.
 * @author __________
 *
 * Once SHELL_CAL_SAMPLES valid samples have been collected, the offset is
 * computed as (target - average), the factor is reset to neutral and the
 * calibration is re-enabled.
 */
uint8_t Shell_u8FeedCalibration(const ProcessedData_t *psData)
{
    if (s_u8CalRunning == 0u)
    {
        return 0u;
    }

    if (psData->error != 0u)
    {
        return 1u;   /* Discard failed measurements, but still "consume" them */
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
        g_userConfig.active                    = 1u;   /* Apply calibration again */
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
