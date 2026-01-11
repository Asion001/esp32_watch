#include "apps/settings/screens/about_screen.h"
#include "apps/settings/screens/display_settings.h"
#include "apps/settings/screens/system_settings.h"
#include "apps/settings/screens/wifi_password.h"
#include "apps/settings/screens/wifi_scan.h"
#include "apps/settings/screens/wifi_settings.h"
#include "apps/settings/settings.h"
#include "apps/watchface/watchface.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "screen_manager.h"
#include "settings_storage.h"
#include "sleep_manager.h"
#include "wifi_manager.h"

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
static void button_monitor_task(void *pvParameters) {
  ESP_LOGI(TAG, "Button monitor task started");

  // Configure button GPIO as input with pull-up
  gpio_config_t button_conf = {.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
                               .mode = GPIO_MODE_INPUT,
                               .pull_up_en = GPIO_PULLUP_ENABLE,
                               .pull_down_en = GPIO_PULLDOWN_DISABLE,
                               .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&button_conf);

  uint32_t press_start_ms = 0;
  bool was_pressed = false;

  while (1) {
    // Button is active-low (pressed = 0)
    bool is_pressed = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);

    if (is_pressed && !was_pressed) {
      // Button just pressed
      press_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      was_pressed = true;
      ESP_LOGD(TAG, "Button pressed");
    } else if (is_pressed && was_pressed) {
      // Button still pressed - check duration
      uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      uint32_t press_duration = current_ms - press_start_ms;

      if (press_duration >= BUTTON_LONG_PRESS_MS) {
        ESP_LOGI(TAG, "Long press detected - resetting watch...");
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay for log to flush
        esp_restart();
      }
    } else if (!is_pressed && was_pressed) {
      // Button released
      was_pressed = false;
      ESP_LOGD(TAG, "Button released");
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
  }
}

void app_main(void) {
  esp_log_level_set("*", ESP_LOG_INFO);
  ESP_LOGI(TAG, "Starting ESP32-C6 Watch Firmware");

  // Initialize NVS flash (required by WiFi driver and settings storage)
  ESP_LOGI(TAG, "Initializing NVS...");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGI(TAG, "NVS partition needs erasing, erasing...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize settings storage (needs NVS, used by WiFi manager for
  // credentials)
  ESP_LOGI(TAG, "Initializing settings storage...");
  ret = settings_storage_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize settings storage: %s",
             esp_err_to_name(ret));
  }

  // Start display subsystem
  ESP_LOGI(TAG, "Initializing display...");
  bsp_display_start();

  // Initialize screen manager
  ESP_LOGI(TAG, "Initializing screen manager...");
  ret = screen_manager_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize screen manager: %s",
             esp_err_to_name(ret));
  }

  // Initialize WiFi manager (needs NVS and settings_storage to be initialized
  // first)
  ESP_LOGI(TAG, "Initializing WiFi manager...");
  ret = wifi_manager_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s",
             esp_err_to_name(ret));
  } else {
    // Register WiFi status callback
    wifi_manager_register_callback(wifi_status_callback, NULL);

    // Auto-connect if credentials are saved
    if (wifi_manager_has_credentials()) {
      ESP_LOGI(TAG, "Saved WiFi credentials found, attempting auto-connect...");
      wifi_manager_auto_connect();
    }
  }

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
  // Initialize sleep manager
  ESP_LOGI(TAG, "Initializing sleep manager...");
  ret = sleep_manager_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize sleep manager: %s",
             esp_err_to_name(ret));
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
  // Create a new screen for the tileview
  lv_obj_t *tileview_screen = lv_obj_create(NULL);

  // Set tileview screen background to black
  lv_obj_set_style_bg_color(tileview_screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(tileview_screen, LV_OPA_COVER, 0);

  lv_obj_t *tileview = lv_tileview_create(tileview_screen);
  lv_obj_set_size(tileview, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLLABLE);

  // Set tileview background to black
  lv_obj_set_style_bg_color(tileview, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_COVER, 0);

  ESP_LOGI(TAG, "Tileview created: %p on screen: %p", tileview,
           tileview_screen);

  // Add watchface tile (col 0, row 0) - home tile, can swipe down
  // For vertical scrolling: col stays 0, row changes (0 -> 1)
  lv_obj_t *watchface_tile =
      lv_tileview_add_tile(tileview, 0, 0, LV_DIR_BOTTOM);

  // Set watchface tile background to black
  lv_obj_set_style_bg_color(watchface_tile, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(watchface_tile, LV_OPA_COVER, 0);

  ESP_LOGI(TAG, "Watchface tile created: %p", watchface_tile);

  lv_obj_t *watchface = watchface_create(watchface_tile);
  if (watchface) {
    ESP_LOGI(TAG, "Watchface created on tile: %p", watchface);
  }

  // Add settings tile (col 0, row 1) - below watchface, can swipe up to return
  lv_obj_t *settings_tile = lv_tileview_add_tile(tileview, 0, 1, LV_DIR_TOP);

  // Set settings tile background to black
  lv_obj_set_style_bg_color(settings_tile, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(settings_tile, LV_OPA_COVER, 0);

  ESP_LOGI(TAG, "Settings tile created: %p", settings_tile);

  // Create settings on its tile
  settings_create(settings_tile);
  settings_set_tileview(
      tileview); // Give settings access to tileview for navigation

  // Create sub-screens as separate screens (not tiles)
  // These are navigated to from settings menu
  // Use the default screen that BSP initialized
  display_settings_create(default_screen);
  system_settings_create(default_screen);
  about_screen_create(default_screen);
  wifi_settings_create(default_screen);
  wifi_scan_create(default_screen);
  // Note: wifi_password screen is created on-demand when needed

  // Set watchface tile as the active tile initially
  lv_tileview_set_tile_by_index(tileview, 0, 0, LV_ANIM_OFF);

  // CRITICAL: Load the tileview screen to make it visible
  lv_scr_load(tileview_screen);
  ESP_LOGI(TAG, "Tileview screen loaded and now active");

  // Unlock LVGL
  bsp_display_unlock();

  ESP_LOGI(TAG, "Watch initialized successfully with tileview navigation");

  // Create button monitor task for reset functionality
  xTaskCreate(button_monitor_task, "button_monitor", 2048, NULL, 5, NULL);
  ESP_LOGI(
      TAG,
      "Button monitor task created (long press power button for 3s to reset)");
}