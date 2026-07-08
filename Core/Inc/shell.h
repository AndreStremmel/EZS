/*
 * shell.h
 *  Interaktive UART-Shell: Kommandoparser + Kalibrierroutine.
 *  Wird vom UartShell-Task aufgerufen (siehe tasks.c).
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "app_messages.h"

/// Anzahl Messwerte, ueber die die Kalibrierung mittelt
#define SHELL_CAL_SAMPLES   ( 8u )

/// Eine komplette Eingabezeile verarbeiten (help / cal <mm> / status)
void Shell_vHandleLine(const char *pcLine);

/**
 * @brief Verarbeitete Sensordaten waehrend einer laufenden Kalibrierung
 *        einsammeln. Gibt 1 zurueck, wenn der Wert von der Kalibrierung
 *        "verbraucht" wurde (dann nicht als Distanz ausgeben).
 */
uint8_t Shell_u8FeedCalibration(const ProcessedData_t *psData);

#endif /* SHELL_H */
