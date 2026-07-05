# LVGL chat UI. Depends on the chat engine and the LVGL port (pulled by bsp).
idf_component_register(
    SRCS "chat_ui.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        chat_core
        esp_lvgl_port
)
