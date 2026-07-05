/**
 * @file app_main.c
 * @brief ESPDeka LORA coprocessor entry point (ESP32-C3 + LR1121).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "board_pins.h"
#include "host_uart.h"
#include "at_shell.h"
#include "status_led.h"
#include "lr1121_radio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "lora_copro";

#define TELEMETRY_TASK_STACK_WORDS      (3072U)
#define TELEMETRY_TASK_PRIORITY         (3U)
#define TELEMETRY_INTERVAL_MS           (1000U)
#define BOOT_LED_DURATION_MS            (1500U)

static StackType_t  s_telemetry_task_stack[TELEMETRY_TASK_STACK_WORDS];
static StaticTask_t s_telemetry_task_tcb;

/**
 * @brief Forward host UART lines to the AT shell.
 */
static void copro_on_host_line(const char *line, void *user_ctx)
{
    (void)user_ctx;
    AtShell_Handle_Line(line);
}

/**
 * @brief Emit telemetry lines once per second while streaming is enabled.
 */
static void telemetry_task(void *arg)
{
    char line[AT_SHELL_TELEMETRY_CAPACITY];

    (void)arg;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_INTERVAL_MS));

        if (AtShell_Is_Stream_Enabled())
        {
            if (AtShell_Format_Telemetry(line, sizeof(line)) == ESP_OK)
            {
                (void)HostUart_Write(line);
                (void)HostUart_Write("\r\n");
                ESP_LOGI(TAG, "telemetry: %s", line);
            }
        }
    }
}

void app_main(void)
{
    bool radio_ok = false;

    ESP_LOGI(TAG, "starting %s on ESP32-C3 (P-3 V1.6)", BOARD_FW_VERSION);

    ESP_ERROR_CHECK(StatusLed_Init());
    (void)StatusLed_Set_Mode(STATUS_LED_MODE_BOOTING);

    ESP_ERROR_CHECK(AtShell_Init());
    ESP_ERROR_CHECK(HostUart_Init());
    ESP_ERROR_CHECK(HostUart_Start(copro_on_host_line, NULL));

    if (Lr1121_Radio_Init() == ESP_OK)
    {
        radio_ok = Lr1121_Radio_Is_Ready();
    }

    AtShell_Notify_Radio_Probe(radio_ok);
    vTaskDelay(pdMS_TO_TICKS(BOOT_LED_DURATION_MS));

    if (xTaskCreateStatic(telemetry_task,
                          "telemetry",
                          TELEMETRY_TASK_STACK_WORDS,
                          NULL,
                          TELEMETRY_TASK_PRIORITY,
                          s_telemetry_task_stack,
                          &s_telemetry_task_tcb) == NULL)
    {
        ESP_LOGE(TAG, "failed to start telemetry task");
    }

    (void)HostUart_Write(BOARD_FW_VERSION);
    (void)HostUart_Write(" ready\r\n");
    ESP_LOGI(TAG, "coprocessor ready — send AT+HELP");
}
