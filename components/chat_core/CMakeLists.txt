# Chat engine: pure logic + FreeRTOS, no hardware dependencies.
idf_component_register(
    SRCS "chat_core.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
