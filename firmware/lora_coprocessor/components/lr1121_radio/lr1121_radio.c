/**
 * @file lr1121_radio.c
 * @brief LR1121 SPI bus setup and BUSY-handshake health probe.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "lr1121_radio.h"

#include <string.h>
#include <stdio.h>

#include "board_pins.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define LR1121_SPI_HOST                     (SPI2_HOST)
#define LR1121_SPI_CLOCK_HZ                 (1000000U)
#define LR1121_OPCODE_GET_VERSION           (0x0101U)
#define LR1121_GET_VERSION_RESP_LEN         (5U)
#define LR1121_RESET_HOLD_MS                (20U)
#define LR1121_RESET_SETTLE_MS              (300U)
#define LR1121_BUSY_POLL_MS                 (1U)
#define LR1121_BUSY_TIMEOUT_MS              (100U)
#define LR1121_BUSY_READY_LEVEL             (0)
#define LR1121_USE_CASE_LR1121              (0x03U)
#define LR1121_USE_CASE_BOOTLOADER          (0xDFU)

static const char *TAG = "lr1121_radio";

static spi_device_handle_t   s_spi_dev   = NULL;
static lr1121_radio_state_t  s_state     = LR1121_RADIO_STATE_NOT_INIT;
static uint32_t              s_version   = 0U;
static bool                  s_bus_owned = false;

/**
 * @brief Parsed LR11XX GET_VERSION response fields.
 */
typedef struct
{
    uint8_t stat1;
    uint8_t hw;
    uint8_t use_case;
    uint8_t fw_major;
    uint8_t fw_minor;
} lr1121_version_info_t;

/**
 * @brief Configure LR1121 control GPIOs (CS, RESET, BUSY).
 */
static esp_err_t lr1121_gpio_init(void)
{
    esp_err_t status = ESP_OK;

    const gpio_config_t busy_cfg =
    {
        .pin_bit_mask = (1ULL << (uint32_t)BOARD_LR1121_PIN_BUSY),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    const gpio_config_t out_cfg =
    {
        .pin_bit_mask = (1ULL << (uint32_t)BOARD_LR1121_PIN_RESET),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    const gpio_config_t miso_cfg =
    {
        .pin_bit_mask = (1ULL << (uint32_t)BOARD_LR1121_PIN_MISO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    status = gpio_config(&busy_cfg);
    if (status == ESP_OK)
    {
        status = gpio_config(&out_cfg);
    }
    if (status == ESP_OK)
    {
        status = gpio_config(&miso_cfg);
    }

    if (status == ESP_OK)
    {
        (void)gpio_set_level(BOARD_LR1121_PIN_RESET, 1);
    }

    return status;
}

/**
 * @brief Re-apply MISO pull-up after the SPI driver claims the bus pins.
 */
static void lr1121_spi_apply_miso_pullup(void)
{
    (void)gpio_set_pull_mode(BOARD_LR1121_PIN_MISO, GPIO_PULLUP_ONLY);
}
static void lr1121_hardware_reset(void)
{
    (void)gpio_set_level(BOARD_LR1121_PIN_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(LR1121_RESET_HOLD_MS));
    (void)gpio_set_level(BOARD_LR1121_PIN_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(LR1121_RESET_SETTLE_MS));
}

/**
 * @brief Wait until BUSY indicates the chip can accept or return data.
 */
static bool lr1121_wait_busy_ready(uint32_t timeout_ms)
{
    bool ready = false;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < timeout_ms)
    {
        if (gpio_get_level(BOARD_LR1121_PIN_BUSY) == LR1121_BUSY_READY_LEVEL)
        {
            ready = true;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(LR1121_BUSY_POLL_MS));
        elapsed_ms += LR1121_BUSY_POLL_MS;
    }

    return ready;
}

/**
 * @brief Full-duplex SPI exchange with the LR1121 (NSS handled by the driver).
 */
static esp_err_t lr1121_spi_exchange(const uint8_t *tx_buf,
                                     uint8_t *rx_buf,
                                     size_t byte_count)
{
    esp_err_t         status = ESP_ERR_INVALID_ARG;
    spi_transaction_t transaction;

    if ((s_spi_dev == NULL) || (tx_buf == NULL) || (rx_buf == NULL) || (byte_count == 0U))
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        (void)memset(&transaction, 0, sizeof(transaction));
        transaction.length    = byte_count * 8U;
        transaction.tx_buffer = tx_buf;
        transaction.rx_buffer = rx_buf;
        status = spi_device_transmit(s_spi_dev, &transaction);
    }

    return status;
}

/**
 * @brief Send a 16-bit LR11XX opcode (write phase).
 */
static esp_err_t lr1121_write_opcode(uint16_t opcode)
{
    esp_err_t status = ESP_ERR_INVALID_STATE;
    uint8_t   tx_buf[2];
    uint8_t   rx_buf[2];

    if (s_spi_dev == NULL)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (!lr1121_wait_busy_ready(LR1121_BUSY_TIMEOUT_MS))
    {
        status = ESP_ERR_TIMEOUT;
    }
    else
    {
        tx_buf[0] = (uint8_t)((opcode >> 8) & 0xFFU);
        tx_buf[1] = (uint8_t)(opcode & 0xFFU);
        status    = lr1121_spi_exchange(tx_buf, rx_buf, sizeof(tx_buf));
    }

    return status;
}

/**
 * @brief Read response bytes after a command (host clocks NOPs on MOSI).
 */
static esp_err_t lr1121_read_response(uint8_t *out_buf, size_t byte_count)
{
    esp_err_t status = ESP_ERR_INVALID_ARG;
    uint8_t   tx_buf[LR1121_GET_VERSION_RESP_LEN];
    uint8_t   rx_buf[LR1121_GET_VERSION_RESP_LEN];

    if ((out_buf == NULL) || (byte_count == 0U) || (byte_count > sizeof(rx_buf)))
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else if (!lr1121_wait_busy_ready(LR1121_BUSY_TIMEOUT_MS))
    {
        status = ESP_ERR_TIMEOUT;
    }
    else
    {
        (void)memset(tx_buf, 0, byte_count);
        (void)memset(rx_buf, 0, sizeof(rx_buf));
        status = lr1121_spi_exchange(tx_buf, rx_buf, byte_count);

        if (status == ESP_OK)
        {
            (void)memcpy(out_buf, rx_buf, byte_count);
        }
    }

    return status;
}

/**
 * @brief Validate GET_VERSION fields for a genuine LR1121 response.
 */
static bool lr1121_version_is_valid(const lr1121_version_info_t *info)
{
    bool valid = false;

    if (info != NULL)
    {
        if (info->use_case == LR1121_USE_CASE_LR1121)
        {
            if ((info->fw_major != 0U) || (info->fw_minor != 0U))
            {
                valid = true;
            }
        }
    }

    return valid;
}

/**
 * @brief Pack version fields into the cached 32-bit status word.
 */
static uint32_t lr1121_pack_version(const lr1121_version_info_t *info)
{
    uint32_t packed = 0U;

    if (info != NULL)
    {
        packed = ((uint32_t)info->hw << 24)
               | ((uint32_t)info->use_case << 16)
               | ((uint32_t)info->fw_major << 8)
               | (uint32_t)info->fw_minor;
    }

    return packed;
}

/**
 * @brief Issue LR11XX GET_VERSION (0x0101) and parse the 5-byte response.
 */
static bool lr1121_read_version(lr1121_version_info_t *out_info)
{
    bool     success = false;
    uint8_t  response[LR1121_GET_VERSION_RESP_LEN];
    esp_err_t status;

    if (out_info == NULL)
    {
        success = false;
    }
    else
    {
        (void)memset(out_info, 0, sizeof(*out_info));
        (void)memset(response, 0, sizeof(response));

        status = lr1121_write_opcode(LR1121_OPCODE_GET_VERSION);
        if (status == ESP_OK)
        {
            status = lr1121_read_response(response, sizeof(response));
        }

        if (status == ESP_OK)
        {
            out_info->stat1    = response[0];
            out_info->hw       = response[1];
            out_info->use_case = (uint8_t)(response[2] & 0x0FU);
            out_info->fw_major = response[3];
            out_info->fw_minor = response[4];

            ESP_LOGI(TAG,
                     "GET_VERSION raw=%02X %02X %02X %02X %02X",
                     (unsigned int)response[0],
                     (unsigned int)response[1],
                     (unsigned int)response[2],
                     (unsigned int)response[3],
                     (unsigned int)response[4]);

            if (lr1121_version_is_valid(out_info))
            {
                success = true;
            }
            else if (out_info->use_case == LR1121_USE_CASE_BOOTLOADER)
            {
                ESP_LOGW(TAG,
                         "LR1121 in bootloader mode (uc=0xDF), not application firmware");
            }
            else
            {
                ESP_LOGW(TAG,
                         "GET_VERSION invalid: stat1=0x%02X hw=0x%02X uc=0x%02X fw=%u.%u",
                         (unsigned int)out_info->stat1,
                         (unsigned int)out_info->hw,
                         (unsigned int)out_info->use_case,
                         (unsigned int)out_info->fw_major,
                         (unsigned int)out_info->fw_minor);
            }
        }
        else
        {
            ESP_LOGE(TAG, "GET_VERSION SPI failed: %s", esp_err_to_name(status));
        }
    }

    return success;
}

esp_err_t Lr1121_Radio_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_state != LR1121_RADIO_STATE_NOT_INIT)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        status = lr1121_gpio_init();

        if (status == ESP_OK)
        {
            const spi_bus_config_t bus_cfg =
            {
                .mosi_io_num     = BOARD_LR1121_PIN_MOSI,
                .miso_io_num     = BOARD_LR1121_PIN_MISO,
                .sclk_io_num     = BOARD_LR1121_PIN_SCK,
                .quadwp_io_num   = -1,
                .quadhd_io_num   = -1,
                .max_transfer_sz = (int)LR1121_GET_VERSION_RESP_LEN,
            };

            status = spi_bus_initialize(LR1121_SPI_HOST, &bus_cfg, SPI_DMA_DISABLED);
            if (status == ESP_OK)
            {
                s_bus_owned = true;
            }
            else if (status == ESP_ERR_INVALID_STATE)
            {
                status = ESP_OK;
            }
        }

        if (status == ESP_OK)
        {
            const spi_device_interface_config_t dev_cfg =
            {
                .clock_speed_hz = (int)LR1121_SPI_CLOCK_HZ,
                .mode           = 0,
                .spics_io_num   = BOARD_LR1121_PIN_CS,
                .queue_size     = 1,
            };

            status = spi_bus_add_device(LR1121_SPI_HOST, &dev_cfg, &s_spi_dev);
            if (status == ESP_OK)
            {
                lr1121_spi_apply_miso_pullup();
            }
        }

        if (status == ESP_OK)
        {
            lr1121_version_info_t version_info;

            lr1121_hardware_reset();

            if (lr1121_wait_busy_ready(LR1121_RESET_SETTLE_MS))
            {
                if (lr1121_read_version(&version_info))
                {
                    s_version = lr1121_pack_version(&version_info);
                    s_state   = LR1121_RADIO_STATE_OK;
                    ESP_LOGI(TAG,
                             "LR1121 OK hw=0x%02X uc=0x%02X fw=%u.%u",
                             (unsigned int)version_info.hw,
                             (unsigned int)version_info.use_case,
                             (unsigned int)version_info.fw_major,
                             (unsigned int)version_info.fw_minor);
                }
                else
                {
                    s_state = LR1121_RADIO_STATE_FAULT;
                    status  = ESP_FAIL;
                }
            }
            else
            {
                s_state = LR1121_RADIO_STATE_FAULT;
                ESP_LOGE(TAG, "LR1121 BUSY timeout after reset");
                status  = ESP_FAIL;
            }
        }
    }

    return status;
}

lr1121_radio_state_t Lr1121_Radio_Get_State(void)
{
    return s_state;
}

bool Lr1121_Radio_Is_Ready(void)
{
    return (s_state == LR1121_RADIO_STATE_OK);
}

esp_err_t Lr1121_Radio_Format_Status(char *out_buf, size_t out_capacity)
{
    esp_err_t status = ESP_OK;

    if ((out_buf == NULL) || (out_capacity == 0U))
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else if (s_state == LR1121_RADIO_STATE_OK)
    {
        const uint8_t hw       = (uint8_t)((s_version >> 24) & 0xFFU);
        const uint8_t use_case = (uint8_t)((s_version >> 16) & 0xFFU);
        const uint8_t fw_major = (uint8_t)((s_version >> 8) & 0xFFU);
        const uint8_t fw_minor = (uint8_t)(s_version & 0xFFU);

        (void)snprintf(out_buf,
                       out_capacity,
                       "lr1121=ok hw=%02X uc=%02X fw=%u.%u",
                       (unsigned int)hw,
                       (unsigned int)use_case,
                       (unsigned int)fw_major,
                       (unsigned int)fw_minor);
    }
    else if (s_state == LR1121_RADIO_STATE_FAULT)
    {
        (void)snprintf(out_buf, out_capacity, "lr1121=fault");
    }
    else
    {
        (void)snprintf(out_buf, out_capacity, "lr1121=not_init");
    }

    return status;
}
