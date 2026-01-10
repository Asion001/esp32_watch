#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "apps/watchface/watchface.h"
#include "sleep_manager.h"

static const char *TAG = "Main";

/**
 * @brief Sleep check task - monitors inactivity and triggers sleep
 */
static void sleep_check_task(void *pvParameters)
{
#ifdef CONFIG_SLEEP_MANAGER_ENABLE
    ESP_LOGI(TAG, "Sleep check task started");

    while (1)
    {
        // Check every 500ms if we should enter sleep
        vTaskDelay(pdMS_TO_TICKS(500));

        if (sleep_manager_should_sleep())
        {
            ESP_LOGI(TAG, "Inactivity timeout - entering sleep mode");

            // Enter sleep (blocks until wake-up)
            esp_err_t ret = sleep_manager_sleep();

            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Sleep failed: %s", esp_err_to_name(ret));
            }
        }
    }
#else
    ESP_LOGI(TAG, "Sleep manager disabled - task exiting");
    vTaskDelete(NULL);
#endif
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-C6 Watch Firmware");

    // Start display subsystem
    ESP_LOGI(TAG, "Initializing display...");
    bsp_display_start();

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
    // Initialize sleep manager
    ESP_LOGI(TAG, "Initializing sleep manager...");
    esp_err_t ret = sleep_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize sleep manager: %s", esp_err_to_name(ret));
    }
#else
    ESP_LOGI(TAG, "Sleep manager disabled in configuration");
#endif

    // Lock LVGL for UI creation
    bsp_display_lock(0);

    // Create watchface on active screen
    watchface_create(lv_screen_active());

    // Unlock LVGL
    bsp_display_unlock();

    ESP_LOGI(TAG, "Watch initialized successfully");

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
    // Create sleep monitoring task
    xTaskCreate(sleep_check_task, "sleep_check", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Sleep monitoring task created");
#endif
}