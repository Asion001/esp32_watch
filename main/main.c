#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "apps/watchface/watchface.h"

static const char *TAG = "Main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-C6 Watch Firmware");

    // Start display subsystem
    ESP_LOGI(TAG, "Initializing display...");
    bsp_display_start();

    // Lock LVGL for UI creation
    bsp_display_lock(0);

    // Create watchface on active screen
    watchface_create(lv_screen_active());

    // Unlock LVGL
    bsp_display_unlock();

    ESP_LOGI(TAG, "Watch initialized successfully");
}