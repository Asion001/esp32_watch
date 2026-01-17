/**
 * @file time_sync_server.c
 * @brief NTP server configuration screen implementation
 */

#include "time_sync_server.h"
#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ntp_client.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "TimeSyncServer";

#ifdef CONFIG_NTP_CLIENT_ENABLE

static lv_obj_t *server_screen = NULL;
static lv_obj_t *server_ta = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *keyboard = NULL;

static void time_sync_server_hide(void)
{
    server_screen = NULL;
    server_ta = NULL;
    status_label = NULL;
    keyboard = NULL;
}

static void save_button_event_cb(lv_event_t *e)
{
    (void)e;

    const char *server = lv_textarea_get_text(server_ta);
    if (!server || strlen(server) == 0)
    {
        bsp_display_lock(0);
        lv_label_set_text(status_label, "Server cannot be empty");
        bsp_display_unlock();
        return;
    }

    esp_err_t ret = ntp_client_set_ntp_server(server);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set NTP server: %s", esp_err_to_name(ret));
        bsp_display_lock(0);
        lv_label_set_text(status_label, "Invalid server");
        bsp_display_unlock();
        return;
    }

    ESP_LOGI(TAG, "NTP server saved: %s", server);
    bsp_display_lock(0);
    screen_manager_go_back();
    bsp_display_unlock();
}

static void reset_button_event_cb(lv_event_t *e)
{
    (void)e;

    bsp_display_lock(0);
    lv_textarea_set_text(server_ta, CONFIG_NTP_DEFAULT_SERVER);
    lv_label_set_text(status_label, "Reset to default");
    bsp_display_unlock();
}

lv_obj_t *time_sync_server_create(void)
{
    if (server_screen)
    {
        return server_screen;
    }

    screen_config_t config = {
        .title = "NTP Server",
        .show_back_button = true,
        .anim_type = SCREEN_ANIM_HORIZONTAL,
        .hide_callback = time_sync_server_hide,
    };

    server_screen = screen_manager_create(&config);
    if (!server_screen)
    {
        ESP_LOGE(TAG, "Failed to create NTP server screen");
        return NULL;
    }

    lv_obj_t *container = lv_obj_create(server_screen);
    lv_obj_set_size(container, LV_PCT(90), LV_VER_RES - 140);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP + 45);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(container, 1, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x444444), 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_style_pad_row(container, 10, 0);

    server_ta = lv_textarea_create(container);
    lv_obj_set_size(server_ta, LV_PCT(100), 50);
    lv_textarea_set_one_line(server_ta, true);
    lv_textarea_set_placeholder_text(server_ta, "pool.ntp.org");
    lv_textarea_set_max_length(server_ta, 63);

    char server[64] = {0};
    if (ntp_client_get_ntp_server(server, sizeof(server)) == ESP_OK &&
        strlen(server) > 0)
    {
        lv_textarea_set_text(server_ta, server);
    }

    status_label = lv_label_create(container);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFAA00), 0);

    lv_obj_t *btn_row = lv_obj_create(container);
    lv_obj_set_size(btn_row, LV_PCT(100), 60);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *save_btn = lv_btn_create(btn_row);
    lv_obj_set_size(save_btn, 120, 45);
    lv_obj_add_event_cb(save_btn, save_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x00AA00), 0);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    lv_obj_t *reset_btn = lv_btn_create(btn_row);
    lv_obj_set_size(reset_btn, 120, 45);
    lv_obj_add_event_cb(reset_btn, reset_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0x666666), 0);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Default");
    lv_obj_center(reset_label);

    keyboard = lv_keyboard_create(server_screen);
    lv_obj_set_size(keyboard, LV_HOR_RES, LV_VER_RES / 2);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard, server_ta);

    return server_screen;
}

void time_sync_server_show(void)
{
    if (!server_screen)
    {
        time_sync_server_create();
    }

    if (!server_screen)
    {
        ESP_LOGE(TAG, "NTP server screen not created");
        return;
    }

    bsp_display_lock(0);
    screen_manager_show(server_screen);
    bsp_display_unlock();
}

#else // CONFIG_NTP_CLIENT_ENABLE

lv_obj_t *time_sync_server_create(void) { return NULL; }

void time_sync_server_show(void)
{
    ESP_LOGW(TAG, "Time sync disabled in menuconfig");
}

#endif // CONFIG_NTP_CLIENT_ENABLE
