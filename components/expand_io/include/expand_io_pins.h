/**
 * @file expand_io_pins.h
 * @brief Centralized pin map for the "Expand IO" connector of the JC4880P443C.
 *
 * This is the SINGLE source of truth for every GPIO routed through the Expand IO
 * connector. Business logic and peripheral drivers MUST reference these symbols
 * (through the Expand IO HAL) and MUST NOT hardcode raw GPIO numbers.
 *
 * Each definition documents the physical Expand IO connector pin and the
 * corresponding ESP32-P4 GPIO number, as taken from PROJECT_BRIEF.md and the
 * board schematics under @docs/.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#ifndef EXPAND_IO_PINS_H
#define EXPAND_IO_PINS_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * Shared I2C bus (Expand IO + on-board capacitive touch controller).
 * NOTE: On this module the on-board GT911 touch panel is wired to the same
 *       pins used by the Expand IO I2C bus, so any external I2C sensor added
 *       later must share this single bus instance owned by the HAL.
 * -----------------------------------------------------------------------------
 */
#define EXPAND_IO_PIN_I2C_SDA           (GPIO_NUM_7)   /* Expand IO I2C SDA  -> ESP32-P4 GPIO7 */
#define EXPAND_IO_PIN_I2C_SCL           (GPIO_NUM_8)   /* Expand IO I2C SCL  -> ESP32-P4 GPIO8 */

/*
 * -----------------------------------------------------------------------------
 * LR1121 868 MHz LORA transceiver (UART link over Expand IO).
 * Populated in roadmap stage 1.2.
 * -----------------------------------------------------------------------------
 */
#define EXPAND_IO_PIN_LORA_TX           (GPIO_NUM_52)  /* LORA module TX (device side) -> ESP32-P4 GPIO52 */
#define EXPAND_IO_PIN_LORA_RX           (GPIO_NUM_51)  /* LORA module RX (device side) -> ESP32-P4 GPIO51 */

/*
 * -----------------------------------------------------------------------------
 * GNSS RUSHFPV M10 PRO (UART data + shared I2C for its compass).
 * Populated in roadmap stage 1.2.
 * -----------------------------------------------------------------------------
 */
#define EXPAND_IO_PIN_GNSS_TX           (GPIO_NUM_31)  /* GNSS module TX -> ESP32-P4 GPIO31 */
#define EXPAND_IO_PIN_GNSS_RX           (GPIO_NUM_29)  /* GNSS module RX -> ESP32-P4 GPIO29 */
#define EXPAND_IO_PIN_GNSS_I2C_SCL      (EXPAND_IO_PIN_I2C_SCL) /* GNSS compass shares the I2C bus */
#define EXPAND_IO_PIN_GNSS_I2C_SDA      (EXPAND_IO_PIN_I2C_SDA) /* GNSS compass shares the I2C bus */

#ifdef __cplusplus
}
#endif

#endif /* EXPAND_IO_PINS_H */
