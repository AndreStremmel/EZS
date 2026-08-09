/**
 ******************************************************************************
 * @file    uart_driver.c
 * @brief   UART-Abstraktion – Implementierung
 *
 * RX-Architektur: Interrupt-getriebener Ringpuffer
 * ─────────────────────────────────────────────────
 *  UART RX-Interrupt
 *       │
 *       ▼  HAL_UART_RxCpltCallback()
 *  rx_ring[ rx_head ]  ←  empfangenes Byte
 *  rx_head = (rx_head + 1) % RX_BUFFER_SIZE
 *       │
 *       ▼  UartShell-Task (praeemptiver DOS-RTOS-Scheduler)
 *  UART_LineAvailable() → sucht \\r oder \\n im Puffer
 *  UART_ReadLine()      → liest bis zum Zeilenende, gibt ohne \\r\\n zurück
 *
 * TX-Architektur: Blockierendes Polling
 * ──────────────────────────────────────
 *  UART_SendString / SendUInt → HAL_UART_Transmit (Polling, HAL_MAX_DELAY)
 *  Kurze Strings (<40 Zeichen) senden bei 115200 Baud in < 3 ms.
 *  TX erfolgt unter g_uartMutex; der sendende Task darf dabei
 *  praeemptiert werden, der Mutex schuetzt die Ausgabe-Integritaet.
 *
 * Welcher UART-Handle und IRQ benutzt wird:
 *  board_config.h definiert APP_UART_HANDLE und APP_UART_IRQn.
 ******************************************************************************
 */

#include "uart_driver.h"
#include "board_config.h"
#include "app_resources.h"
#include "os_semaphore.h"
#include <string.h>

/* ==========================================================================
 * Ringpuffer für den Empfang
 * ========================================================================== */

#define RX_BUFFER_SIZE  128U    /* Muss eine Potenz von 2 sein, max 256 bei uint8_t Index */

static uint8_t           rx_ring[RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0U;   /* Schreibposition (IRQ)         */
static volatile uint16_t rx_tail = 0U;   /* Leseposition   (Task)         */
static uint8_t           rx_byte;        /* HAL-Empfangspuffer (1 Byte)   */

/* ==========================================================================
 * Initialisierung
 * ========================================================================== */

/**
 * @brief  NVIC für UART aktivieren und ersten Byte-Empfang per IT starten.
 *
 * Muss in main.c nach MX_USARTx_UART_Init() aufgerufen werden:
 *
 *   MX_USART1_UART_Init();   // CubeMX-Init
 *   ...
 *   UART_Init();             // Interrupt starten
 *   Scheduler_vInit();
 */
void UART_Init(void)
{
    /* Priorität 6: niedriger als SysTick (0) und ECHO-EXTI (5),
     * höher als PendSV (0xFF) – UART-Zeichen gehen nicht verloren,
     * stören aber den Sensor-Timing nicht. */
    HAL_NVIC_SetPriority(APP_UART_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(APP_UART_IRQn);

    /* Ersten Empfang starten – HAL reaktiviert sich in RxCpltCallback */
    HAL_UART_Receive_IT(&APP_UART_HANDLE, &rx_byte, 1U);
}

/* ==========================================================================
 * RX – Interrupt-Callback
 * ========================================================================== */

/**
 * @brief  Wird von HAL nach jedem empfangenen Byte aufgerufen.
 *         Schreibt in den Ringpuffer und startet den nächsten Empfang.
 *
 * @note   Diese Funktion überschreibt den weak-Stub in stm32l4xx_hal_uart.c.
 *         Sie wird aus dem USART1_IRQHandler aufgerufen (via HAL_UART_IRQHandler).
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == APP_UART_HANDLE.Instance)
    {
        uint16_t next_head = (rx_head + 1U) % RX_BUFFER_SIZE;

        if (next_head != rx_tail)   /* Puffer nicht voll – Byte übernehmen */
        {
            rx_ring[rx_head] = rx_byte;
            rx_head          = next_head;

            /* Zeilenende -> Shell-Task aufwecken (statt Polling).
             * Give ist ISR-sicher; binaere Semaphore verschluckt
             * Mehrfach-Gives bei \r\n automatisch. */
            if (rx_byte == (uint8_t)'\r' || rx_byte == (uint8_t)'\n')
            {
                OS_Semaphore_Give(&g_uartRxSemaphore);
            }
        }
        /* Bei vollem Puffer: Byte verwerfen, aber Empfang nicht stoppen */

        HAL_UART_Receive_IT(&APP_UART_HANDLE, &rx_byte, 1U);
    }
}

/* ==========================================================================
 * TX – Senden
 * ========================================================================== */

void UART_SendString(const char *str)
{
    if (str == NULL) { return; }

    size_t len = strlen(str);
    if (len == 0U) { return; }

    /* Cast: HAL erwartet uint8_t*, String ist const char* – semantisch identisch */
    HAL_UART_Transmit(&APP_UART_HANDLE,
                      (const uint8_t *)str,
                      (uint16_t)len,
                      HAL_MAX_DELAY);
}

void UART_SendUInt(uint16_t value)
{
    char    buf[6U];   /* max. 5 Stellen für 65535, plus '\0' */
    uint8_t i = 0U;
    uint8_t j = 0U;

    if (value == 0U)
    {
        UART_SendString("0");
        return;
    }

    /* Ziffern in umgekehrter Reihenfolge aufbauen */
    while (value > 0U)
    {
        buf[i++] = (char)('0' + (value % 10U));
        value    /= 10U;
    }

    /* In-Place umkehren: "321" → "123" */
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
 * RX – Zeilenbasiertes Lesen
 * ========================================================================== */

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
            /* Windows-Zeilenende \r\n: das folgende \n noch konsumieren */
            if ((c == '\r') && (rx_tail != rx_head) && (rx_ring[rx_tail] == '\n'))
            {
                rx_tail = (rx_tail + 1U) % RX_BUFFER_SIZE;
            }
            break;  /* Zeile fertig – ohne \r/\n im Puffer zurückgeben */
        }

        buf[i++] = (char)c;
    }

    buf[i] = '\0';
}
