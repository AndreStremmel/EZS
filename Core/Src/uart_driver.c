/**
 ******************************************************************************
 * @file    uart_driver.c
 * @brief   UART abstraction - implementation.
 * @author  __________
 ******************************************************************************
 *
 * RX architecture: interrupt-driven ring buffer
 * ---------------------------------------------
 *  UART RX interrupt
 *       |
 *       v  HAL_UART_RxCpltCallback()
 *  rx_ring[ rx_head ]  <-  received byte
 *  rx_head = (rx_head + 1) % RX_BUFFER_SIZE
 *       |
 *       v  UartShell task (preemptive DOS-RTOS scheduler)
 *  UART_LineAvailable() -> scans the buffer for \\r or \\n
 *  UART_ReadLine()      -> reads up to the line ending, returns it without \\r\\n
 *
 * TX architecture: blocking polling
 * ---------------------------------
 *  UART_SendString / SendUInt -> HAL_UART_Transmit (polled, HAL_MAX_DELAY)
 *  Short strings (< 40 characters) are sent in under 3 ms at 115200 baud.
 *  TX happens under g_uartMutex; the sending task may be preempted while
 *  transmitting - the mutex is what keeps the output from interleaving.
 *
 * Which UART handle and IRQ are used is decided in board_config.h via
 * APP_UART_HANDLE and APP_UART_IRQn.
 *
 ******************************************************************************
 */

#include "uart_driver.h"
#include "board_config.h"
#include "app_resources.h"
#include "os_semaphore.h"
#include <string.h>

/* ==========================================================================
 * Receive ring buffer
 * ========================================================================== */

#define RX_BUFFER_SIZE  128U    /* Must be a power of two; max 256 with a uint8_t index */

static uint8_t           rx_ring[RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0U;   /* Write position (IRQ)           */
static volatile uint16_t rx_tail = 0U;   /* Read position  (task)          */
static uint8_t           rx_byte;        /* HAL receive buffer (1 byte)    */

/* ==========================================================================
 * Initialisation
 * ========================================================================== */

/**
 * @brief Enable the UART interrupt in the NVIC and start the first
 *        interrupt-driven byte reception.
 * @author __________
 *
 * Must be called in main.c after MX_USARTx_UART_Init():
 * @code
 *   MX_USART1_UART_Init();   // CubeMX init
 *   ...
 *   UART_Init();             // start the interrupt
 *   Scheduler_vInit();
 * @endcode
 */
void UART_Init(void)
{
    /* Priority 6: lower than SysTick (0) and the ECHO EXTI (5), higher than
     * PendSV (0xFF) - no UART characters are lost, but the interrupt does not
     * disturb the sensor timing either. */
    HAL_NVIC_SetPriority(APP_UART_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(APP_UART_IRQn);

    /* Start the first reception - the HAL re-arms itself in RxCpltCallback */
    HAL_UART_Receive_IT(&APP_UART_HANDLE, &rx_byte, 1U);
}

/* ==========================================================================
 * RX - interrupt callback
 * ========================================================================== */

/**
 * @brief Called by the HAL after every received byte: stores it in the ring
 *        buffer and re-arms the next reception.
 * @param huart UART handle that raised the interrupt.
 * @author __________
 *
 * @note This overrides the weak stub in stm32l4xx_hal_uart.c. It is invoked
 *       from USART1_IRQHandler via HAL_UART_IRQHandler().
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == APP_UART_HANDLE.Instance)
    {
        uint16_t next_head = (rx_head + 1U) % RX_BUFFER_SIZE;

        if (next_head != rx_tail)   /* Buffer not full - accept the byte */
        {
            rx_ring[rx_head] = rx_byte;
            rx_head          = next_head;

            /* Line ending -> wake the shell task instead of having it poll.
             * Give is ISR-safe, and the binary semaphore swallows the extra
             * give of a \r\n pair automatically. */
            if (rx_byte == (uint8_t)'\r' || rx_byte == (uint8_t)'\n')
            {
                OS_Semaphore_Give(&g_uartRxSemaphore);
            }
        }
        /* On a full buffer: drop the byte, but do not stop receiving */

        HAL_UART_Receive_IT(&APP_UART_HANDLE, &rx_byte, 1U);
    }
}

/* ==========================================================================
 * TX - transmitting
 * ========================================================================== */

/**
 * @brief Send a null-terminated string, blocking until it has been written.
 * @param str String to send; NULL and empty strings are ignored.
 * @author __________
 *
 * @warning Task context only, and only while the caller holds g_uartMutex.
 */
void UART_SendString(const char *str)
{
    if (str == NULL) { return; }

    size_t len = strlen(str);
    if (len == 0U) { return; }

    /* Cast: the HAL expects uint8_t*, the string is const char* - same bytes */
    HAL_UART_Transmit(&APP_UART_HANDLE,
                      (const uint8_t *)str,
                      (uint16_t)len,
                      HAL_MAX_DELAY);
}

/**
 * @brief Send an unsigned number as a decimal string.
 * @param value Value to print (0..65535).
 * @author __________
 *
 * Implemented by hand rather than via printf to avoid pulling the newlib
 * formatting machinery (and its stack usage) into the task stacks.
 *
 * @warning Same conditions as UART_SendString().
 */
void UART_SendUInt(uint16_t value)
{
    char    buf[6U];   /* up to 5 digits for 65535, plus '\0' */
    uint8_t i = 0U;
    uint8_t j = 0U;

    if (value == 0U)
    {
        UART_SendString("0");
        return;
    }

    /* Build the digits in reverse order */
    while (value > 0U)
    {
        buf[i++] = (char)('0' + (value % 10U));
        value    /= 10U;
    }

    /* Reverse in place: "321" -> "123" */
    for (j = 0U; j < i / 2U; j++)
    {
        char tmp        = buf[j];
        buf[j]          = buf[i - 1U - j];
        buf[i - 1U - j] = tmp;
    }
    buf[i] = '\0';

    UART_SendString(buf);
}

/* ==========================================================================
 * RX - line-based reading
 * ========================================================================== */

/**
 * @brief  Check whether a complete input line is waiting in the ring buffer.
 * @return true if the buffer holds a line terminated by \\r or \\n.
 * @author __________
 *
 * Scans without consuming, so the caller can decide whether to read the line.
 */
bool UART_LineAvailable(void)
{
    uint16_t i = rx_tail;

    while (i != rx_head)
    {
        if (rx_ring[i] == '\r' || rx_ring[i] == '\n')
        {
            return true;
        }
        i = (i + 1U) % RX_BUFFER_SIZE;
    }

    return false;
}

/**
 * @brief Read one complete line out of the ring buffer.
 * @param buf Destination buffer; receives the line without \\r\\n, always
 *            null-terminated.
 * @param len Size of @p buf in bytes; longer lines are truncated.
 * @author __________
 */
void UART_ReadLine(char *buf, size_t len)
{
    if (buf == NULL || len == 0U) { return; }

    size_t i = 0U;

    while ((rx_tail != rx_head) && (i < (len - 1U)))
    {
        uint8_t c = rx_ring[rx_tail];
        rx_tail   = (rx_tail + 1U) % RX_BUFFER_SIZE;

        if (c == '\r' || c == '\n')
        {
            /* Windows line ending \r\n: consume the following \n as well, so
             * it does not show up as an empty line on the next call */
            if ((c == '\r') && (rx_tail != rx_head) && (rx_ring[rx_tail] == '\n'))
            {
                rx_tail = (rx_tail + 1U) % RX_BUFFER_SIZE;
            }
            break;  /* Line complete - return it without the \r/\n */
        }

        buf[i++] = (char)c;
    }

    buf[i] = '\0';
}
