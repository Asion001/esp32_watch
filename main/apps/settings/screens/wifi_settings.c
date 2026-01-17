/**
 * @file wifi_settings.c
 * @brief WiFi settings screen implementation
 */

#include "wifi_settings.h"
#include "../settings.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "wifi_manager.h"
#include "wifi_scan.h"
#include <string.h>

static const char *TAG = "wifi_settings";

// Screen objects
static lv_obj_t *wifi_settings_screen = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *ssid_label = NULL;
static lv_obj_t *signal_label = NULL;
static lv_obj_t *ip_label = NULL;
static lv_obj_t *scan_btn = NULL;
static lv_obj_t *disconnect_btn = NULL;
static lv_obj_t *forget_btn = NULL;
static lv_timer_t *status_timer = NULL;

// Forward declarations
static void scan_button_event_cb(lv_event_t *e);
static void disconnect_button_event_cb(lv_event_t *e);
static void forget_button_event_cb(lv_event_t *e);
static void update_connection_info(void);
static void wifi_settings_update_status_internal(bool lock_display);
static void wifi_status_timer_cb(lv_timer_t *timer);
static const char *get_signal_indicator(int8_t rssi);
static void wifi_settings_hide(void);

lv_obj_t *wifi_settings_create(lv_obj_t *parent)
{
  // Return existing screen if already created
  if (wifi_settings_screen)
  {
    ESP_LOGI(TAG, "WiFi settings screen already exists, returning existing");
    return wifi_settings_screen;
  }

  ESP_LOGI(TAG, "Creating WiFi settings screen");

  // Create screen using screen_manager
  screen_config_t config = {
      .title = "WiFi",
      .show_back_button = true,
      .anim_type = SCREEN_ANIM_HORIZONTAL,
      .hide_callback = wifi_settings_hide,
  };

  wifi_settings_screen = screen_manager_create(&config);
  if (!wifi_settings_screen)
  {
    ESP_LOGE(TAG, "Failed to create WiFi settings screen");
    return NULL;
  }

  // Container for status info
  lv_obj_t *container = lv_obj_create(wifi_settings_screen);
  lv_obj_set_size(container, LV_PCT(90), LV_VER_RES - 120);
  lv_obj_align(container, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP + 45);
  lv_obj_set_style_bg_color(container, lv_color_hex(0x222222), 0);
  lv_obj_set_style_border_width(container, 1, 0);
  lv_obj_set_style_border_color(container, lv_color_hex(0x444444), 0);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(container, 10, 0);
  lv_obj_set_style_pad_row(container, 10, 0);

  // Status label
  status_label = lv_label_create(container);
  lv_label_set_text(status_label, "Status: Checking...");
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);

  // SSID label (only visible when connected)
  ssid_label = lv_label_create(container);
  lv_label_set_text(ssid_label, "Network: ---");
  lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);

  // Signal label (only visible when connected)
  signal_label = lv_label_create(container);
  lv_label_set_text(signal_label, "Signal: ---");
  lv_obj_set_style_text_font(signal_label, &lv_font_montserrat_14, 0);

  // IP label (only visible when connected)
  ip_label = lv_label_create(container);
  lv_label_set_text(ip_label, "IP: ---");
  lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);

  // Scan button
  scan_btn = lv_btn_create(container);
  lv_obj_set_size(scan_btn, LV_PCT(90), 50);
  lv_obj_add_event_cb(scan_btn, scan_button_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *scan_label = lv_label_create(scan_btn);
  lv_label_set_text(scan_label, "Scan for Networks");
  lv_obj_center(scan_label);

  // Disconnect button (hidden by default)
  disconnect_btn = lv_btn_create(container);
  lv_obj_set_size(disconnect_btn, LV_PCT(90), 50);
  lv_obj_add_event_cb(disconnect_btn, disconnect_button_event_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0xFF6600), 0);
  lv_obj_add_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *disconnect_label = lv_label_create(disconnect_btn);
  lv_label_set_text(disconnect_label, "Disconnect");
  lv_obj_center(disconnect_label);

  // Forget button (hidden by default)
  forget_btn = lv_btn_create(container);
  lv_obj_set_size(forget_btn, LV_PCT(90), 50);
  lv_obj_add_event_cb(forget_btn, forget_button_event_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0x666666), 0);
  lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *forget_label = lv_label_create(forget_btn);
  lv_label_set_text(forget_label, "Forget Network");
  lv_obj_center(forget_label);

  ESP_LOGI(TAG, "WiFi settings screen created");
  return wifi_settings_screen;
}

void wifi_settings_show(void)
{
  if (wifi_settings_screen)
  {
    ESP_LOGI(TAG, "Showing WiFi settings screen");
    bsp_display_lock(0);
    screen_manager_show(wifi_settings_screen);

    if (!status_timer)
    {
      status_timer = lv_timer_create(wifi_status_timer_cb, 2000, NULL);
    }
    bsp_display_unlock();

    // Update status
    wifi_settings_update_status();
  }
  else
  {
    ESP_LOGW(TAG, "WiFi settings screen not created");
  }
}

void wifi_settings_hide(void)
{
  ESP_LOGI(TAG, "Hiding WiFi settings screen");
  if (status_timer)
  {
    lv_timer_del(status_timer);
    status_timer = NULL;
  }
  // Clear references since screen will be auto-deleted
  wifi_settings_screen = NULL;
  status_label = NULL;
  ssid_label = NULL;
  signal_label = NULL;
  ip_label = NULL;
  scan_btn = NULL;
  disconnect_btn = NULL;
  forget_btn = NULL;
}

void wifi_settings_update_status(void)
{
  if (!wifi_settings_screen)
  {
    return;
  }

  wifi_settings_update_status_internal(true);
}

static void wifi_settings_update_status_internal(bool lock_display)
{
  if (!wifi_settings_screen)
  {
    return;
  }

  if (lock_display)
  {
    bsp_display_lock(0);
  }

  wifi_state_t state = wifi_manager_get_state();

  switch (state)
  {
  case WIFI_STATE_SCANNING:
    lv_label_set_text(status_label, "Status: Scanning...");
    lv_label_set_text(ssid_label, "Network: ---");
    lv_label_set_text(signal_label, "Signal: ---");
    lv_label_set_text(ip_label, "IP: ---");
    lv_obj_add_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case WIFI_STATE_DISCONNECTED:
    lv_label_set_text(status_label, "Status: Disconnected");
    lv_label_set_text(ssid_label, "Network: ---");
    lv_label_set_text(signal_label, "Signal: ---");
    lv_label_set_text(ip_label, "IP: ---");
    lv_obj_add_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case WIFI_STATE_CONNECTING:
    lv_label_set_text(status_label, "Status: Connecting...");
    lv_label_set_text(ssid_label, "Network: ---");
    lv_label_set_text(signal_label, "Signal: ---");
    lv_label_set_text(ip_label, "IP: ---");
    lv_obj_add_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case WIFI_STATE_CONNECTED:
    lv_label_set_text(status_label, "Status: Connected");
    update_connection_info();
    lv_obj_clear_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case WIFI_STATE_FAILED:
    lv_label_set_text(status_label, "Status: Connection Failed");
    lv_label_set_text(ssid_label, "Network: ---");
    lv_label_set_text(signal_label, "Signal: ---");
    lv_label_set_text(ip_label, "IP: ---");
    lv_obj_add_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    break;
  }

  if (lock_display)
  {
    bsp_display_unlock();
  }
}

static void wifi_status_timer_cb(lv_timer_t *timer)
{
  (void)timer;
  wifi_settings_update_status_internal(false);
}

static void update_connection_info(void)
{
  char ssid[33] = {0};
  char ip[16] = {0};
  int8_t rssi = 0;
  char buffer[64];

  // Get SSID
  if (wifi_manager_get_connected_ssid(ssid, sizeof(ssid)) == ESP_OK)
  {
    snprintf(buffer, sizeof(buffer), "Network: %s", ssid);
    lv_label_set_text(ssid_label, buffer);
  }
  else
  {
    lv_label_set_text(ssid_label, "Network: ---");
  }

  // Get signal strength
  if (wifi_manager_get_rssi(&rssi) == ESP_OK)
  {
    snprintf(buffer, sizeof(buffer), "Signal: %s %d dBm",
             get_signal_indicator(rssi), rssi);
    lv_label_set_text(signal_label, buffer);
  }
  else
  {
    lv_label_set_text(signal_label, "Signal: ---");
  }

  // Get IP address
  if (wifi_manager_get_ip_address(ip, sizeof(ip)) == ESP_OK)
  {
    snprintf(buffer, sizeof(buffer), "IP: %s", ip);
    lv_label_set_text(ip_label, buffer);
  }
  else
  {
    lv_label_set_text(ip_label, "IP: ---");
  }
}

static const char *get_signal_indicator(int8_t rssi)
{
  if (rssi >= -50)
  {
    return LV_SYMBOL_WIFI; // Excellent
  }
  else if (rssi >= -60)
  {
    return LV_SYMBOL_WIFI; // Good
  }
  else if (rssi >= -70)
  {
    return LV_SYMBOL_WIFI; // Fair
  }
  else
  {
    return LV_SYMBOL_WIFI; // Weak
  }
}

static void scan_button_event_cb(lv_event_t *e)
{
  ESP_LOGI(TAG, "Scan button pressed");
  wifi_scan_show();
}

static void disconnect_button_event_cb(lv_event_t *e)
{
  ESP_LOGI(TAG, "Disconnect button pressed");
  esp_err_t ret = wifi_manager_disconnect();
  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "Disconnected from WiFi");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
  }
}

static void forget_button_event_cb(lv_event_t *e)
{
  ESP_LOGI(TAG, "Forget button pressed");
  esp_err_t ret = wifi_manager_clear_credentials();
  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "WiFi credentials cleared");
    wifi_manager_disconnect();
  }
  else
  {
    ESP_LOGE(TAG, "Failed to clear credentials: %s", esp_err_to_name(ret));
  }
}
