/**
 * @file lora_driver.c
 * @brief UART-based LR1121 LORA module driver (stage 1.2 probe/status).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "lora_driver.h"

#include <string.h>
#include <stdio.h>

#include "expand_io_hal.h"
#include "expand_io_pins.h"

#include "esp_log.h"

#define LORA_UART_RX_BUF_SIZE           (1024U)
#define LORA_UART_TX_BUF_SIZE           (256U)
#define LORA_AT_CMD                     "AT\r\n"
#define LORA_AT_PROBE_TIMEOUT_MS        (200U)
#define LORA_RX_DRAIN_CHUNK              (64U)
#define LORA_RESPONSE_CAPACITY           (48U)

static const char *TAG = "lora_driver";

static lora_snapshot_t s_snapshot;
static bool            s_initialized = false;

/**
 * @brief Drain the UART RX buffer and count bytes.
 */
static uint32_t lora_drain_rx(void)
{
    uint8_t chunk[LORA_RX_DRAIN_CHUNK];
    uint32_t total = 0U;

    for (;;)
    {
        const int nbytes = ExpandIo_Uart_Read(EXPAND_IO_UART_PORT_LORA,
                                              chunk,
                                              sizeof(chunk),
                                              0U);
        if (nbytes <= 0)
        {
            break;
        }
        total += (uint32_t)nbytes;
    }

    return total;
}

/**
 * @brief Send an AT probe and capture a short response line.
 */
static void lora_run_at_probe(void)
{
    uint8_t  response[LORA_RESPONSE_CAPACITY];
    int      nbytes;
    size_t   idx;

    (void)ExpandIo_Uart_Flush_Rx(EXPAND_IO_UART_PORT_LORA);
    (void)ExpandIo_Uart_Write(EXPAND_IO_UART_PORT_LORA,
                              (const uint8_t *)LORA_AT_CMD,
                              strlen(LORA_AT_CMD),
                              LORA_AT_PROBE_TIMEOUT_MS);

    (void)memset(response, 0, sizeof(response));
    nbytes = ExpandIo_Uart_Read(EXPAND_IO_UART_PORT_LORA,
                                response,
                                sizeof(response) - 1U,
                                LORA_AT_PROBE_TIMEOUT_MS);

    if (nbytes > 0)
    {
        s_snapshot.rx_byte_count += (uint32_t)nbytes;

        for (idx = 0U; idx < (size_t)nbytes; idx++)
        {
            if ((response[idx] == '\r') || (response[idx] == '\n'))
            {
                response[idx] = '\0';
                break;
            }
        }

        if (strstr((const char *)response, "OK") != NULL)
        {
            s_snapshot.at_responded = true;
            (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status), "AT OK");
        }
        else if (response[0] != '\0')
        {
            s_snapshot.at_responded = true;
            (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status), "%s", (const char *)response);
        }
        else
        {
            s_snapshot.at_responded = false;
            (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status), "rx %dB no AT", nbytes);
        }
    }
    else
    {
        s_snapshot.at_responded = false;
        (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status), "no response");
    }
}

esp_err_t Lora_Driver_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
        (void)strncpy(s_snapshot.status, "init", sizeof(s_snapshot.status) - 1U);

        status = ExpandIo_Uart_Init(EXPAND_IO_UART_PORT_LORA,
                                    (int)EXPAND_IO_PIN_LORA_TX,
                                    (int)EXPAND_IO_PIN_LORA_RX,
                                    LORA_DRIVER_UART_BAUD,
                                    LORA_UART_RX_BUF_SIZE,
                                    LORA_UART_TX_BUF_SIZE);

        if (status == ESP_OK)
        {
            s_snapshot.uart_ready = true;
            s_snapshot.rx_byte_count += lora_drain_rx();
            lora_run_at_probe();
            s_initialized = true;
            ESP_LOGI(TAG, "LORA driver ready (UART%d @ %u baud, status: %s)",
                     (int)EXPAND_IO_UART_PORT_LORA,
                     (unsigned int)LORA_DRIVER_UART_BAUD,
                     s_snapshot.status);
        }
    }

    return status;
}

esp_err_t Lora_Driver_Poll(lora_snapshot_t *out_snapshot)
{
    esp_err_t status = ESP_OK;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (out_snapshot == NULL)
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        s_snapshot.rx_byte_count += lora_drain_rx();
        lora_run_at_probe();
        *out_snapshot = s_snapshot;
    }

    return status;
}
