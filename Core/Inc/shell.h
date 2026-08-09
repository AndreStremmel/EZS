/**
 ******************************************************************************
 * @file    shell.h
 * @brief   Interactive UART shell: command parser and calibration routine.
 * @author  __________
 ******************************************************************************
 *
 * Driven by UartShellTask (see tasks.c). Supported commands:
 *   help          - list the available commands
 *   status        - show the current calibration values
 *   cal <mm>      - start a calibration run
 *
 * Calibration: `cal <mm>` switches the output to raw values, averages
 * #SHELL_CAL_SAMPLES measurements and derives the offset from the result
 * (offset = target value - measured average), storing it in g_userConfig.
 *
 ******************************************************************************
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "app_messages.h"

/** @brief Number of measurements the calibration averages over. */
#define SHELL_CAL_SAMPLES   ( 8u )

/**
 * @brief Parse and execute one complete input line.
 * @param pcLine Null-terminated input line without the line ending
 *               (e.g. "cal 500"). Unknown commands produce a hint on the UART.
 * @author __________
 *
 * @note The caller must NOT hold g_uartMutex - this function acquires it
 *       itself for its own output.
 */
void Shell_vHandleLine(const char *pcLine);

/**
 * @brief Feed a processed measurement into a running calibration.
 * @param  psData Measurement record that was just received.
 * @return 1 if the value was consumed by the calibration (in which case it
 *         must NOT be printed as a distance), 0 otherwise.
 * @author __________
 *
 * Called by UartShellTask for every received measurement before that
 * measurement is printed.
 */
uint8_t Shell_u8FeedCalibration(const ProcessedData_t *psData);

#endif /* SHELL_H */
