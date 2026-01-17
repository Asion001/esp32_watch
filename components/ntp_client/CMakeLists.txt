idf_component_register(
    SRCS "ntp_client.c"
    INCLUDE_DIRS "."
    REQUIRES lwip esp_netif settings_storage pcf85063_rtc
)
