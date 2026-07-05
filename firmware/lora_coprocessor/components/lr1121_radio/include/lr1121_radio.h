/**
 * @file lr1121_radio.h
 * @brief LR1121 bring-up and health probe over SPI (P-3 V1.6 pinout).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef LR1121_RADIO_H
#define LR1121_RADIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Human-readable radio status for AT+LR1121? */
#define LR1121_RADIO_STATUS_CAPACITY    (64U)

/**
 * @brief LR1121 probe result after reset and BUSY handshake.
 */
typedef enum
{
    LR1121_RADIO_STATE_NOT_INIT = 0,
    LR1121_RADIO_STATE_OK       = 1,
    LR1121_RADIO_STATE_FAULT    = 2
} lr1121_radio_state_t;

/**
 * @brief Initialize SPI, control GPIOs and probe the LR1121 transceiver.
 *
 * @return ESP_OK when the chip responds (BUSY handshake OK), ESP_FAIL on probe
 *         failure, or another esp_err_t for driver errors.
 */
esp_err_t Lr1121_Radio_Init(void);

/**
 * @brief Return the cached probe state.
 */
lr1121_radio_state_t Lr1121_Radio_Get_State(void);

/**
 * @brief Format radio status for AT responses.
 *
 * @param[out] out_buf       Destination buffer.
 * @param[in]  out_capacity  Buffer size in bytes.
 *
 * @return ESP_OK on success.
 */
esp_err_t Lr1121_Radio_Format_Status(char *out_buf, size_t out_capacity);

/**
 * @brief Return true when the LR1121 passed the BUSY probe.
 */
bool Lr1121_Radio_Is_Ready(void);

#ifdef __cplusplus
}
#endif

#endif /* LR1121_RADIO_H */
