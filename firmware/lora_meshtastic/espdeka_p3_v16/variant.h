/**

 * @file variant.h

 * @brief Meshtastic pin map for P-3 V1.6 per module schematic (ESP32-C3 + LR1121).

 *

 * Bench UART (left pads):  TX=GPIO21, RX=GPIO20  (USB-UART / console).

 * Host UART (RX2/TX2):    TX=GPIO19, RX=GPIO18  (ESP32-P4 Expand IO).

 *

 * @author Freimor

 * @agent Cursor AI

 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1

 * @date 2026-07-03

 * @license MIT

 */



#ifndef _VARIANT_ESPDEKA_P3_V16_

#define _VARIANT_ESPDEKA_P3_V16_



#define HAS_SCREEN 0

#define HAS_GPS 0

#undef GPS_RX_PIN

#undef GPS_TX_PIN

/* No on-board I2C peripherals (MESHTASTIC_EXCLUDE_I2C=1). */

/* Coprocessor-proven SPI clock (ESP-IDF lr1121_radio.c uses 1 MHz). */
#define LORA_SPI_FREQUENCY 1000000

/* WS2812 RGB on GPIO8 (GRB). */

#define HAS_NEOPIXEL

#define NEOPIXEL_COUNT 1

#define NEOPIXEL_DATA 8

#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800)



/* LR1121 — P-3 V1.6 schematic: MOSI=4, MISO=5, SCK=6, NSS=7, DIO1=1. */
#define USE_LR1121

#define LORA_SCK 6
#define LORA_MISO 5
#define LORA_MOSI 4
#define LORA_CS 7
#define LORA_RESET 2
#define LORA_DIO1 1
#define LORA_BUSY 3

#define LORA_DIO0 RADIOLIB_NC

#define LORA_DIO2 RADIOLIB_NC



#ifdef USE_LR1121

#define LR1121_IRQ_PIN LORA_DIO1

#define LR1121_NRESET_PIN LORA_RESET

#define LR1121_BUSY_PIN LORA_BUSY

#define LR1121_SPI_NSS_PIN LORA_CS

#define LR1121_SPI_SCK_PIN LORA_SCK

#define LR1121_SPI_MOSI_PIN LORA_MOSI

#define LR1121_SPI_MISO_PIN LORA_MISO

/* External FEM/switches via LR1121 DIO5/DIO6 (900 MHz + 2.4 GHz paths on schematic). */
#define LR11X0_DIO_AS_RF_SWITCH

#endif



#endif /* _VARIANT_ESPDEKA_P3_V16_ */


