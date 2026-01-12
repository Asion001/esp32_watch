# Collect LVGL demo sources (for reference/testing)
set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

# Collect all app sources from apps directory (recursively)
file(GLOB_RECURSE APP_SOURCES "apps/*.c")

# Exclude WiFi UI screens if WiFi is disabled
if(NOT CONFIG_ENABLE_WIFI)
    list(FILTER APP_SOURCES EXCLUDE REGEX "apps/settings/screens/wifi_.*\\.c$")
endif()

# Build list of required components based on configuration
set(COMPONENT_REQUIRES pcf85063_rtc axp2101_pmu sleep_manager uptime_tracker settings_storage screen_manager button_handler)

# Conditionally add WiFi manager
if(CONFIG_ENABLE_WIFI)
    list(APPEND COMPONENT_REQUIRES wifi_manager)
endif()

# Conditionally add OTA manager  
if(CONFIG_ENABLE_OTA)
    list(APPEND COMPONENT_REQUIRES ota_manager)
endif()

idf_component_register(
    SRCS main.c ${APP_SOURCES} ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS . apps apps/common apps/watchface apps/settings apps/settings/screens ${LV_DEMO_DIR}
    REQUIRES ${COMPONENT_REQUIRES})

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
