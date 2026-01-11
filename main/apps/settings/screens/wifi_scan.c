/**
 * @file wifi_scan.c
 * @brief WiFi scan results screen implementation
 */

#include "wifi_scan.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "screen_manager.h"
#include "wifi_manager.h"
#include "wifi_password.h"
#include "wifi_settings.h"
#include <string.h>

static const char *TAG = "wifi_scan";

// Screen objects
static lv_obj_t *wifi_scan_screen = NULL;
static lv_obj_t *ap_list = NULL;
static lv_obj_t *loading_label = NULL;

// Scan results
static wifi_ap_info_t scan_results[20];
static uint16_t scan_count = 0;

// Forward declarations
static void back_button_event_cb(lv_event_t *e);
static void ap_list_event_cb(lv_event_t *e);
static void start_scan(void);
static void update_ap_list(void);
static const char *get_signal_bars(int8_t rssi);
static const char *get_security_icon(wifi_auth_mode_t auth);

lv_obj_t *wifi_scan_create(lv_obj_t *parent) {
  // Create screen
  wifi_scan_screen = lv_obj_create(parent);
  lv_obj_set_size(wifi_scan_screen, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_bg_color(wifi_scan_screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(wifi_scan_screen, LV_OPA_COVER, 0);
  lv_obj_add_flag(wifi_scan_screen, LV_OBJ_FLAG_HIDDEN);

  // Title
  lv_obj_t *title = lv_label_create(wifi_scan_screen);
  lv_label_set_text(title, "WiFi Networks");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  // Back button
  lv_obj_t *back_btn = lv_btn_create(wifi_scan_screen);
  lv_obj_set_size(back_btn, 50, 40);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
  lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  // Loading label
  loading_label = lv_label_create(wifi_scan_screen);
  lv_label_set_text(loading_label, "Scanning...");
  lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_16, 0);
  lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 0);

  // AP list (initially hidden)
  ap_list = lv_list_create(wifi_scan_screen);
  lv_obj_set_size(ap_list, LV_HOR_RES - 20, LV_VER_RES - 70);
  lv_obj_align(ap_list, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_bg_color(ap_list, lv_color_hex(0x111111), 0);
  lv_obj_add_flag(ap_list, LV_OBJ_FLAG_HIDDEN);

  return wifi_scan_screen;
}

void wifi_scan_show(void) {
  if (!wifi_scan_screen) {
    ESP_LOGE(TAG, "WiFi scan screen not created");
    return;
  }

  bsp_display_lock(0);
  lv_obj_clear_flag(wifi_scan_screen, LV_OBJ_FLAG_HIDDEN);
  lv_scr_load(wifi_scan_screen);

  // Show loading, hide list
  lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ap_list, LV_OBJ_FLAG_HIDDEN);
  bsp_display_unlock();

  // Start scan
  start_scan();
}

static void start_scan(void) {
  ESP_LOGI(TAG, "Starting WiFi scan...");

  esp_err_t ret = wifi_manager_scan_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
    bsp_display_lock(0);
    lv_label_set_text(loading_label, "Scan failed");
    bsp_display_unlock();
    return;
  }

  // Wait for scan to complete (typically 1-3 seconds)
  vTaskDelay(pdMS_TO_TICKS(3000));

  // Get scan results
  scan_count = 20; // Max 20 APs
  ret = wifi_manager_get_scan_results(scan_results, &scan_count);

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Found %d networks", scan_count);
    update_ap_list();
  } else {
    ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(ret));
    bsp_display_lock(0);
    lv_label_set_text(loading_label, "No networks found");
    bsp_display_unlock();
  }
}

static void update_ap_list(void) {
  bsp_display_lock(0);

  // Hide loading, show list
  lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ap_list, LV_OBJ_FLAG_HIDDEN);

  // Clear existing list
  lv_obj_clean(ap_list);

  if (scan_count == 0) {
    lv_obj_add_flag(ap_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(loading_label, "No networks found");
    bsp_display_unlock();
    return;
  }

  // Add APs to list
  for (uint16_t i = 0; i < scan_count; i++) {
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

static const char *get_signal_bars(int8_t rssi) {
  if (rssi >= -50) {
    return "****"; // Excellent
  } else if (rssi >= -60) {
    return "*** "; // Good
  } else if (rssi >= -70) {
    return "**  "; // Fair
  } else {
    return "*   "; // Weak
  }
}

static const char *get_security_icon(wifi_auth_mode_t auth) {
  if (auth == WIFI_AUTH_OPEN) {
    return "O"; // Open network (LV_SYMBOL_UNLOCK not available)
  } else {
    return "L"; // Secured network (LV_SYMBOL_LOCK not available)
  }
}

static void back_button_event_cb(lv_event_t *e) {
  ESP_LOGI(TAG, "Back button pressed");
  wifi_settings_show();
}

static void ap_list_event_cb(lv_event_t *e) {
  uint16_t index = (uint16_t)(uintptr_t)lv_event_get_user_data(e);

  if (index >= scan_count) {
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
