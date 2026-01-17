/**
 * @file time_sync.c
 * @brief Time synchronization settings screen implementation
 */

#include "time_sync.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "ntp_client.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "time_sync_server.h"
#ifdef CONFIG_ENABLE_WIFI
#include "wifi_manager.h"
#endif
#include <string.h>
#include <time.h>
#include <stdio.h>

static const char *TAG = "TimeSync";

#ifdef CONFIG_NTP_CLIENT_ENABLE

static lv_obj_t *time_sync_screen = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *last_sync_label = NULL;
static lv_obj_t *server_label = NULL;
static lv_obj_t *timezone_dropdown = NULL;
static lv_obj_t *dst_switch = NULL;

static const char *timezone_options =
    "UTC-12\nUTC-11\nUTC-10\nUTC-9\nUTC-8\nUTC-7\nUTC-6\nUTC-5\n"
    "UTC-4\nUTC-3\nUTC-2\nUTC-1\nUTC+0\nUTC+1\nUTC+2\nUTC+3\n"
    "UTC+4\nUTC+5\nUTC+6\nUTC+7\nUTC+8\nUTC+9\nUTC+10\nUTC+11\n"
    "UTC+12\nUTC+13\nUTC+14";

static void time_sync_hide(void)
{
    time_sync_screen = NULL;
    status_label = NULL;
    last_sync_label = NULL;
    server_label = NULL;
    timezone_dropdown = NULL;
    dst_switch = NULL;
}

static int timezone_index_from_string(const char *timezone)
{
    if (!timezone)
    {
        return 12;
    }

    if (strncmp(timezone, "UTC", 3) != 0)
    {
        return 12;
    }

    int sign = 1;
    int hours = 0;
    int minutes = 0;

    if (timezone[3] == '+')
    {
        sign = 1;
    }
    else if (timezone[3] == '-')
    {
        sign = -1;
    }
    else if (timezone[3] == '\0')
    {
        sign = 1;
    }
    else
    {
        return 12;
    }

    if (timezone[3] == '\0')
    {
        hours = 0;
    }
    else if (strchr(timezone + 4, ':'))
    {
        if (sscanf(timezone + 4, "%d:%d", &hours, &minutes) < 1)
        {
            return 12;
        }
    }
    else
    {
        if (sscanf(timezone + 4, "%d", &hours) != 1)
        {
            return 12;
        }
    }

    int32_t total_hours = sign * hours;

    int index = total_hours + 12;
    if (index < 0)
    {
        index = 0;
    }
    else if (index > 26)
    {
        index = 26;
    }

    (void)minutes;
    return index;
}

static void update_last_sync_label(void)
{
    time_t last_sync = 0;
    char buffer[64] = {0};

    if (ntp_client_get_last_sync(&last_sync) != ESP_OK || last_sync == 0)
    {
        snprintf(buffer, sizeof(buffer), "Last Sync: Never");
        lv_label_set_text(last_sync_label, buffer);
        return;
    }

    struct tm local_time = {0};
    if (ntp_client_get_local_time_from_utc(last_sync, &local_time) != ESP_OK)
    {
        snprintf(buffer, sizeof(buffer), "Last Sync: ---");
        lv_label_set_text(last_sync_label, buffer);
        return;
    }

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", &local_time);
    snprintf(buffer, sizeof(buffer), "Last Sync: %s", time_str);
    lv_label_set_text(last_sync_label, buffer);
}

static void update_server_label(void)
{
    char server[64] = {0};
    if (ntp_client_get_ntp_server(server, sizeof(server)) == ESP_OK &&
        strlen(server) > 0)
    {
        lv_label_set_text_fmt(server_label, "Server: %s", server);
    }
    else
    {
        lv_label_set_text(server_label, "Server: ---");
    }
}

static void sync_button_event_cb(lv_event_t *e)
{
    (void)e;
#ifdef CONFIG_ENABLE_WIFI
    if (!wifi_manager_is_connected())
    {
        bsp_display_lock(0);
        lv_label_set_text(status_label, "WiFi disconnected");
        bsp_display_unlock();
        return;
    }
#endif

    esp_err_t ret = ntp_client_sync_now();
    bsp_display_lock(0);
    if (ret == ESP_OK)
    {
        lv_label_set_text(status_label, "Sync requested");
    }
    else
    {
        lv_label_set_text(status_label, "Sync failed");
    }
    bsp_display_unlock();
}

static void edit_server_event_cb(lv_event_t *e)
{
    (void)e;
    time_sync_server_show();
}

static void timezone_changed_event_cb(lv_event_t *e)
{
    (void)e;
    if (!timezone_dropdown)
    {
        return;
    }

    char selection[16];
    lv_dropdown_get_selected_str(timezone_dropdown, selection,
                                 sizeof(selection));
    if (strlen(selection) == 0)
    {
        return;
    }

    if (ntp_client_set_timezone(selection) != ESP_OK)
    {
        ESP_LOGW(TAG, "Invalid timezone selection: %s", selection);
    }

    update_last_sync_label();
}

static void dst_switch_event_cb(lv_event_t *e)
{
    (void)e;
    if (!dst_switch)
    {
        return;
    }

    bool enabled = lv_obj_has_state(dst_switch, LV_STATE_CHECKED);
    ntp_client_set_dst_enabled(enabled);
    update_last_sync_label();
}

lv_obj_t *time_sync_create(lv_obj_t *parent)
{
    (void)parent;

    if (time_sync_screen)
    {
        return time_sync_screen;
    }

    screen_config_t config = {
        .title = "Time & Sync",
        .show_back_button = true,
        .anim_type = SCREEN_ANIM_HORIZONTAL,
        .hide_callback = time_sync_hide,
    };

    time_sync_screen = screen_manager_create(&config);
    if (!time_sync_screen)
    {
        ESP_LOGE(TAG, "Failed to create time sync screen");
        return NULL;
    }

    lv_obj_t *container = lv_obj_create(time_sync_screen);
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

    status_label = lv_label_create(container);
    lv_label_set_text(status_label, "Status: Idle");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);

    last_sync_label = lv_label_create(container);
    lv_label_set_text(last_sync_label, "Last Sync: ---");
    lv_obj_set_style_text_font(last_sync_label, &lv_font_montserrat_14, 0);

    server_label = lv_label_create(container);
    lv_label_set_text(server_label, "Server: ---");
    lv_obj_set_style_text_font(server_label, &lv_font_montserrat_14, 0);

    lv_obj_t *server_btn = lv_btn_create(container);
    lv_obj_set_size(server_btn, LV_PCT(90), 45);
    lv_obj_add_event_cb(server_btn, edit_server_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *server_btn_label = lv_label_create(server_btn);
    lv_label_set_text(server_btn_label, "Edit NTP Server");
    lv_obj_center(server_btn_label);

    lv_obj_t *tz_label = lv_label_create(container);
    lv_label_set_text(tz_label, "Time Zone");
    lv_obj_set_style_text_font(tz_label, &lv_font_montserrat_14, 0);

    timezone_dropdown = lv_dropdown_create(container);
    lv_dropdown_set_options_static(timezone_dropdown, timezone_options);
    lv_obj_set_width(timezone_dropdown, LV_PCT(90));
    lv_obj_add_event_cb(timezone_dropdown, timezone_changed_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *dst_row = lv_obj_create(container);
    lv_obj_set_size(dst_row, LV_PCT(90), 40);
    lv_obj_set_style_bg_opa(dst_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dst_row, 0, 0);
    lv_obj_set_flex_flow(dst_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dst_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *dst_label = lv_label_create(dst_row);
    lv_label_set_text(dst_label, "DST (+1h)");
    lv_obj_set_style_text_font(dst_label, &lv_font_montserrat_14, 0);

    dst_switch = lv_switch_create(dst_row);
    lv_obj_add_event_cb(dst_switch, dst_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                        NULL);

    lv_obj_t *sync_btn = lv_btn_create(container);
    lv_obj_set_size(sync_btn, LV_PCT(90), 50);
    lv_obj_add_event_cb(sync_btn, sync_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(sync_btn, lv_color_hex(0x00AA00), 0);
    lv_obj_t *sync_label = lv_label_create(sync_btn);
    lv_label_set_text(sync_label, "Sync Now");
    lv_obj_center(sync_label);

    return time_sync_screen;
}

void time_sync_show(void)
{
    if (!time_sync_screen)
    {
        time_sync_create(NULL);
    }

    if (!time_sync_screen)
    {
        ESP_LOGE(TAG, "Time sync screen not created");
        return;
    }

    time_sync_update_status();

    bsp_display_lock(0);
    screen_manager_show(time_sync_screen);
    bsp_display_unlock();
}

void time_sync_update_status(void)
{
    if (!time_sync_screen)
    {
        return;
    }

    bsp_display_lock(0);
#ifdef CONFIG_ENABLE_WIFI
    if (wifi_manager_is_connected())
    {
        lv_label_set_text(status_label, "Status: WiFi Connected");
    }
    else
    {
        lv_label_set_text(status_label, "Status: WiFi Disconnected");
    }
#else
    lv_label_set_text(status_label, "Status: WiFi Disabled");
#endif

    update_last_sync_label();
    update_server_label();

    char timezone[16] = {0};
    if (ntp_client_get_timezone(timezone, sizeof(timezone)) == ESP_OK)
    {
        int index = timezone_index_from_string(timezone);
        lv_dropdown_set_selected(timezone_dropdown, index);
    }

    bool dst_enabled = false;
    if (ntp_client_get_dst_enabled(&dst_enabled) == ESP_OK)
    {
        if (dst_enabled)
        {
            lv_obj_add_state(dst_switch, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(dst_switch, LV_STATE_CHECKED);
        }
    }

    bsp_display_unlock();
}

#else // CONFIG_NTP_CLIENT_ENABLE

lv_obj_t *time_sync_create(lv_obj_t *parent)
{
    (void)parent;
    return NULL;
}

void time_sync_show(void)
{
    ESP_LOGW(TAG, "Time sync disabled in menuconfig");
}

void time_sync_update_status(void) {}

#endif // CONFIG_NTP_CLIENT_ENABLE
