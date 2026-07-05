/**
 * @file expand_io_hal.c
 * @brief Implementation of the Expand IO hardware abstraction layer.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "expand_io_hal.h"
#include "expand_io_pins.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "expand_io";

static i2c_master_bus_handle_t s_i2c_bus   = NULL;
static bool                    s_initialized = false;

esp_err_t ExpandIo_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        const i2c_master_bus_config_t i2c_cfg =
        {
            .i2c_port                     = EXPAND_IO_I2C_PORT,
            .sda_io_num                   = EXPAND_IO_PIN_I2C_SDA,
            .scl_io_num                   = EXPAND_IO_PIN_I2C_SCL,
            .clk_source                   = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt            = EXPAND_IO_I2C_GLITCH_CNT,
            .flags.enable_internal_pullup = true,
        };

        status = i2c_new_master_bus(&i2c_cfg, &s_i2c_bus);

        if (status == ESP_OK)
        {
            s_initialized = true;
            ESP_LOGI(TAG, "Expand IO I2C bus ready (SDA=%d SCL=%d)",
                     (int)EXPAND_IO_PIN_I2C_SDA, (int)EXPAND_IO_PIN_I2C_SCL);
        }
    }

    return status;
}

i2c_master_bus_handle_t ExpandIo_I2c_Get_Bus(void)
{
    return s_i2c_bus;
}

esp_err_t ExpandIo_Uart_Init(uart_port_t port,
                             int tx_pin,
                             int rx_pin,
                             uint32_t baud_rate,
                             size_t rx_buf_sz,
                             size_t tx_buf_sz)
{
    esp_err_t status = ESP_OK;

    if ((tx_pin < 0) || (rx_pin < 0))
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        const uart_config_t uart_cfg =
        {
            .baud_rate           = (int)baud_rate,
            .data_bits           = UART_DATA_8_BITS,
            .parity              = UART_PARITY_DISABLE,
            .stop_bits           = UART_STOP_BITS_1,
            .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
            .source_clk          = UART_SCLK_DEFAULT,
        };

        status = uart_driver_install(port, (int)rx_buf_sz, (int)tx_buf_sz, 0U, NULL, 0U);

        if (status == ESP_OK)
        {
            status = uart_param_config(port, &uart_cfg);
        }

        if (status == ESP_OK)
        {
            status = uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        }

        if (status == ESP_OK)
        {
            ESP_LOGI(TAG, "UART%d ready (TX=%d RX=%d @ %u baud)",
                     (int)port, tx_pin, rx_pin, (unsigned int)baud_rate);
        }
        else
        {
            (void)uart_driver_delete(port);
        }
    }

    return status;
}

int ExpandIo_Uart_Write(uart_port_t port,
                        const uint8_t *data,
                        size_t length,
                        uint32_t timeout_ms)
{
    int result = 0;

    if (data == NULL)
    {
        result = (int)ESP_ERR_INVALID_ARG;
    }
    else
    {
        const int written = uart_write_bytes(port, (const char *)data, (size_t)length);
        if (written < 0)
        {
            result = written;
        }
        else
        {
            const esp_err_t wait_status = uart_wait_tx_done(port, pdMS_TO_TICKS(timeout_ms));
            if (wait_status != ESP_OK)
            {
                result = (int)wait_status;
            }
            else
            {
                result = written;
            }
        }
    }

    return result;
}

int ExpandIo_Uart_Read(uart_port_t port,
                       uint8_t *out_data,
                       size_t max_length,
                       uint32_t timeout_ms)
{
    int result = 0;

    if (out_data == NULL)
    {
        result = (int)ESP_ERR_INVALID_ARG;
    }
    else
    {
        result = uart_read_bytes(port, out_data, max_length, pdMS_TO_TICKS(timeout_ms));
    }

    return result;
}

esp_err_t ExpandIo_Uart_Flush_Rx(uart_port_t port)
{
    return uart_flush_input(port);
}
