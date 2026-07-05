idf_component_register(
    SRCS "status_led.c"
    INCLUDE_DIRS "include"
    REQUIRES board_p3_v16 led_strip driver esp_timer
)
