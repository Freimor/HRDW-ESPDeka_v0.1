# Board support package: display (ST7701S/DSI), touch (GT911) and LVGL glue.
idf_component_register(
    SRCS "bsp_display.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        driver
        esp_lcd
        esp_hw_support
        esp_lcd_st7701
        esp_lcd_touch_gt911
        esp_lvgl_port
        expand_io
)
