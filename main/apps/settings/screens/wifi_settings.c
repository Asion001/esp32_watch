/**
 * @file wifi_settings.c
 * @brief WiFi settings screen implementation
 */

#include "wifi_settings.h"
#include "wifi_scan.h"
#include "wifi_manager.h"
#include "screen_manager.h"
#include "../settings.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
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

// Forward declarations
static void back_button_event_cb(lv_event_t *e);
static void scan_button_event_cb(lv_event_t *e);
static void disconnect_button_event_cb(lv_event_t *e);
static void forget_button_event_cb(lv_event_t *e);
static void update_connection_info(void);
static const char *get_signal_indicator(int8_t rssi);

lv_obj_t *wifi_settings_create(lv_obj_t *parent)
{
    // Create screen
    wifi_settings_screen = lv_obj_create(parent);
    lv_obj_set_size(wifi_settings_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(wifi_settings_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(wifi_settings_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(wifi_settings_screen, LV_OBJ_FLAG_HIDDEN);

    // Title
    lv_obj_t *title = lv_label_create(wifi_settings_screen);
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Back button
    lv_obj_t *back_btn = lv_btn_create(wifi_settings_screen);
    lv_obj_set_size(back_btn, 50, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    // Container for status info
    lv_obj_t *container = lv_obj_create(wifi_settings_screen);
    lv_obj_set_size(container, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(container, 1, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x444444), 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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
    lv_obj_set_size(scan_btn, LV_HOR_RES - 80, 50);
    lv_obj_add_event_cb(scan_btn, scan_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan for Networks");
    lv_obj_center(scan_label);

    // Disconnect button (hidden by default)
    disconnect_btn = lv_btn_create(container);
    lv_obj_set_size(disconnect_btn, LV_HOR_RES - 80, 50);
    lv_obj_add_event_cb(disconnect_btn, disconnect_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0xFF6600), 0);
    lv_obj_add_flag(disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *disconnect_label = lv_label_create(disconnect_btn);
    lv_label_set_text(disconnect_label, "Disconnect");
    lv_obj_center(disconnect_label);

    // Forget button (hidden by default)
    forget_btn = lv_btn_create(container);
    lv_obj_set_size(forget_btn, LV_HOR_RES - 80, 50);
    lv_obj_add_event_cb(forget_btn, forget_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0x666666), 0);
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *forget_label = lv_label_create(forget_btn);
    lv_label_set_text(forget_label, "Forget Network");
    lv_obj_center(forget_label);

    return wifi_settings_screen;
}

void wifi_settings_show(void)
{
    if (!wifi_settings_screen)
    {
        ESP_LOGE(TAG, "WiFi settings screen not created");
        return;
    }

    bsp_display_lock(0);
    lv_obj_clear_flag(wifi_settings_screen, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(wifi_settings_screen);
    bsp_display_unlock();

    // Update status
    wifi_settings_update_status();
}

void wifi_settings_update_status(void)
{
    if (!wifi_settings_screen)
    {
        return;
    }

    bsp_display_lock(0);

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

    bsp_display_unlock();
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

    // Get signal strength
    if (wifi_manager_get_rssi(&rssi) == ESP_OK)
    {
        snprintf(buffer, sizeof(buffer), "Signal: %s %d dBm", get_signal_indicator(rssi), rssi);
        lv_label_set_text(signal_label, buffer);
    }

    // Get IP address
    if (wifi_manager_get_ip_address(ip, sizeof(ip)) == ESP_OK)
    {
        snprintf(buffer, sizeof(buffer), "IP: %s", ip);
        lv_label_set_text(ip_label, buffer);
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

static void back_button_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Back button pressed");
    lv_obj_t *settings = settings_get_screen();
    if (settings)
    {
        screen_manager_show(settings);
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
