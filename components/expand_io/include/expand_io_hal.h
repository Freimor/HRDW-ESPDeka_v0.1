/**
 * @file expand_io_hal.h
 * @brief Hardware abstraction for the Expand IO connector (I2C and UART).
 *
 * Owns the shared I2C master bus (touch, GNSS compass, future sensors) and
 * provides thin UART helpers for GNSS and LORA modules. Pin numbers are taken
 * exclusively from expand_io_pins.h.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef EXPAND_IO_HAL_H
#define EXPAND_IO_HAL_H

#include <stdint.h>
#include <stddef.h>

#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** I2C port used for the Expand IO bus. */
#define EXPAND_IO_I2C_PORT              (I2C_NUM_0)

/** Glitch filter count for the shared I2C bus. */
#define EXPAND_IO_I2C_GLITCH_CNT        (7U)

/** UART port assigned to the GNSS module. */
#define EXPAND_IO_UART_PORT_GNSS        (UART_NUM_1)

/** UART port assigned to the LORA module. */
#define EXPAND_IO_UART_PORT_LORA        (UART_NUM_2)

/**
 * @brief Initialize the Expand IO HAL (shared I2C bus).
 *
 * Safe to call exactly once during start-up. UART ports are opened separately
 * by the GNSS and LORA drivers.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already initialized,
 *         or another esp_err_t on failure.
 */
esp_err_t ExpandIo_Init(void);

/**
 * @brief Return the shared I2C master bus handle.
 *
 * @return Bus handle, or NULL if ::ExpandIo_Init has not succeeded.
 */
i2c_master_bus_handle_t ExpandIo_I2c_Get_Bus(void);

/**
 * @brief Install and configure a UART on Expand IO pins.
 *
 * @param[in] port       UART controller index.
 * @param[in] tx_pin     MCU TX pin (connects to peripheral RX).
 * @param[in] rx_pin     MCU RX pin (connects to peripheral TX).
 * @param[in] baud_rate  Line speed in bits per second.
 * @param[in] rx_buf_sz  Driver RX ring buffer size in bytes.
 * @param[in] tx_buf_sz  Driver TX ring buffer size in bytes.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t ExpandIo_Uart_Init(uart_port_t port,
                             int tx_pin,
                             int rx_pin,
                             uint32_t baud_rate,
                             size_t rx_buf_sz,
                             size_t tx_buf_sz);

/**
 * @brief Write bytes to an Expand IO UART (blocking, finite timeout).
 *
 * @param[in] port      UART controller index.
 * @param[in] data      Source buffer.
 * @param[in] length    Number of bytes to send.
 * @param[in] timeout_ms  Maximum wait in milliseconds.
 *
 * @return Number of bytes written, or a negative esp_err_t cast to int on error.
 */
int ExpandIo_Uart_Write(uart_port_t port,
                        const uint8_t *data,
                        size_t length,
                        uint32_t timeout_ms);

/**
 * @brief Read bytes from an Expand IO UART (blocking, finite timeout).
 *
 * @param[in]  port       UART controller index.
 * @param[out] out_data   Destination buffer.
 * @param[in]  max_length Maximum bytes to read.
 * @param[in]  timeout_ms Maximum wait in milliseconds.
 *
 * @return Number of bytes read (0 if timed out with no data), or a negative
 *         esp_err_t cast to int on error.
 */
int ExpandIo_Uart_Read(uart_port_t port,
                       uint8_t *out_data,
                       size_t max_length,
                       uint32_t timeout_ms);

/**
 * @brief Discard all bytes currently buffered in the UART RX FIFO/driver.
 *
 * @param[in] port  UART controller index.
 *
 * @return ESP_OK on success.
 */
esp_err_t ExpandIo_Uart_Flush_Rx(uart_port_t port);

#ifdef __cplusplus
}
#endif

#endif /* EXPAND_IO_HAL_H */
