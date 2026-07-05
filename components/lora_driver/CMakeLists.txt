# LR1121 LORA module over Expand IO UART.
idf_component_register(
    SRCS "lora_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES expand_io esp_driver_uart
)
