/**
 * @file lora_driver.h
 * @brief UART driver for the LR1121 868 MHz LORA transceiver (Expand IO).
 *
 * Probes the module over UART (AT-style firmware) and exposes a compact status
 * snapshot suitable for chat telemetry. Raw SPI LR1121 control is out of scope
 * for stage 1.2 because the hardware is wired for UART on GPIO51/52.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default UART baud rate for AT-firmware LORA modules. */
#define LORA_DRIVER_UART_BAUD           (115200U)

/** Maximum length of the human-readable status string. */
#define LORA_DRIVER_STATUS_CAPACITY     (96U)

/**
 * @brief Snapshot of the LORA UART link and last probe response.
 */
typedef struct
{
    bool     uart_ready;   /**< UART port installed successfully. */
    bool     at_responded; /**< Module answered the last AT probe. */
    uint32_t rx_byte_count; /**< Total bytes received since init. */
    char     status[LORA_DRIVER_STATUS_CAPACITY]; /**< Short summary for chat. */
} lora_snapshot_t;

/**
 * @brief Initialize the LORA UART and perform an initial AT probe.
 *
 * Requires ::ExpandIo_Init to have succeeded beforehand.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t Lora_Driver_Init(void);

/**
 * @brief Refresh the probe (drain RX, send AT) and copy the latest snapshot.
 *
 * @param[out] out_snapshot  Destination structure.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p out_snapshot is NULL,
 *         ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t Lora_Driver_Poll(lora_snapshot_t *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif /* LORA_DRIVER_H */
