/**
 * @file bsp_pins.h
 * @brief On-board (non Expand IO) pin map for the Guition JC4880P443C.
 *
 * These GPIOs belong to peripherals soldered on the module itself (LCD panel,
 * backlight, touch bus) and are only ever used by the board support package.
 * Values are taken from the board schematics under @docs/ and cross-checked
 * against the vendor firmware.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#ifndef BSP_PINS_H
#define BSP_PINS_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ST7701S panel hardware reset line (active low). */
#define BSP_PIN_LCD_RESET       (GPIO_NUM_5)

/** LCD backlight enable / PWM dimming line. */
#define BSP_PIN_LCD_BACKLIGHT   (GPIO_NUM_23)

/*
 * The GT911 capacitive touch controller shares the module's I2C bus, whose pins
 * are defined centrally in expand_io_pins.h (GPIO7 = SDA, GPIO8 = SCL). The
 * touch INT and RST lines are not routed on this module, hence configured as -1.
 */
#define BSP_TOUCH_GPIO_INT      (-1)
#define BSP_TOUCH_GPIO_RST      (-1)

#ifdef __cplusplus
}
#endif

#endif /* BSP_PINS_H */
