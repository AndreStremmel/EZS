/**
 ******************************************************************************
 * @file    uart_driver.h
 * @brief   UART-Abstraktion: Senden, Empfangen, Zeilenbasierte Shell
 *
 * Board-spezifisch (UART-Handle, IRQn) wird über board_config.h gesteuert.
 * Alle Funktionen sind für den kooperativen Scheduler ausgelegt:
 *   TX: blockierendes Polling (kurz, unkritisch bei 115200 Baud)
 *   RX: Interrupt-getriebener Ringpuffer – Shell_Task muss nicht warten
 ******************************************************************************
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  UART initialisieren: NVIC aktivieren, Empfangs-Interrupt starten.
 *         Aufruf in main.c nach MX_USARTx_UART_Init(), vor Scheduler_vInit().
 */
void UART_Init(void);

/**
 * @brief  Nullterminierte Zeichenkette senden (blockierendes Polling).
 * @param  str  Zeiger auf den String. NULL wird ignoriert.
 */
void UART_SendString(const char *str);

/**
 * @brief  Vorzeichenlose 16-bit-Zahl als ASCII-Dezimalzahl senden.
 * @param  value  Wert 0–65535
 */
void UART_SendUInt(uint16_t value);

/**
 * @brief  Prüft ob eine vollständige Zeile (\\r oder \\n) im Empfangspuffer liegt.
 * @return true  wenn mindestens ein Zeilenende-Zeichen im Puffer vorhanden ist
 */
bool UART_LineAvailable(void);

/**
 * @brief  Liest eine Zeile aus dem Empfangspuffer (ohne \\r / \\n).
 *         Windows-Zeilenenden (\\r\\n) werden korrekt behandelt.
 * @param  buf  Zielpuffer
 * @param  len  Größe des Zielpuffers inkl. Nullterminierung
 */
void UART_ReadLine(char *buf, size_t len);

#endif /* UART_DRIVER_H */
