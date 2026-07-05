/**
 * @file host_uart.h
 * @brief Host-facing UART link (ESP32-P4 or USB-UART bench adapter).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef HOST_UART_H
#define HOST_UART_H

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of one received command line (including NUL). */
#define HOST_UART_LINE_CAPACITY         (128U)

/**
 * @brief Callback invoked for each complete line received on the host UART.
 *
 * @param[in] line      NUL-terminated line without CR/LF.
 * @param[in] user_ctx  Opaque pointer from ::HostUart_Start.
 */
typedef void (*host_uart_line_cb_t)(const char *line, void *user_ctx);

/**
 * @brief Install and configure the host UART selected in menuconfig.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t HostUart_Init(void);

/**
 * @brief Start the RX reader task that dispatches complete lines.
 *
 * @param[in] callback  Line handler (must not be NULL).
 * @param[in] user_ctx  Forwarded to the callback.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t HostUart_Start(host_uart_line_cb_t callback, void *user_ctx);

/**
 * @brief Send a NUL-terminated string on the host UART (blocking TX drain).
 *
 * @param[in] text  Message to send.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t HostUart_Write(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HOST_UART_H */
