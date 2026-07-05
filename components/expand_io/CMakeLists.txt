# Expand IO HAL: centralized pin map + I2C/UART abstraction.
idf_component_register(
    SRCS "expand_io_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c esp_driver_uart
)
