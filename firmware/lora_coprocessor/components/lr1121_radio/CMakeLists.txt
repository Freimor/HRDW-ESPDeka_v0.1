idf_component_register(
    SRCS "lr1121_radio.c"
    INCLUDE_DIRS "include"
    REQUIRES board_p3_v16 driver esp_timer
)
