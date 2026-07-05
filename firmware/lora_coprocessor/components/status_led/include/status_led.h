/**
 * @file status_led.h
 * @brief WS2812 RGB status LED patterns for the P-3 V1.6 module (GPIO8, GRB).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-level LED indication modes (mirrors ExpressLRS-style semantics).
 */
typedef enum
{
    STATUS_LED_MODE_BOOTING   = 0, /**< Rainbow fade during start-up. */
    STATUS_LED_MODE_READY     = 1, /**< Slow green blink — idle, UART ready. */
    STATUS_LED_MODE_STREAMING = 2, /**< Cyan pulse — telemetry stream active. */
    STATUS_LED_MODE_RADIO_ERR = 3, /**< Fast red blink — LR1121 not detected. */
    STATUS_LED_MODE_OFF       = 4  /**< LED forced off (AT+LED=0). */
} status_led_mode_t;

/**
 * @brief Initialize the WS2812 driver and start the pattern task.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t StatusLed_Init(void);

/**
 * @brief Select the active indication mode.
 *
 * @param[in] mode  Pattern to display.
 *
 * @return ESP_OK on success.
 */
esp_err_t StatusLed_Set_Mode(status_led_mode_t mode);

/**
 * @brief Return the current indication mode.
 */
status_led_mode_t StatusLed_Get_Mode(void);

/**
 * @brief Brief white flash on successful host command (non-blocking).
 */
void StatusLed_Pulse_Ack(void);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_LED_H */
