idf_component_register(
    SRCS "button_handler.c"
    INCLUDE_DIRS "."
    REQUIRES driver screen_manager esp32_c6_touch_amoled_2_06
)
