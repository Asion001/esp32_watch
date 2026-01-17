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
#include "esp_task_wdt.h"
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
static lv_obj_t *g_watchface_tile = NULL;
static lv_obj_t *g_settings_tile = NULL;

#ifdef CONFIG_APP_STATE_RESTORE_ENABLE
static int32_t g_saved_tile_row = 0;
static int32_t g_saved_tile_col = 0;

#define SETTINGS_KEY_TILE_ROW "ui_tile_row"
#define SETTINGS_KEY_TILE_COL "ui_tile_col"

static void app_state_restore_save_tile(int32_t col, int32_t row)
{
  if (col == g_saved_tile_col && row == g_saved_tile_row)
  {
    return;
  }

  esp_err_t row_ret = settings_set_int(SETTINGS_KEY_TILE_ROW, row);
  esp_err_t col_ret = settings_set_int(SETTINGS_KEY_TILE_COL, col);
  if (row_ret != ESP_OK || col_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to persist tile state (row=%ld col=%ld)",
             (long)row, (long)col);
  }
  else
  {
    g_saved_tile_row = row;
    g_saved_tile_col = col;
  }
}

static void app_state_restore_load_tile(int32_t *col, int32_t *row)
{
  int32_t saved_row = 0;
  int32_t saved_col = 0;

  settings_get_int(SETTINGS_KEY_TILE_ROW, 0, &saved_row);
  settings_get_int(SETTINGS_KEY_TILE_COL, 0, &saved_col);

  if (saved_row < 0 || saved_row > 1)
  {
    saved_row = 0;
  }

  if (saved_col < 0 || saved_col > 0)
  {
    saved_col = 0;
  }

  *row = saved_row;
  *col = saved_col;
  g_saved_tile_row = saved_row;
  g_saved_tile_col = saved_col;
}

static void tileview_state_event_cb(lv_event_t *e)
{
  lv_obj_t *tileview = lv_event_get_target(e);
  lv_obj_t *active_tile = lv_tileview_get_tile_active(tileview);

  if (active_tile == g_watchface_tile)
  {
    app_state_restore_save_tile(0, 0);
  }
  else if (active_tile == g_settings_tile)
  {
    app_state_restore_save_tile(0, 1);
  }
}
#endif

#ifdef CONFIG_APP_WATCHDOG_ENABLE
static TaskHandle_t app_watchdog_task_handle = NULL;

static void app_watchdog_task(void *param)
{
  (void)param;
  ESP_LOGI(TAG, "App watchdog task started");

  esp_err_t add_ret = esp_task_wdt_add(NULL);
  if (add_ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to add watchdog task: %s", esp_err_to_name(add_ret));
  }

  while (1)
  {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_WATCHDOG_FEED_INTERVAL_MS));
  }
}
#endif

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

#ifdef CONFIG_APP_WATCHDOG_ENABLE
  ESP_LOGI(TAG, "Initializing task watchdog...");
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = CONFIG_APP_WATCHDOG_TIMEOUT_SECONDS * 1000,
      .idle_core_mask = 0,
      .trigger_panic = CONFIG_APP_WATCHDOG_PANIC,
  };
  esp_err_t wdt_ret = esp_task_wdt_init(&wdt_config);
  if (wdt_ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to init task watchdog: %s", esp_err_to_name(wdt_ret));
  }
  else
  {
    BaseType_t task_ret =
        xTaskCreate(app_watchdog_task, "app_wdt", 2048, NULL, 2,
                    &app_watchdog_task_handle);
    if (task_ret != pdPASS)
    {
      ESP_LOGE(TAG, "Failed to create watchdog task");
      app_watchdog_task_handle = NULL;
    }
  }
#endif

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
  g_watchface_tile = watchface_tile;

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
  g_settings_tile = settings_tile;

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
  int32_t target_tile_col = 0;
  int32_t target_tile_row = 0;

#ifdef CONFIG_APP_STATE_RESTORE_ENABLE
  sleep_manager_sleep_type_t last_sleep_type = SLEEP_MANAGER_SLEEP_TYPE_NONE;
  if (sleep_manager_get_last_sleep_type(&last_sleep_type) &&
      last_sleep_type == SLEEP_MANAGER_SLEEP_TYPE_DEEP)
  {
    app_state_restore_load_tile(&target_tile_col, &target_tile_row);
    ESP_LOGI(TAG, "Restoring tile after deep sleep: (%ld,%ld)",
             (long)target_tile_col, (long)target_tile_row);
  }

  lv_obj_add_event_cb(tileview, tileview_state_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);
#endif

  lv_tileview_set_tile_by_index(tileview, target_tile_col, target_tile_row,
                                LV_ANIM_OFF);

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
