/**
 * @file bsp_display.h
 * @brief Board support: MIPI-DSI (ST7701S) display, backlight and GT911 touch.
 *
 * Brings up the display stack for the Guition JC4880P443C and hands it over to
 * LVGL through esp_lvgl_port. After ::Bsp_Display_Init succeeds, LVGL is running
 * and the default display/input device are registered; UI code interacts with
 * LVGL under the port lock (lvgl_port_lock/lvgl_port_unlock).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Physical panel width in pixels (portrait orientation). */
#define BSP_LCD_H_RES   (480U)

/** Physical panel height in pixels (portrait orientation). */
#define BSP_LCD_V_RES   (800U)

/**
 * @brief Initialize the full display stack: DSI panel, touch, backlight and LVGL.
 *
 * Intended to be called once from app_main during system start-up.
 *
 * @return ESP_OK on success (aborts via ESP_ERROR_CHECK on fatal HW failure,
 *         as permitted for one-time hardware initialization).
 */
esp_err_t Bsp_Display_Init(void);

/**
 * @brief Set the LCD backlight brightness.
 *
 * @param[in] percent  Brightness in the range [0, 100]; values above 100 are clamped.
 */
void Bsp_Display_Set_Backlight(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DISPLAY_H */
