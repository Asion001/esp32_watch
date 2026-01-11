#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_system.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/gpio.h"
#include "screen_manager.h"
#include "apps/watchface/watchface.h"
#include "apps/settings/settings.h"
#include "apps/settings/screens/display_settings.h"
#include "apps/settings/screens/system_settings.h"
#include "apps/settings/screens/about_screen.h"
#include "apps/settings/screens/wifi_settings.h"
#include "apps/settings/screens/wifi_scan.h"
#include "apps/settings/screens/wifi_password.h"
#include "wifi_manager.h"
#include "sleep_manager.h"

static const char *TAG = "Main";

// WiFi status callback
static void wifi_status_callback(wifi_state_t state, void *user_data) {
    ESP_LOGI(TAG, "WiFi state changed: %d", state);
    wifi_settings_update_status();
}

// Button long press configuration
#define BUTTON_LONG_PRESS_MS 3000 // 3 seconds for reset

/**
 * @brief Button monitoring task for reset functionality
 */
static void button_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Button monitor task started");

    // Configure button GPIO as input with pull-up
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&button_conf);

    uint32_t press_start_ms = 0;
    bool was_pressed = false;

    while (1)
    {
        // Button is active-low (pressed = 0)
        bool is_pressed = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);

        if (is_pressed && !was_pressed)
        {
            // Button just pressed
            press_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            was_pressed = true;
            ESP_LOGD(TAG, "Button pressed");
        }
        else if (is_pressed && was_pressed)
        {
            // Button still pressed - check duration
            uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t press_duration = current_ms - press_start_ms;

            if (press_duration >= BUTTON_LONG_PRESS_MS)
            {
                ESP_LOGI(TAG, "Long press detected - resetting watch...");
                vTaskDelay(pdMS_TO_TICKS(100)); // Small delay for log to flush
                esp_restart();
            }
        }
        else if (!is_pressed && was_pressed)
        {
            // Button released
            was_pressed = false;
            ESP_LOGD(TAG, "Button released");
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
    }
}

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
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
#endif

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "Starting ESP32-C6 Watch Firmware");

    // Start display subsystem
    ESP_LOGI(TAG, "Initializing display...");
    bsp_display_start();

    // Initialize screen manager
    ESP_LOGI(TAG, "Initializing screen manager...");
    esp_err_t ret = screen_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize screen manager: %s", esp_err_to_name(ret));
    }

    // Initialize WiFi manager
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    ret = wifi_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(ret));
    }
    else
    {
        // Register WiFi status callback
        wifi_manager_register_callback(wifi_status_callback, NULL);
        
        // Auto-connect if credentials are saved
        if (wifi_manager_has_credentials())
        {
            ESP_LOGI(TAG, "Saved WiFi credentials found, attempting auto-connect...");
            wifi_manager_auto_connect();
        }
    }

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
    // Initialize sleep manager
    ESP_LOGI(TAG, "Initializing sleep manager...");
    ret = sleep_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize sleep manager: %s", esp_err_to_name(ret));
    }
#else
    ESP_LOGI(TAG, "Sleep manager disabled in configuration");
#endif

    // Lock LVGL for UI creation
    bsp_display_lock(0);

    // Get the actual default screen that BSP uses
    lv_obj_t *default_screen = lv_scr_act();
    ESP_LOGI(TAG, "Default active screen at startup: %p", default_screen);

    // Create main tileview for watchface <-> settings navigation
    lv_obj_t *tileview = lv_tileview_create(lv_screen_active());
    lv_obj_set_size(tileview, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLLABLE);
    ESP_LOGI(TAG, "Tileview created: %p", tileview);

    // Add watchface tile (row 0, col 0) - home tile, can swipe down
    lv_obj_t *watchface_tile = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_BOTTOM);
    ESP_LOGI(TAG, "Watchface tile created: %p", watchface_tile);
    
    lv_obj_t *watchface = watchface_create(watchface_tile);
    if (watchface)
    {
        ESP_LOGI(TAG, "Watchface created on tile: %p", watchface);
    }

    // Add settings tile (row 0, col 1) - can swipe up to return to watchface
    lv_obj_t *settings_tile = lv_tileview_add_tile(tileview, 0, 1, LV_DIR_TOP);
    ESP_LOGI(TAG, "Settings tile created: %p", settings_tile);
    
    // Create settings on its tile
    settings_create(settings_tile);
    settings_set_tileview(tileview); // Give settings access to tileview for navigation

    // Create sub-screens as separate screens (not tiles)
    // These are navigated to from settings menu
    display_settings_create(lv_screen_active());
    system_settings_create(lv_screen_active());
    about_screen_create(lv_screen_active());
    wifi_settings_create(lv_screen_active());
    wifi_scan_create(lv_screen_active());
    // Note: wifi_password screen is created on-demand when needed

    // Set watchface tile as the active tile initially
    lv_tileview_set_tile_by_id(tileview, 0, 0, LV_ANIM_OFF);

    // Unlock LVGL
    bsp_display_unlock();

    ESP_LOGI(TAG, "Watch initialized successfully with tileview navigation");

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
    // Create sleep monitoring task
    xTaskCreate(sleep_check_task, "sleep_check", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Sleep monitoring task created");
#endif

    // Create button monitor task for reset functionality
    xTaskCreate(button_monitor_task, "button_monitor", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Button monitor task created (long press power button for 3s to reset)");
}