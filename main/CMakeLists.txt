# Collect LVGL demo sources (for reference/testing)
set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

# Collect all app sources from apps directory
file(GLOB_RECURSE APP_SOURCES "apps/*/*.c")

idf_component_register(
    SRCS main.c ${APP_SOURCES} ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS . apps apps/watchface ${LV_DEMO_DIR}
    REQUIRES pcf85063_rtc axp2101_pmu sleep_manager)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
