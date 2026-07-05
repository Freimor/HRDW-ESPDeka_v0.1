/**
 * @file at_shell.c
 * @brief Full AT command parser for the LORA coprocessor firmware.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "at_shell.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "board_pins.h"
#include "host_uart.h"
#include "status_led.h"
#include "lr1121_radio.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AT_SHELL_MS_PER_SECOND          (1000U)
#define AT_SHELL_RST_DELAY_MS           (200U)
#define AT_SHELL_LED_MODE_AUTO          (1U)
#define AT_SHELL_LED_MODE_TEST          (2U)

static const char *TAG = "at_shell";

static bool     s_stream_enabled = false;
static bool     s_radio_ok       = false;
static bool     s_led_manual     = false;
static uint32_t s_boot_time_ms   = 0U;

/**
 * @brief Case-insensitive compare of @p lhs and @p rhs.
 */
static bool at_shell_str_ieq(const char *lhs, const char *rhs)
{
    bool equal = false;

    if ((lhs == NULL) || (rhs == NULL))
    {
        equal = false;
    }
    else
    {
        size_t idx = 0U;
        equal = true;

        while ((lhs[idx] != '\0') || (rhs[idx] != '\0'))
        {
            const int left_ch  = (int)tolower((unsigned char)lhs[idx]);
            const int right_ch = (int)tolower((unsigned char)rhs[idx]);

            if (left_ch != right_ch)
            {
                equal = false;
                break;
            }
            idx++;
        }
    }

    return equal;
}

/**
 * @brief Send a line followed by CRLF on the host UART.
 */
static void at_shell_reply(const char *line)
{
    if (line != NULL)
    {
        (void)HostUart_Write(line);
        (void)HostUart_Write("\r\n");
    }
}

/**
 * @brief Apply automatic LED mode from stream/radio state (when not overridden).
 */
static void at_shell_apply_auto_led(void)
{
    if (!s_led_manual)
    {
        if (!s_radio_ok)
        {
            (void)StatusLed_Set_Mode(STATUS_LED_MODE_RADIO_ERR);
        }
        else if (s_stream_enabled)
        {
            (void)StatusLed_Set_Mode(STATUS_LED_MODE_STREAMING);
        }
        else
        {
            (void)StatusLed_Set_Mode(STATUS_LED_MODE_READY);
        }
    }
}

/**
 * @brief Handle AT+STREAM=0|1 and legacy /start /stop aliases.
 */
static void at_shell_handle_stream_command(const char *line)
{
    const char *value_ptr = NULL;
    bool        enable    = false;

    if (line == NULL)
    {
        /* No-op. */
    }
    else if (at_shell_str_ieq(line, "/start") || at_shell_str_ieq(line, "AT+STREAM=1"))
    {
        s_stream_enabled = true;
        at_shell_apply_auto_led();
        StatusLed_Pulse_Ack();
        at_shell_reply("OK");
        ESP_LOGI(TAG, "telemetry stream enabled");
    }
    else if (at_shell_str_ieq(line, "/stop") || at_shell_str_ieq(line, "AT+STREAM=0"))
    {
        s_stream_enabled = false;
        at_shell_apply_auto_led();
        StatusLed_Pulse_Ack();
        at_shell_reply("OK");
        ESP_LOGI(TAG, "telemetry stream disabled");
    }
    else if (strstr(line, "AT+STREAM=") == line)
    {
        value_ptr = line + strlen("AT+STREAM=");
        enable    = (value_ptr[0] == '1');

        if ((value_ptr[0] == '0') || enable)
        {
            s_stream_enabled = enable;
            at_shell_apply_auto_led();
            StatusLed_Pulse_Ack();
            at_shell_reply("OK");
        }
        else
        {
            at_shell_reply("ERROR");
        }
    }
    else
    {
        at_shell_reply("ERROR");
    }
}

/**
 * @brief Handle AT+LED=0|1|2 and AT+LED? queries.
 */
static void at_shell_handle_led_command(const char *line)
{
    uint32_t mode = 0U;

    if (at_shell_str_ieq(line, "AT+LED?"))
    {
        const status_led_mode_t mode_now = StatusLed_Get_Mode();
        char                  reply[AT_SHELL_RESPONSE_CAPACITY];

        (void)snprintf(reply,
                       sizeof(reply),
                       "+LED:%u,manual:%u",
                       (unsigned int)mode_now,
                       s_led_manual ? 1U : 0U);
        at_shell_reply(reply);
    }
    else if (strstr(line, "AT+LED=") == line)
    {
        mode = (uint32_t)strtoul(line + strlen("AT+LED="), NULL, 10);

        if (mode == 0U)
        {
            s_led_manual = true;
            (void)StatusLed_Set_Mode(STATUS_LED_MODE_OFF);
            at_shell_reply("OK");
        }
        else if (mode == AT_SHELL_LED_MODE_AUTO)
        {
            s_led_manual = false;
            at_shell_apply_auto_led();
            at_shell_reply("OK");
        }
        else if (mode == AT_SHELL_LED_MODE_TEST)
        {
            s_led_manual = true;
            (void)StatusLed_Set_Mode(STATUS_LED_MODE_BOOTING);
            at_shell_reply("OK");
        }
        else
        {
            at_shell_reply("ERROR");
        }
    }
    else
    {
        at_shell_reply("ERROR");
    }
}

/**
 * @brief Print supported AT commands (AT+HELP).
 */
static void at_shell_reply_help(void)
{
    at_shell_reply("+HELP:AT");
    at_shell_reply("+HELP:AT+GMR");
    at_shell_reply("+HELP:AT+HELP");
    at_shell_reply("+HELP:AT+RST");
    at_shell_reply("+HELP:AT+STATUS?");
    at_shell_reply("+HELP:AT+STREAM=0|1");
    at_shell_reply("+HELP:AT+UART?");
    at_shell_reply("+HELP:AT+LR1121?");
    at_shell_reply("+HELP:AT+LED=0|1|2");
    at_shell_reply("+HELP:AT+LED?");
    at_shell_reply("OK");
}

/**
 * @brief Describe the active host UART (AT+UART?).
 */
static void at_shell_reply_uart_info(void)
{
    char reply[AT_SHELL_RESPONSE_CAPACITY];

#if CONFIG_ESPDEKA_HOST_UART_PORT_1
    (void)snprintf(reply,
                   sizeof(reply),
                   "+UART:1,baud:%u,tx:%d,rx:%d",
                   (unsigned int)BOARD_HOST_UART_BAUD,
                   (int)BOARD_HOST_UART1_TX_PIN,
                   (int)BOARD_HOST_UART1_RX_PIN);
#else
    (void)snprintf(reply,
                   sizeof(reply),
                   "+UART:0,baud:%u,tx:%d,rx:%d",
                   (unsigned int)BOARD_HOST_UART_BAUD,
                   (int)BOARD_HOST_UART_TX_PIN,
                   (int)BOARD_HOST_UART_RX_PIN);
#endif
    at_shell_reply(reply);
}

esp_err_t AtShell_Init(void)
{
    s_stream_enabled = false;
    s_radio_ok       = false;
    s_led_manual     = false;
    s_boot_time_ms   = (uint32_t)(esp_timer_get_time() / (int64_t)AT_SHELL_MS_PER_SECOND);
    ESP_LOGI(TAG, "AT shell ready (%s)", BOARD_FW_VERSION);
    return ESP_OK;
}

void AtShell_Notify_Radio_Probe(bool radio_ok)
{
    s_radio_ok = radio_ok;
    at_shell_apply_auto_led();
}

void AtShell_Handle_Line(const char *line)
{
    if ((line == NULL) || (line[0] == '\0'))
    {
        /* Ignore empty lines. */
    }
    else if (at_shell_str_ieq(line, "AT"))
    {
        StatusLed_Pulse_Ack();
        at_shell_reply("OK");
    }
    else if (at_shell_str_ieq(line, "AT+GMR"))
    {
        at_shell_reply(BOARD_FW_VERSION);
    }
    else if (at_shell_str_ieq(line, "AT+HELP"))
    {
        at_shell_reply_help();
    }
    else if (at_shell_str_ieq(line, "AT+RST"))
    {
        at_shell_reply("OK");
        vTaskDelay(pdMS_TO_TICKS(AT_SHELL_RST_DELAY_MS));
        esp_restart();
    }
    else if (at_shell_str_ieq(line, "AT+STATUS?"))
    {
        char status_line[AT_SHELL_TELEMETRY_CAPACITY];

        if (AtShell_Format_Telemetry(status_line, sizeof(status_line)) == ESP_OK)
        {
            at_shell_reply(status_line);
        }
        else
        {
            at_shell_reply("ERROR");
        }
    }
    else if (at_shell_str_ieq(line, "AT+UART?"))
    {
        at_shell_reply_uart_info();
    }
    else if (at_shell_str_ieq(line, "AT+LR1121?"))
    {
        char radio_line[LR1121_RADIO_STATUS_CAPACITY];

        if (Lr1121_Radio_Format_Status(radio_line, sizeof(radio_line)) == ESP_OK)
        {
            at_shell_reply(radio_line);
        }
        else
        {
            at_shell_reply("ERROR");
        }
    }
    else if ((strstr(line, "AT+LED=") == line) || at_shell_str_ieq(line, "AT+LED?"))
    {
        at_shell_handle_led_command(line);
    }
    else if ((strstr(line, "AT+STREAM=") == line)
             || at_shell_str_ieq(line, "/start")
             || at_shell_str_ieq(line, "/stop"))
    {
        at_shell_handle_stream_command(line);
    }
    else
    {
        ESP_LOGW(TAG, "unknown command: %s", line);
        at_shell_reply("ERROR");
    }
}

bool AtShell_Is_Stream_Enabled(void)
{
    return s_stream_enabled;
}

esp_err_t AtShell_Format_Telemetry(char *out_buf, size_t out_capacity)
{
    esp_err_t status = ESP_OK;
    uint32_t  uptime_s = 0U;
    char      radio_part[LR1121_RADIO_STATUS_CAPACITY];

    if ((out_buf == NULL) || (out_capacity == 0U))
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / (int64_t)AT_SHELL_MS_PER_SECOND);

        if (now_ms >= s_boot_time_ms)
        {
            uptime_s = (now_ms - s_boot_time_ms) / AT_SHELL_MS_PER_SECOND;
        }

        (void)memset(radio_part, 0, sizeof(radio_part));
        (void)Lr1121_Radio_Format_Status(radio_part, sizeof(radio_part));

        (void)snprintf(out_buf,
                       out_capacity,
                       "LORA: %s uart=ok %s stream=%u uptime=%lus",
                       BOARD_FW_VERSION,
                       radio_part,
                       s_stream_enabled ? 1U : 0U,
                       (unsigned long)uptime_s);
    }

    return status;
}
