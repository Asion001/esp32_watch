# Collect LVGL demo sources (for reference/testing)
set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

# Collect all app sources from apps directory (recursively)
file(GLOB_RECURSE APP_SOURCES "apps/*.c")

# Exclude WiFi UI screens if WiFi is disabled
if(NOT CONFIG_ENABLE_WIFI)
    list(FILTER APP_SOURCES EXCLUDE REGEX "apps/settings/screens/wifi_.*\\.c$")
endif()

# Build list of required components
# Note: wifi_manager and ota_manager are always included in REQUIRES
# because CMake processes component dependencies before CONFIG_ variables
# are available. The actual WiFi/OTA code is conditionally compiled via
# preprocessor guards (#ifdef CONFIG_ENABLE_WIFI / CONFIG_ENABLE_OTA).
set(COMPONENT_REQUIRES 
    pcf85063_rtc 
    axp2101_pmu 
    sleep_manager 
    uptime_tracker 
    settings_storage 
    screen_manager 
    button_handler
    wifi_manager
    ota_manager
)

idf_component_register(
    SRCS main.c ${APP_SOURCES} ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS . apps apps/watchface apps/settings apps/settings/screens ${LV_DEMO_DIR}
    REQUIRES ${COMPONENT_REQUIRES})

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
