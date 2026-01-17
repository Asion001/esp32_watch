/**
 * @file wifi_scan.c
 * @brief WiFi scan results screen implementation
 */

#include "wifi_scan.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "wifi_manager.h"
#include "wifi_password.h"
#include "wifi_settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wifi_scan";

// Screen objects
static lv_obj_t *wifi_scan_screen = NULL;
static lv_obj_t *ap_list = NULL;
static lv_obj_t *loading_label = NULL;

// Scan results
static wifi_ap_info_t scan_results[20];
static uint16_t scan_count = 0;
static bool scan_in_progress = false;

// Forward declarations
static void ap_list_event_cb(lv_event_t *e);
static void start_scan(void);
static void scan_task(void *param);
static void update_ap_list(void);
static const char *get_signal_bars(int8_t rssi);
static const char *get_security_icon(wifi_auth_mode_t auth);
static void wifi_scan_hide(void);

lv_obj_t *wifi_scan_create(lv_obj_t *parent)
{
  // Return existing screen if already created
  if (wifi_scan_screen)
  {
    ESP_LOGI(TAG, "WiFi scan screen already exists, returning existing");
    return wifi_scan_screen;
  }

  ESP_LOGI(TAG, "Creating WiFi scan screen");

  // Create screen using screen_manager
  screen_config_t config = {
      .title = "WiFi Networks",
      .show_back_button = true,
      .anim_type = SCREEN_ANIM_HORIZONTAL,
      .hide_callback = wifi_scan_hide,
  };

  wifi_scan_screen = screen_manager_create(&config);
  if (!wifi_scan_screen)
  {
    ESP_LOGE(TAG, "Failed to create WiFi scan screen");
    return NULL;
  }

  // Loading label
  loading_label = lv_label_create(wifi_scan_screen);
  lv_label_set_text(loading_label, "Scanning...");
  lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(loading_label, lv_color_white(), 0);
  lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 0);

  // AP list (initially hidden)
  ap_list = lv_list_create(wifi_scan_screen);
  lv_obj_set_size(ap_list, LV_PCT(90), LV_VER_RES - 120);
  lv_obj_align(ap_list, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP + 45);
  lv_obj_set_style_bg_color(ap_list, lv_color_hex(0x111111), 0);
  lv_obj_add_flag(ap_list, LV_OBJ_FLAG_HIDDEN);

  ESP_LOGI(TAG, "WiFi scan screen created");
  return wifi_scan_screen;
}

void wifi_scan_show(void)
{
  // Create screen if it doesn't exist
  if (!wifi_scan_screen)
  {
    ESP_LOGI(TAG, "Creating WiFi scan screen on demand");
    wifi_scan_create(NULL);
  }

  if (wifi_scan_screen)
  {
    ESP_LOGI(TAG, "Showing WiFi scan screen");
    bsp_display_lock(0);
    screen_manager_show(wifi_scan_screen);

    // Show loading, hide list
    lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ap_list, LV_OBJ_FLAG_HIDDEN);
    bsp_display_unlock();

    // Start scan
    if (!scan_in_progress)
    {
      scan_in_progress = true;
      xTaskCreate(scan_task, "wifi_scan", 4096, NULL, 5, NULL);
    }
  }
  else
  {
    ESP_LOGE(TAG, "Failed to create WiFi scan screen");
  }
}

void wifi_scan_hide(void)
{
  ESP_LOGI(TAG, "Hiding WiFi scan screen");
  // Clear references since screen will be auto-deleted
  wifi_scan_screen = NULL;
  ap_list = NULL;
  loading_label = NULL;
}

static void start_scan(void)
{
  ESP_LOGI(TAG, "Starting WiFi scan...");

  esp_err_t ret = wifi_manager_scan_start();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
    if (loading_label)
    {
      bsp_display_lock(0);
      lv_label_set_text(loading_label, "Scan failed");
      bsp_display_unlock();
    }
    return;
  }

  // Wait for scan to complete with timeout
  // Active scan across all channels can take several seconds
  const int timeout_ms = 10000;
  esp_err_t wait_ret = wifi_manager_wait_for_scan(timeout_ms);
  if (wait_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Scan wait timeout after %d ms", timeout_ms);
    if (wifi_manager_get_state() == WIFI_STATE_SCANNING)
    {
      if (loading_label)
      {
        bsp_display_lock(0);
        lv_label_set_text(loading_label, "Scan timeout");
        bsp_display_unlock();
      }
      return;
    }
  }

  // Get scan results
  scan_count = 20; // Max 20 APs
  ESP_LOGI(TAG, "Requesting scan results, max count: %d", scan_count);
  ret = wifi_manager_get_scan_results(scan_results, &scan_count);

  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "Found %d networks", scan_count);

    // Debug: Log first few SSIDs
    for (int i = 0; i < (scan_count < 3 ? scan_count : 3); i++)
    {
      ESP_LOGI(TAG, "  AP[%d]: %s (RSSI: %d)", i, scan_results[i].ssid,
               scan_results[i].rssi);
    }

    if (wifi_scan_screen && ap_list && loading_label)
    {
      update_ap_list();
    }
  }
  else
  {
    ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(ret));
    if (loading_label)
    {
      bsp_display_lock(0);
      lv_label_set_text(loading_label, "Scan failed");
      bsp_display_unlock();
    }
  }
}

static void scan_task(void *param)
{
  (void)param;

  start_scan();

  scan_in_progress = false;
  vTaskDelete(NULL);
}

static void update_ap_list(void)
{
  bsp_display_lock(0);

  // Hide loading, show list
  lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ap_list, LV_OBJ_FLAG_HIDDEN);

  // Clear existing list
  lv_obj_clean(ap_list);

  if (scan_count == 0)
  {
    lv_obj_add_flag(ap_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(loading_label, "No networks found");
    bsp_display_unlock();
    return;
  }

  // Add APs to list
  for (uint16_t i = 0; i < scan_count; i++)
  {
    char label_text[64];
    snprintf(label_text, sizeof(label_text), "%s  %s %s",
             get_security_icon(scan_results[i].authmode),
             (char *)scan_results[i].ssid,
             get_signal_bars(scan_results[i].rssi));

    lv_obj_t *btn = lv_list_add_btn(ap_list, NULL, label_text);
    lv_obj_set_height(btn, 60);
    lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(btn, ap_list_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
  }

  bsp_display_unlock();
}

static const char *get_signal_bars(int8_t rssi)
{
  if (rssi >= -50)
  {
    return "****"; // Excellent
  }
  else if (rssi >= -60)
  {
    return "*** "; // Good
  }
  else if (rssi >= -70)
  {
    return "**  "; // Fair
  }
  else
  {
    return "*   "; // Weak
  }
}

static const char *get_security_icon(wifi_auth_mode_t auth)
{
  if (auth == WIFI_AUTH_OPEN)
  {
    return "O"; // Open network (LV_SYMBOL_UNLOCK not available)
  }
  else
  {
    return "L"; // Secured network (LV_SYMBOL_LOCK not available)
  }
}

static void ap_list_event_cb(lv_event_t *e)
{
  uint16_t index = (uint16_t)(uintptr_t)lv_event_get_user_data(e);

  if (index >= scan_count)
  {
    ESP_LOGE(TAG, "Invalid AP index: %d", index);
    return;
  }

  wifi_ap_info_t *ap = &scan_results[index];
  ESP_LOGI(TAG, "Selected AP: %s (RSSI: %d, Auth: %d)", ap->ssid, ap->rssi,
           ap->authmode);

  // Check if network is open
  bool is_open = (ap->authmode == WIFI_AUTH_OPEN);

  // Show password input screen
  wifi_password_show((char *)ap->ssid, is_open);
}
