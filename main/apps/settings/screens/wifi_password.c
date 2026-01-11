/**
 * @file wifi_password.c
 * @brief WiFi password input screen implementation
 */

#include "wifi_password.h"
#include "wifi_settings.h"
#include "wifi_manager.h"
#include "screen_manager.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "wifi_password";

// Screen objects
static lv_obj_t *wifi_password_screen = NULL;
static lv_obj_t *keyboard = NULL;
static lv_obj_t *password_ta = NULL;
static lv_obj_t *save_checkbox = NULL;
static lv_obj_t *status_label = NULL;

// Current SSID
static char current_ssid[33] = {0};
static bool is_open_network = false;

// Forward declarations
static void cancel_button_event_cb(lv_event_t *e);
static void connect_button_event_cb(lv_event_t *e);
static void show_hide_button_event_cb(lv_event_t *e);
static void do_connect(void);

lv_obj_t *wifi_password_create(lv_obj_t *parent, const char *ssid, bool is_open) {
    // Save SSID and open status
    strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
    is_open_network = is_open;
    
    // Create screen
    wifi_password_screen = lv_obj_create(parent);
    lv_obj_set_size(wifi_password_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(wifi_password_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(wifi_password_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(wifi_password_screen, LV_OBJ_FLAG_HIDDEN);

    // Title
    lv_obj_t *title = lv_label_create(wifi_password_screen);
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "Connect to:\n%s", ssid);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    if (!is_open) {
        // Password input area
        password_ta = lv_textarea_create(wifi_password_screen);
        lv_obj_set_size(password_ta, LV_HOR_RES - 40, 50);
        lv_obj_align(password_ta, LV_ALIGN_TOP_MID, 0, 60);
        lv_textarea_set_placeholder_text(password_ta, "Password");
        lv_textarea_set_password_mode(password_ta, true);
        lv_textarea_set_one_line(password_ta, true);
        lv_textarea_set_max_length(password_ta, 64);

        // Show/Hide password button
        lv_obj_t *show_hide_btn = lv_btn_create(wifi_password_screen);
        lv_obj_set_size(show_hide_btn, 60, 40);
        lv_obj_align_to(show_hide_btn, password_ta, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        lv_obj_add_event_cb(show_hide_btn, show_hide_button_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *eye_label = lv_label_create(show_hide_btn);
        lv_label_set_text(eye_label, LV_SYMBOL_EYE_OPEN);
        lv_obj_center(eye_label);

        // Keyboard
        keyboard = lv_keyboard_create(wifi_password_screen);
        lv_obj_set_size(keyboard, LV_HOR_RES, LV_VER_RES / 2);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(keyboard, password_ta);

        // Save credentials checkbox
        save_checkbox = lv_checkbox_create(wifi_password_screen);
        lv_checkbox_set_text(save_checkbox, "Remember network");
        lv_obj_align(save_checkbox, LV_ALIGN_TOP_LEFT, 20, 120);
        lv_obj_add_state(save_checkbox, LV_STATE_CHECKED);  // Default: save
    } else {
        // Open network - no password needed
        password_ta = NULL;
        keyboard = NULL;
        save_checkbox = lv_checkbox_create(wifi_password_screen);
        lv_checkbox_set_text(save_checkbox, "Remember network");
        lv_obj_align(save_checkbox, LV_ALIGN_TOP_LEFT, 20, 80);
        lv_obj_add_state(save_checkbox, LV_STATE_CHECKED);
    }

    // Status label
    status_label = lv_label_create(wifi_password_screen);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF6600), 0);
    if (is_open) {
        lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 120);
    } else {
        lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 180);
    }

    // Button container
    lv_obj_t *btn_container = lv_obj_create(wifi_password_screen);
    lv_obj_set_size(btn_container, LV_HOR_RES - 40, 60);
    if (is_open) {
        lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 160);
    } else {
        lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    }
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);

    // Cancel button
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, 150, 50);
    lv_obj_add_event_cb(cancel_btn, cancel_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x666666), 0);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    // Connect button
    lv_obj_t *connect_btn = lv_btn_create(btn_container);
    lv_obj_set_size(connect_btn, 150, 50);
    lv_obj_add_event_cb(connect_btn, connect_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x00AA00), 0);
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);

    return wifi_password_screen;
}

void wifi_password_show(const char *ssid, bool is_open) {
    // Clean up existing screen if it exists
    if (wifi_password_screen) {
        bsp_display_lock(0);
        lv_obj_del(wifi_password_screen);
        wifi_password_screen = NULL;
        bsp_display_unlock();
    }
    
    // Create new screen
    lv_obj_t *screen = wifi_password_create(lv_scr_act(), ssid, is_open);
    
    if (!screen) {
        ESP_LOGE(TAG, "Failed to create WiFi password screen");
        return;
    }

    bsp_display_lock(0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(screen);
    bsp_display_unlock();
}

static void cancel_button_event_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Cancel button pressed");
    
    // Clear password buffer
    if (password_ta) {
        const char *pwd = lv_textarea_get_text(password_ta);
        if (pwd) {
            memset((void *)pwd, 0, strlen(pwd));
        }
    }
    
    wifi_settings_show();
}

static void connect_button_event_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Connect button pressed");
    do_connect();
}

static void show_hide_button_event_cb(lv_event_t *e) {
    if (!password_ta) return;
    
    bool is_pwd_mode = lv_textarea_get_password_mode(password_ta);
    lv_textarea_set_password_mode(password_ta, !is_pwd_mode);
    
    // Update button icon
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (is_pwd_mode) {
        lv_label_set_text(label, LV_SYMBOL_EYE_CLOSE);
    } else {
        lv_label_set_text(label, LV_SYMBOL_EYE_OPEN);
    }
}

static void do_connect(void) {
    const char *password = "";
    bool save_creds = false;
    
    // Get password (if not open network)
    if (!is_open_network && password_ta) {
        password = lv_textarea_get_text(password_ta);
        if (!password || strlen(password) < 8) {
            bsp_display_lock(0);
            lv_label_set_text(status_label, "Password must be 8-64 characters");
            bsp_display_unlock();
            return;
        }
    }
    
    // Get save credentials checkbox state
    if (save_checkbox) {
        save_creds = (lv_obj_get_state(save_checkbox) & LV_STATE_CHECKED) != 0;
    }
    
    // Show connecting status
    bsp_display_lock(0);
    lv_label_set_text(status_label, "Connecting...");
    bsp_display_unlock();
    
    // Attempt connection
    ESP_LOGI(TAG, "Connecting to %s (save: %d)", current_ssid, save_creds);
    esp_err_t ret = wifi_manager_connect(current_ssid, password, save_creds);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
        bsp_display_lock(0);
        lv_label_set_text(status_label, "Connection failed");
        bsp_display_unlock();
        return;
    }
    
    // Wait for connection (timeout 10 seconds)
    int timeout = 10;
    while (timeout > 0 && wifi_manager_get_state() == WIFI_STATE_CONNECTING) {
        vTaskDelay(pdMS_TO_TICKS(500));
        timeout--;
    }
    
    // Clear password buffer
    if (password && !is_open_network) {
        memset((void *)password, 0, strlen(password));
    }
    
    // Check result
    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "Successfully connected to %s", current_ssid);
        wifi_settings_show();
    } else {
        ESP_LOGE(TAG, "Connection failed or timed out");
        bsp_display_lock(0);
        lv_label_set_text(status_label, "Connection failed. Check password.");
        bsp_display_unlock();
    }
}
