# GNSS RUSHFPV M10 PRO: UART NMEA + I2C compass.
idf_component_register(
    SRCS "gnss_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES expand_io esp_driver_i2c esp_driver_uart
)
