/**
 * @file bsp_display.c
 * @brief ST7701S (MIPI-DSI) + GT911 touch + LVGL bring-up for JC4880P443C.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#include "bsp_display.h"
#include "bsp_pins.h"
#include "expand_io_pins.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"

#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"

#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "bsp_display";

/* --- MIPI-DSI PHY power (LDO) ------------------------------------------------ */
#define BSP_MIPI_DSI_PHY_LDO_CHANNEL    (3)
#define BSP_MIPI_DSI_PHY_LDO_VOLTAGE_MV (2500)

/* --- DSI link parameters (from the JC4880P443C panel) ----------------------- */
#define BSP_DSI_LANE_COUNT              (2U)
#define BSP_DSI_LANE_BITRATE_MBPS       (500U)
#define BSP_DPI_CLOCK_MHZ               (34U)

/* --- DPI video timing (porches) --------------------------------------------- */
#define BSP_DPI_HSYNC_PULSE_WIDTH       (12U)
#define BSP_DPI_HSYNC_BACK_PORCH        (42U)
#define BSP_DPI_HSYNC_FRONT_PORCH       (42U)
#define BSP_DPI_VSYNC_PULSE_WIDTH       (2U)
#define BSP_DPI_VSYNC_BACK_PORCH        (8U)
#define BSP_DPI_VSYNC_FRONT_PORCH       (166U)

/* --- LCD pixel format -------------------------------------------------------- */
#define BSP_LCD_BITS_PER_PIXEL          (16U)

/* --- Backlight PWM ----------------------------------------------------------- */
#define BSP_BL_LEDC_TIMER               (LEDC_TIMER_0)
#define BSP_BL_LEDC_CHANNEL             (LEDC_CHANNEL_0)
#define BSP_BL_LEDC_MODE                (LEDC_LOW_SPEED_MODE)
#define BSP_BL_LEDC_FREQ_HZ             (5000U)
#define BSP_BL_LEDC_DUTY_RES            (LEDC_TIMER_10_BIT)
#define BSP_BL_LEDC_DUTY_MAX            (1023U)
#define BSP_BL_PERCENT_MAX              (100U)
#define BSP_BL_DEFAULT_PERCENT          (80U)

/* --- Touch I2C --------------------------------------------------------------- */
#define BSP_TOUCH_I2C_PORT              (I2C_NUM_0)
#define BSP_TOUCH_I2C_GLITCH_CNT        (7U)

/* --- LVGL ports -------------------------------------------------------------- */
#define BSP_LVGL_TASK_STACK             (6144U)
#define BSP_LVGL_TASK_PRIORITY          (4U)
#define BSP_LVGL_TIMER_PERIOD_MS        (5U)
#define BSP_LVGL_MAX_SLEEP_MS           (500U)
#define BSP_LVGL_DRAW_BUFFER_LINES      (200U)

/* --- Persistent handles ------------------------------------------------------ */
static esp_ldo_channel_handle_t   s_ldo_phy   = NULL;
static esp_lcd_dsi_bus_handle_t   s_dsi_bus   = NULL;
static esp_lcd_panel_io_handle_t  s_dbi_io    = NULL;
static esp_lcd_panel_handle_t     s_panel     = NULL;
static i2c_master_bus_handle_t    s_i2c_bus   = NULL;
static esp_lcd_panel_io_handle_t  s_tp_io     = NULL;
static esp_lcd_touch_handle_t     s_touch     = NULL;
static lv_display_t              *s_lv_disp   = NULL;

/*
 * ST7701S initialization sequence for the JC4880P443C panel.
 *
 * MISRA DEVIATION (Rule 9.x / compound literals): the vendor register dump is
 * expressed with file-scope compound literals to keep the table compact and
 * verifiable against the panel datasheet / vendor firmware. When a custom
 * init_cmds array is supplied, esp_lcd_st7701 sends it verbatim and does NOT
 * append sleep-out/display-on, so the trailing 0x11 (SLPOUT, 120 ms) and 0x29
 * (DISPON) commands MUST be included here or the panel stays asleep (black).
 */
static const st7701_lcd_init_cmd_t s_st7701_init_cmds[] =
{
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t[]){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x58}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},  /* Sleep Out, wait 120 ms */
    {0x29, (uint8_t[]){0x00}, 0, 0},    /* Display On */
};

/**
 * @brief Configure the backlight PWM timer/channel (starts fully off).
 */
static void bsp_backlight_init(void)
{
    const ledc_timer_config_t timer_cfg =
    {
        .speed_mode      = BSP_BL_LEDC_MODE,
        .duty_resolution = BSP_BL_LEDC_DUTY_RES,
        .timer_num       = BSP_BL_LEDC_TIMER,
        .freq_hz         = BSP_BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t channel_cfg =
    {
        .gpio_num   = BSP_PIN_LCD_BACKLIGHT,
        .speed_mode = BSP_BL_LEDC_MODE,
        .channel    = BSP_BL_LEDC_CHANNEL,
        .timer_sel  = BSP_BL_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

/**
 * @brief Power the MIPI D-PHY and create the DSI bus + DBI control IO.
 */
static void bsp_dsi_bus_init(void)
{
    const esp_ldo_channel_config_t ldo_cfg =
    {
        .chan_id    = BSP_MIPI_DSI_PHY_LDO_CHANNEL,
        .voltage_mv = BSP_MIPI_DSI_PHY_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &s_ldo_phy));

    const esp_lcd_dsi_bus_config_t bus_cfg =
    {
        .bus_id             = 0,
        .num_data_lanes     = BSP_DSI_LANE_COUNT,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = BSP_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &s_dsi_bus));

    const esp_lcd_dbi_io_config_t dbi_cfg =
    {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_cfg, &s_dbi_io));
}

/**
 * @brief Create the ST7701S panel over DSI and turn the display on.
 */
static void bsp_panel_init(void)
{
    const esp_lcd_dpi_panel_config_t dpi_cfg =
    {
        .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = BSP_DPI_CLOCK_MHZ,
        .virtual_channel    = 0,
        .pixel_format       = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs            = 1,
        .video_timing =
        {
            .h_size            = BSP_LCD_H_RES,
            .v_size            = BSP_LCD_V_RES,
            .hsync_back_porch  = BSP_DPI_HSYNC_BACK_PORCH,
            .hsync_pulse_width = BSP_DPI_HSYNC_PULSE_WIDTH,
            .hsync_front_porch = BSP_DPI_HSYNC_FRONT_PORCH,
            .vsync_back_porch  = BSP_DPI_VSYNC_BACK_PORCH,
            .vsync_pulse_width = BSP_DPI_VSYNC_PULSE_WIDTH,
            .vsync_front_porch = BSP_DPI_VSYNC_FRONT_PORCH,
        },
        .flags.use_dma2d = true,
    };

    st7701_vendor_config_t vendor_cfg =
    {
        .init_cmds      = s_st7701_init_cmds,
        .init_cmds_size = (uint16_t)(sizeof(s_st7701_init_cmds) / sizeof(s_st7701_init_cmds[0])),
        .mipi_config =
        {
            .dsi_bus    = s_dsi_bus,
            .dpi_config = &dpi_cfg,
        },
        .flags.use_mipi_interface = 1,
    };

    const esp_lcd_panel_dev_config_t panel_cfg =
    {
        .reset_gpio_num = BSP_PIN_LCD_RESET,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config  = &vendor_cfg,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(s_dbi_io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
}

/**
 * @brief Bring up the GT911 touch controller on the shared I2C bus.
 */
static void bsp_touch_init(void)
{
    const i2c_master_bus_config_t i2c_cfg =
    {
        .i2c_port                     = BSP_TOUCH_I2C_PORT,
        .sda_io_num                   = EXPAND_IO_PIN_I2C_SDA,
        .scl_io_num                   = EXPAND_IO_PIN_I2C_SCL,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = BSP_TOUCH_I2C_GLITCH_CNT,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &s_i2c_bus));

    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &s_tp_io));

    const esp_lcd_touch_config_t tp_cfg =
    {
        .x_max        = BSP_LCD_H_RES,
        .y_max        = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_TOUCH_GPIO_RST,
        .int_gpio_num = BSP_TOUCH_GPIO_INT,
        .levels =
        {
            .reset     = 0,
            .interrupt = 0,
        },
        .flags =
        {
            .swap_xy  = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(s_tp_io, &tp_cfg, &s_touch));
}

/**
 * @brief Start LVGL (esp_lvgl_port) and attach the display + touch input.
 */
static void bsp_lvgl_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg =
    {
        .task_priority     = BSP_LVGL_TASK_PRIORITY,
        .task_stack        = BSP_LVGL_TASK_STACK,
        .task_affinity     = -1,
        .task_max_sleep_ms = BSP_LVGL_MAX_SLEEP_MS,
        .timer_period_ms   = BSP_LVGL_TIMER_PERIOD_MS,
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg =
    {
        .io_handle     = s_dbi_io,
        .panel_handle  = s_panel,
        .buffer_size   = (uint32_t)BSP_LCD_H_RES * BSP_LVGL_DRAW_BUFFER_LINES,
        .double_buffer = true,
        .hres          = BSP_LCD_H_RES,
        .vres          = BSP_LCD_V_RES,
        .monochrome    = false,
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .rotation =
        {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags =
        {
            .buff_dma    = false,
            .buff_spiram = true,
            .swap_bytes  = false,
        },
    };

    const lvgl_port_display_dsi_cfg_t dsi_cfg =
    {
        .flags.avoid_tearing = false,
    };

    s_lv_disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    ESP_ERROR_CHECK((s_lv_disp != NULL) ? ESP_OK : ESP_FAIL);

    const lvgl_port_touch_cfg_t touch_cfg =
    {
        .disp   = s_lv_disp,
        .handle = s_touch,
    };
    ESP_ERROR_CHECK((lvgl_port_add_touch(&touch_cfg) != NULL) ? ESP_OK : ESP_FAIL);
}

void Bsp_Display_Set_Backlight(uint8_t percent)
{
    uint32_t clamped = (percent > BSP_BL_PERCENT_MAX) ? BSP_BL_PERCENT_MAX : (uint32_t)percent;
    uint32_t duty    = (clamped * BSP_BL_LEDC_DUTY_MAX) / BSP_BL_PERCENT_MAX;

    (void)ledc_set_duty(BSP_BL_LEDC_MODE, BSP_BL_LEDC_CHANNEL, duty);
    (void)ledc_update_duty(BSP_BL_LEDC_MODE, BSP_BL_LEDC_CHANNEL);
}

esp_err_t Bsp_Display_Init(void)
{
    ESP_LOGI(TAG, "initializing display stack (ST7701S %ux%u)", BSP_LCD_H_RES, BSP_LCD_V_RES);

    bsp_backlight_init();
    bsp_dsi_bus_init();
    bsp_panel_init();
    bsp_touch_init();
    bsp_lvgl_init();

    Bsp_Display_Set_Backlight(BSP_BL_DEFAULT_PERCENT);

    ESP_LOGI(TAG, "display stack ready");
    return ESP_OK;
}
