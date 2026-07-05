/**
 * @file chat_ui.h
 * @brief LVGL chat user interface for Stage 1.1.
 *
 * Builds a simple messenger screen (message history, text input and an
 * on-screen keyboard) on top of the running LVGL instance and binds it to the
 * chat core. Must be called after ::Bsp_Display_Init and ::ChatCore_Init.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#ifndef CHAT_UI_H
#define CHAT_UI_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build the chat UI and subscribe it to the chat core.
 *
 * @return ESP_OK on success, or an error code from the chat core registration.
 */
esp_err_t ChatUi_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* CHAT_UI_H */
