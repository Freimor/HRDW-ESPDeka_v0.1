idf_component_register(
    SRCS
        "app_main.c"
        "host_uart.c"
        "at_shell.c"
    INCLUDE_DIRS "."
    REQUIRES
        board_p3_v16
        status_led
        lr1121_radio
        esp_timer
        driver
)
