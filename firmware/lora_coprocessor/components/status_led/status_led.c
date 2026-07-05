/**
 * @file status_led.c
 * @brief WS2812 RGB patterns on GPIO8 (GRB order, ExpressLRS-compatible).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "status_led.h"

#include "board_pins.h"

#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define STATUS_LED_COUNT                (1U)
#define STATUS_LED_TASK_STACK_WORDS     (2048U)
#define STATUS_LED_TASK_PRIORITY        (2U)
#define STATUS_LED_TICK_MS              (50U)
#define STATUS_LED_READY_PERIOD_MS      (500U)
#define STATUS_LED_STREAM_PERIOD_MS     (250U)
#define STATUS_LED_RADIO_ERR_PERIOD_MS  (100U)
#define STATUS_LED_ACK_DURATION_MS      (80U)
#define STATUS_LED_BOOT_CYCLES          (40U)
#define STATUS_LED_COLOR_MAX            (255U)
#define STATUS_LED_COLOR_OFF            (0U)
#define STATUS_LED_HUE_WRAP             (360U)

static const char *TAG = "status_led";

static led_strip_handle_t   s_strip          = NULL;
static status_led_mode_t    s_mode           = STATUS_LED_MODE_BOOTING;
static bool                 s_ack_active     = false;
static uint32_t             s_tick_count     = 0U;
static StackType_t          s_task_stack[STATUS_LED_TASK_STACK_WORDS];
static StaticTask_t         s_task_tcb;
static bool                 s_initialized    = false;

/**
 * @brief Push RGB values to the single WS2812 (GRB wire order).
 */
static void status_led_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_strip != NULL)
    {
        (void)led_strip_set_pixel(s_strip, 0U, red, green, blue);
        (void)led_strip_refresh(s_strip);
    }
}

/**
 * @brief Convert a boot-time hue angle to RGB (simple HSV slice).
 */
static void status_led_hue_to_rgb(uint16_t hue, uint8_t *out_red, uint8_t *out_green, uint8_t *out_blue)
{
    const uint16_t sector = (uint16_t)(hue % STATUS_LED_HUE_WRAP);
    const uint8_t  value  = STATUS_LED_COLOR_MAX;

    if (sector < 120U)
    {
        *out_red   = (uint8_t)(((120U - sector) * (uint32_t)value) / 120U);
        *out_green = (uint8_t)((sector * (uint32_t)value) / 120U);
        *out_blue  = STATUS_LED_COLOR_OFF;
    }
    else if (sector < 240U)
    {
        const uint16_t sub = (uint16_t)(sector - 120U);
        *out_red   = STATUS_LED_COLOR_OFF;
        *out_green = (uint8_t)(((120U - sub) * (uint32_t)value) / 120U);
        *out_blue  = (uint8_t)((sub * (uint32_t)value) / 120U);
    }
    else
    {
        const uint16_t sub = (uint16_t)(sector - 240U);
        *out_red   = (uint8_t)((sub * (uint32_t)value) / 120U);
        *out_green = STATUS_LED_COLOR_OFF;
        *out_blue  = (uint8_t)(((120U - sub) * (uint32_t)value) / 120U);
    }
}

/**
 * @brief Render the pattern for the current mode and tick counter.
 */
static void status_led_render_frame(void)
{
    uint8_t red   = STATUS_LED_COLOR_OFF;
    uint8_t green = STATUS_LED_COLOR_OFF;
    uint8_t blue  = STATUS_LED_COLOR_OFF;
    bool    on    = false;

    if (s_ack_active)
    {
        status_led_set_rgb(STATUS_LED_COLOR_MAX, STATUS_LED_COLOR_MAX, STATUS_LED_COLOR_MAX);
        return;
    }

    switch (s_mode)
    {
        case STATUS_LED_MODE_BOOTING:
            status_led_hue_to_rgb((uint16_t)((s_tick_count * 9U) % STATUS_LED_HUE_WRAP), &red, &green, &blue);
            break;

        case STATUS_LED_MODE_READY:
            on = ((((s_tick_count * STATUS_LED_TICK_MS) / STATUS_LED_READY_PERIOD_MS) % 2U) == 0U);
            green = on ? STATUS_LED_COLOR_MAX : STATUS_LED_COLOR_OFF;
            break;

        case STATUS_LED_MODE_STREAMING:
            on = ((((s_tick_count * STATUS_LED_TICK_MS) / STATUS_LED_STREAM_PERIOD_MS) % 2U) == 0U);
            if (on)
            {
                green = STATUS_LED_COLOR_MAX;
                blue  = STATUS_LED_COLOR_MAX;
            }
            break;

        case STATUS_LED_MODE_RADIO_ERR:
            on = ((((s_tick_count * STATUS_LED_TICK_MS) / STATUS_LED_RADIO_ERR_PERIOD_MS) % 2U) == 0U);
            red = on ? STATUS_LED_COLOR_MAX : STATUS_LED_COLOR_OFF;
            break;

        case STATUS_LED_MODE_OFF:
        default:
            red   = STATUS_LED_COLOR_OFF;
            green = STATUS_LED_COLOR_OFF;
            blue  = STATUS_LED_COLOR_OFF;
            break;
    }

    status_led_set_rgb(red, green, blue);
}

/**
 * @brief LED pattern task (static stack, periodic refresh).
 */
static void status_led_task(void *arg)
{
    uint32_t ack_ticks = 0U;

    (void)arg;

    for (;;)
    {
        if (s_ack_active)
        {
            ack_ticks++;
            if (ack_ticks >= (STATUS_LED_ACK_DURATION_MS / STATUS_LED_TICK_MS))
            {
                s_ack_active = false;
                ack_ticks    = 0U;
            }
        }

        status_led_render_frame();
        s_tick_count++;
        vTaskDelay(pdMS_TO_TICKS(STATUS_LED_TICK_MS));
    }
}

esp_err_t StatusLed_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        const led_strip_config_t strip_cfg =
        {
            .strip_gpio_num    = BOARD_LED_RGB_PIN,
            .max_leds          = STATUS_LED_COUNT,
            .led_pixel_format  = LED_PIXEL_FORMAT_GRB,
            .led_model         = LED_MODEL_WS2812,
            .flags.invert_out  = false,
        };

        const led_strip_rmt_config_t rmt_cfg =
        {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10000000U,
            .mem_block_symbols = 0U,
            .flags.with_dma = false,
        };

        status = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);

        if (status == ESP_OK)
        {
            (void)led_strip_clear(s_strip);

            if (xTaskCreateStatic(status_led_task,
                                  "status_led",
                                  STATUS_LED_TASK_STACK_WORDS,
                                  NULL,
                                  STATUS_LED_TASK_PRIORITY,
                                  s_task_stack,
                                  &s_task_tcb) == NULL)
            {
                status = ESP_ERR_NO_MEM;
            }
            else
            {
                s_initialized = true;
                ESP_LOGI(TAG, "WS2812 status LED on GPIO%d", (int)BOARD_LED_RGB_PIN);
            }
        }
    }

    return status;
}

esp_err_t StatusLed_Set_Mode(status_led_mode_t mode)
{
    esp_err_t status = ESP_OK;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        s_mode = mode;
    }

    return status;
}

status_led_mode_t StatusLed_Get_Mode(void)
{
    return s_mode;
}

void StatusLed_Pulse_Ack(void)
{
    s_ack_active = true;
}
