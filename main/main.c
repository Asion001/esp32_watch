#include "apps/settings/screens/about_screen.h"
#include "apps/settings/screens/display_settings.h"
#include "apps/settings/screens/system_settings.h"
#ifdef CONFIG_ENABLE_WIFI
#include "apps/settings/screens/wifi_password.h"
#include "apps/settings/screens/wifi_scan.h"
#include "apps/settings/screens/wifi_settings.h"
#include "wifi_manager.h"
#endif
#ifdef CONFIG_NTP_CLIENT_ENABLE
#include "ntp_client.h"
#endif
#ifdef CONFIG_ENABLE_OTA
#include "ota_manager.h"
#endif
#include "apps/settings/settings.h"
#include "apps/watchface/watchface.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "button_handler.h"
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

static const char *TAG = "Main";

// Store tileview reference for button handler
static lv_obj_t *g_tileview = NULL;

#ifdef CONFIG_ENABLE_WIFI
// WiFi status callback
static void wifi_status_callback(wifi_state_t state, void *user_data)
{
  ESP_LOGI(TAG, "WiFi state changed: %d", state);
  wifi_settings_update_status();

#ifdef CONFIG_NTP_CLIENT_ENABLE
  if (state == WIFI_STATE_CONNECTED)
  {
    ntp_client_on_wifi_connected();
  }
#endif
}
#endif

void app_main(void)
{
  esp_log_level_set("*", CONFIG_APP_LOG_LEVEL);
  ESP_LOGI(TAG, "Log level set to: %d", CONFIG_APP_LOG_LEVEL);
  ESP_LOGI(TAG, "Starting ESP32-C6 Watch Firmware");

  // Initialize NVS flash (required by WiFi driver and settings storage)
  ESP_LOGI(TAG, "Initializing NVS...");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGI(TAG, "NVS partition needs erasing, erasing...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize settings storage (needs NVS, used by WiFi manager for
  // credentials)
  ESP_LOGI(TAG, "Initializing settings storage...");
  ret = settings_storage_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize settings storage: %s",
             esp_err_to_name(ret));
  }

  // Start display subsystem
  ESP_LOGI(TAG, "Initializing display...");
  bsp_display_start();

  // Initialize screen manager
  ESP_LOGI(TAG, "Initializing screen manager...");
  ret = screen_manager_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize screen manager: %s",
             esp_err_to_name(ret));
  }

#ifdef CONFIG_ENABLE_WIFI
  // Initialize WiFi manager (needs NVS and settings_storage to be initialized
  // first)
  ESP_LOGI(TAG, "Initializing WiFi manager...");
  ret = wifi_manager_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s",
             esp_err_to_name(ret));
  }
  else
  {
    // Register WiFi status callback
    wifi_manager_register_callback(wifi_status_callback, NULL);

#ifdef CONFIG_WIFI_AUTO_CONNECT
    // Auto-connect (uses saved credentials or defaults if configured)
    ESP_LOGI(TAG, "Attempting WiFi auto-connect...");
    esp_err_t auto_ret = wifi_manager_auto_connect();
    if (auto_ret != ESP_OK && auto_ret != ESP_ERR_NOT_FOUND)
    {
      ESP_LOGW(TAG, "WiFi auto-connect failed: %s", esp_err_to_name(auto_ret));
    }
#endif
  }
#else
  ESP_LOGI(TAG, "WiFi disabled in configuration");
#endif
#ifdef CONFIG_NTP_CLIENT_ENABLE
  ESP_LOGI(TAG, "Initializing NTP client...");
  ret = ntp_client_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize NTP client: %s",
             esp_err_to_name(ret));
  }
#else
  ESP_LOGI(TAG, "NTP client disabled in configuration");
#endif

#ifdef CONFIG_ENABLE_OTA
  // Initialize OTA manager (requires WiFi)
  ESP_LOGI(TAG, "Initializing OTA manager...");
  ret = ota_manager_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize OTA manager: %s",
             esp_err_to_name(ret));
  }
#else
  ESP_LOGI(TAG, "OTA updates disabled in configuration");
#endif

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
  // Initialize sleep manager
  ESP_LOGI(TAG, "Initializing sleep manager...");
  ret = sleep_manager_init();
  if (ret != ESP_OK)
  {
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

  // Set tileview background to black
  lv_obj_set_style_bg_color(tileview, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_COVER, 0);

  // Store tileview reference for button handler
  g_tileview = tileview;

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
  if (watchface)
  {
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

  // NOTE: All settings sub-screens (wifi_settings, wifi_scan, display_settings, etc.)
  // are now created on-demand when opened to support auto-delete pattern.
  // This avoids navigation stack issues and memory leaks.

  // Set watchface tile as the active tile initially
  lv_tileview_set_tile_by_index(tileview, 0, 0, LV_ANIM_OFF);

  // CRITICAL: Load the tileview screen to make it visible
  lv_scr_load(tileview_screen);
  ESP_LOGI(TAG, "Tileview screen loaded and now active");

  // Register tileview screen as root for navigation stack
  screen_manager_set_root(tileview_screen);

  // Unlock LVGL
  bsp_display_unlock();

  ESP_LOGI(TAG, "Watch initialized successfully with tileview navigation");

  // Initialize button handler for navigation and reset
  button_handler_config_t btn_config = BUTTON_HANDLER_CONFIG_DEFAULT();
  btn_config.tileview = &g_tileview;
  button_handler_init(&btn_config);
  ESP_LOGI(TAG, "Button handler initialized (short=back, long 3s=reset)");
}
