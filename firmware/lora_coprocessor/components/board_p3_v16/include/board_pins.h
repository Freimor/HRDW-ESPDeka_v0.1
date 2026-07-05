/**
 * @file board_pins.h
 * @brief Pin map for the P-3 V1.6 ESP32-C3 + LR1121 module (ex-ExpressLRS RX).
 *
 * SPI per P-3 V1.6 schematic: MOSI=4, MISO=5, SCK=6, NSS=7, DIO1=1, LED=8.
 * verify RX2/TX2 against your schematic before production wiring to the P4.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "driver/gpio.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Firmware version string reported by AT+GMR. */
#define BOARD_FW_VERSION                "ESPDeka-LORA-Copro v0.2.3"

/*
 * -----------------------------------------------------------------------------
 * Host UART (ESP32-P4 Expand IO or bench USB-UART on the left-edge pads).
 * ESP32-C3 UART0 defaults: RX=GPIO20, TX=GPIO21 (left silk: RX / TX).
 * Top-edge RX2/TX2 pads are routed to UART1 below — confirm on schematic.
 * -----------------------------------------------------------------------------
 */
#define BOARD_HOST_UART_PORT            (UART_NUM_0)
#define BOARD_HOST_UART_TX_PIN          (GPIO_NUM_21)
#define BOARD_HOST_UART_RX_PIN          (GPIO_NUM_20)
#define BOARD_HOST_UART_BAUD            (115200U)

#define BOARD_HOST_UART1_PORT           (UART_NUM_1)
#define BOARD_HOST_UART1_TX_PIN         (GPIO_NUM_19)
#define BOARD_HOST_UART1_RX_PIN         (GPIO_NUM_18)

/*
 * -----------------------------------------------------------------------------
 * LR1121 radio (SPI) — ExpressLRS Generic C3 LR1121 layout.
 * Stage A does not drive the radio yet; pins are defined for stage B bring-up.
 * -----------------------------------------------------------------------------
 */
#define BOARD_LR1121_PIN_SCK            (GPIO_NUM_6)
#define BOARD_LR1121_PIN_MISO           (GPIO_NUM_5)
#define BOARD_LR1121_PIN_MOSI           (GPIO_NUM_4)
#define BOARD_LR1121_PIN_CS             (GPIO_NUM_7)
#define BOARD_LR1121_PIN_RESET          (GPIO_NUM_2)
#define BOARD_LR1121_PIN_BUSY           (GPIO_NUM_3)
#define BOARD_LR1121_PIN_DIO1           (GPIO_NUM_1)

/** WS2812-style RGB status LED (ExpressLRS led_rgb = 8). */
#define BOARD_LED_RGB_PIN               (GPIO_NUM_8)

#ifdef __cplusplus
}
#endif

#endif /* BOARD_PINS_H */
