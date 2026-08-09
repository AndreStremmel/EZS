/**
 ******************************************************************************
 * @file    uart_driver.h
 * @brief   UART abstraction: interrupt-driven RX ring buffer, blocking TX.
 * @author  __________
 ******************************************************************************
 *
 * Reception is interrupt-driven into a ring buffer. When a line ending
 * (\\r or \\n) arrives, the ISR additionally gives g_uartRxSemaphore, so the
 * shell task can wait for user input in a blocked state instead of polling.
 *
 * Transmission is blocking (polled) and therefore only allowed from task
 * context; the caller has to hold g_uartMutex so that output from different
 * tasks cannot interleave.
 *
 ******************************************************************************
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Enable the UART interrupt in the NVIC and start the first
 *        interrupt-driven byte reception.
 * @author __________
 *
 * @note Must be called from main() AFTER MX_USARTx_UART_Init(), because the
 *       UART handle is not initialised before that point.
 */
void UART_Init(void);

/**
 * @brief Send a null-terminated string, blocking until it has been written.
 * @param str String to send.
 * @author __________
 *
 * @warning Task context only, and only while the caller holds g_uartMutex.
 */
void UART_SendString(const char *str);

/**
 * @brief Send an unsigned number as a decimal string.
 * @param value Value to print (0..65535).
 * @author __________
 *
 * @warning Same conditions as UART_SendString().
 */
void UART_SendUInt(uint16_t value);

/**
 * @brief  Check whether a complete input line is waiting in the ring buffer.
 * @return true if the buffer holds a line terminated by \\r or \\n,
 *         false otherwise.
 * @author __________
 */
bool UART_LineAvailable(void);

/**
 * @brief Read one complete line out of the ring buffer.
 * @param buf Destination buffer; receives the line without \\r\\n, always
 *            null-terminated.
 * @param len Size of @p buf in bytes; longer lines are truncated.
 * @author __________
 */
void UART_ReadLine(char *buf, size_t len);

#endif /* UART_DRIVER_H */
