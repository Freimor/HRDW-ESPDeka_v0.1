/**
 * @file at_shell.h
 * @brief Full AT command handler for the LORA coprocessor host link.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef AT_SHELL_H
#define AT_SHELL_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Telemetry line capacity (fits ESPDeka chat_core message width). */
#define AT_SHELL_TELEMETRY_CAPACITY     (96U)

/** Response line capacity for multi-field AT answers. */
#define AT_SHELL_RESPONSE_CAPACITY      (128U)

/**
 * @brief Initialize the AT shell (no hardware dependencies).
 *
 * @return ESP_OK always.
 */
esp_err_t AtShell_Init(void);

/**
 * @brief Notify the shell that LR1121 probe finished (updates LED policy).
 *
 * @param[in] radio_ok  true when ::Lr1121_Radio_Is_Ready returned true.
 */
void AtShell_Notify_Radio_Probe(bool radio_ok);

/**
 * @brief Parse and execute one command line from the host.
 *
 * @param[in] line  NUL-terminated input (without line endings).
 */
void AtShell_Handle_Line(const char *line);

/**
 * @brief Query whether periodic telemetry streaming is enabled.
 *
 * @return true if AT+STREAM=1 (or /start alias) was issued.
 */
bool AtShell_Is_Stream_Enabled(void);

/**
 * @brief Build the current telemetry line into @p out_buf.
 *
 * @param[out] out_buf       Destination buffer.
 * @param[in]  out_capacity  Size of @p out_buf in bytes.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if buffers are invalid.
 */
esp_err_t AtShell_Format_Telemetry(char *out_buf, size_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif /* AT_SHELL_H */
