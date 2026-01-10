idf_component_register(
    SRCS "sleep_manager.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer lvgl__lvgl waveshare__esp32_c6_touch_amoled_2_06
)
