/**
 * @file    shell.h
 * @brief   Interaktive UART-Shell: Kommandoparser und Kalibrierroutine.
 * @author  TODO: Name eintragen
 *
 * Wird vom UartShell-Task aufgerufen (siehe tasks.c). Unterstuetzte
 * Kommandos: `help`, `status`, `cal <mm>`.
 *
 * Kalibrierung: `cal <mm>` schaltet auf Rohwerte um, mittelt
 * #SHELL_CAL_SAMPLES Messungen und setzt daraus den Offset
 * (Offset = Zielwert - Mittelwert) in g_userConfig.
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "app_messages.h"

/// Anzahl Messwerte, ueber die die Kalibrierung mittelt.
#define SHELL_CAL_SAMPLES   ( 8u )

/**
 * @brief Eine komplette Eingabezeile verarbeiten.
 * @param pcLine Nullterminierte Eingabezeile ohne Zeilenende
 *               (z.B. "cal 500"). Unbekannte Kommandos erzeugen einen
 *               Hinweis auf der UART.
 * @note Der Aufrufer darf g_uartMutex NICHT halten - die Funktion
 *       sperrt ihn fuer ihre Ausgaben selbst.
 */
void Shell_vHandleLine(const char *pcLine);

/**
 * @brief Verarbeitete Sensordaten einer laufenden Kalibrierung zufuehren.
 *
 * Wird vom UartShell-Task fuer jeden empfangenen Messwert aufgerufen,
 * bevor dieser ausgegeben wird.
 *
 * @param psData Zeiger auf den soeben empfangenen Messdatensatz.
 * @return 1, wenn der Wert von der Kalibrierung verbraucht wurde (dann
 *         NICHT als Distanz ausgeben), sonst 0.
 */
uint8_t Shell_u8FeedCalibration(const ProcessedData_t *psData);

#endif /* SHELL_H */
