/**
 * @file app_main.c
 * @brief ESPDeka firmware entry point (Stage 1.1: RTOS bring-up + chat app).
 *
 * Boot flow:
 *   1. Start the chat engine (seeds the virtual contact + welcome message).
 *   2. Bring up the display, touch and LVGL.
 *   3. Build the chat UI and bind it to the engine.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#include "esp_log.h"

#include "chat_core.h"
#include "bsp_display.h"
#include "chat_ui.h"

static const char *TAG = "app_main";

/**
 * @brief Application entry point invoked by the ESP-IDF/FreeRTOS start-up code.
 *
 * ESP_ERROR_CHECK is used deliberately here: these are one-time hardware and
 * subsystem initializations, and a failure at this stage is unrecoverable.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ESPDeka Stage 1.1 starting");

    ESP_ERROR_CHECK(ChatCore_Init());
    ESP_ERROR_CHECK(Bsp_Display_Init());
    ESP_ERROR_CHECK(ChatUi_Init());

    ESP_LOGI(TAG, "startup complete; chat is live");
}
