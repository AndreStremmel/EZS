/**
 * @file    uart_driver.h
 * @brief   UART-Abstraktion: interruptgetriebener RX-Ringpuffer, blockierendes TX.
 * @author  TODO: Name eintragen
 *
 * Der Empfang laeuft interruptgetrieben in einen Ringpuffer; bei einem
 * Zeilenende (\\r oder \\n) gibt die ISR zusaetzlich g_uartRxSemaphore frei,
 * sodass der Shell-Task blockierend auf Eingaben warten kann.
 * Das Senden ist blockierend (Polling) und daher nur aus Task-Kontext
 * erlaubt - der Aufrufer muss g_uartMutex halten.
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief NVIC aktivieren und ersten Byte-Empfang per Interrupt starten.
 *
 * Muss in main() NACH MX_USARTx_UART_Init() aufgerufen werden, da der
 * UART-Handle bis dahin nicht initialisiert ist.
 */
void UART_Init(void);

/**
 * @brief Nullterminierten String blockierend senden.
 * @param str Zu sendender, nullterminierter String.
 * @warning Nur aus Task-Kontext aufrufen, und nur waehrend der Aufrufer
 *          g_uartMutex haelt (sonst vermischen sich Ausgaben).
 */
void UART_SendString(const char *str);

/**
 * @brief Vorzeichenlose Zahl als Dezimalstring senden.
 * @param value Auszugebender Wert (0..65535).
 * @warning Gleiche Bedingungen wie UART_SendString().
 */
void UART_SendUInt(uint16_t value);

/**
 * @brief Pruefen, ob eine komplette Eingabezeile bereitsteht.
 * @return true, wenn im Ringpuffer eine mit \\r oder \\n abgeschlossene
 *         Zeile liegt, sonst false.
 */
bool UART_LineAvailable(void);

/**
 * @brief Eine komplette Zeile aus dem Ringpuffer lesen.
 * @param buf Zielpuffer fuer die Zeile (ohne \\r\\n, immer nullterminiert).
 * @param len Groesse von @p buf in Bytes; laengere Zeilen werden gekuerzt.
 */
void UART_ReadLine(char *buf, size_t len);

#endif /* UART_DRIVER_H */
