/**
 * @file ota_settings.c
 * @brief OTA settings screen implementation
 */

#include "ota_settings.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "ota_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ota_settings";

#ifdef CONFIG_ENABLE_OTA

// Screen objects
static lv_obj_t *ota_screen = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *current_version_label = NULL;
static lv_obj_t *latest_version_label = NULL;
static lv_obj_t *url_label = NULL;
static lv_obj_t *progress_label = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *check_btn = NULL;
static lv_obj_t *update_btn = NULL;

static bool update_in_progress = false;

// Forward declarations
static void ota_check_event_cb(lv_event_t *e);
static void ota_update_event_cb(lv_event_t *e);
static void ota_settings_hide(void);
static void ota_update_task(void *param);
static void ota_progress_cb(ota_state_t state, uint8_t progress, void *user_data);
static void ota_set_buttons_enabled(bool enabled);
static void ota_update_status_text(const char *text, bool lock_display);

static void ota_set_buttons_enabled(bool enabled)
{
    if (!check_btn || !update_btn)
    {
        return;
    }

    if (enabled)
    {
        lv_obj_clear_state(check_btn, LV_STATE_DISABLED);
        lv_obj_clear_state(update_btn, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_add_state(check_btn, LV_STATE_DISABLED);
        lv_obj_add_state(update_btn, LV_STATE_DISABLED);
    }
}

static void ota_update_status_text(const char *text, bool lock_display)
{
    if (!status_label || !text)
    {
        return;
    }

    if (lock_display)
    {
        bsp_display_lock(0);
    }

    lv_label_set_text(status_label, text);

    if (lock_display)
    {
        bsp_display_unlock();
    }
}

static void ota_progress_cb(ota_state_t state, uint8_t progress, void *user_data)
{
    (void)user_data;

    if (!ota_screen || !status_label || !progress_bar || !progress_label)
    {
        return;
    }

    bsp_display_lock(0);

    switch (state)
    {
    case OTA_STATE_CHECKING:
        lv_label_set_text(status_label, "Status: Checking...");
        break;
    case OTA_STATE_DOWNLOADING:
        lv_label_set_text(status_label, "Status: Downloading...");
        lv_bar_set_value(progress_bar, progress, LV_ANIM_OFF);
        {
            char progress_text[32];
            snprintf(progress_text, sizeof(progress_text), "Progress: %u%%", progress);
            lv_label_set_text(progress_label, progress_text);
        }
        break;
    case OTA_STATE_COMPLETE:
        lv_label_set_text(status_label, "Status: Complete (restarting)");
        lv_bar_set_value(progress_bar, 100, LV_ANIM_OFF);
        lv_label_set_text(progress_label, "Progress: 100%");
        update_in_progress = false;
        ota_set_buttons_enabled(true);
        break;
    case OTA_STATE_FAILED:
        lv_label_set_text(status_label, "Status: Failed");
        update_in_progress = false;
        ota_set_buttons_enabled(true);
        break;
    case OTA_STATE_IDLE:
    default:
        lv_label_set_text(status_label, "Status: Idle");
        break;
    }

    bsp_display_unlock();
}

lv_obj_t *ota_settings_create(lv_obj_t *parent)
{
    if (ota_screen)
    {
        ESP_LOGI(TAG, "OTA settings screen already exists, returning existing");
        return ota_screen;
    }

    ESP_LOGI(TAG, "Creating OTA settings screen");

    ota_manager_init();

    screen_config_t config = {
        .title = "App: OTA Updates",
        .show_back_button = true,
        .anim_type = SCREEN_ANIM_HORIZONTAL,
        .hide_callback = ota_settings_hide,
    };

    ota_screen = screen_manager_create(&config);
    if (!ota_screen)
    {
        ESP_LOGE(TAG, "Failed to create OTA settings screen");
        return NULL;
    }

    lv_obj_t *container = lv_obj_create(ota_screen);
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

    current_version_label = lv_label_create(container);
    lv_obj_set_style_text_font(current_version_label, &lv_font_montserrat_14, 0);

    latest_version_label = lv_label_create(container);
    lv_label_set_text(latest_version_label, "Latest Version: ---");
    lv_obj_set_style_text_font(latest_version_label, &lv_font_montserrat_14, 0);

    url_label = lv_label_create(container);
    lv_label_set_text(url_label, "Update URL: ---");
    lv_obj_set_style_text_font(url_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(url_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(url_label, LV_PCT(85));

    progress_label = lv_label_create(container);
    lv_label_set_text(progress_label, "Progress: 0%");
    lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_14, 0);

    progress_bar = lv_bar_create(container);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_width(progress_bar, LV_PCT(90));
    lv_obj_set_height(progress_bar, 18);

    check_btn = lv_btn_create(container);
    lv_obj_set_size(check_btn, LV_PCT(90), 50);
    lv_obj_add_event_cb(check_btn, ota_check_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *check_label = lv_label_create(check_btn);
    lv_label_set_text(check_label, "Check for Updates");
    lv_obj_center(check_label);

    update_btn = lv_btn_create(container);
    lv_obj_set_size(update_btn, LV_PCT(90), 50);
    lv_obj_set_style_bg_color(update_btn, lv_color_hex(0x00AA88), 0);
    lv_obj_add_event_cb(update_btn, ota_update_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *update_label = lv_label_create(update_btn);
    lv_label_set_text(update_label, "Start OTA Update");
    lv_obj_center(update_label);

    ota_manager_register_callback(ota_progress_cb, NULL);

    ESP_LOGI(TAG, "OTA settings screen created");
    return ota_screen;
}

void ota_settings_show(void)
{
    if (!ota_screen)
    {
        ESP_LOGW(TAG, "OTA settings screen not created");
        return;
    }

    ESP_LOGI(TAG, "Showing OTA settings screen");

    bsp_display_lock(0);
    screen_manager_show(ota_screen);

    char current_version[32] = {0};
    if (ota_manager_get_current_version(current_version, sizeof(current_version)) == ESP_OK)
    {
        char version_text[64];
        snprintf(version_text, sizeof(version_text), "Current Version: %s", current_version);
        lv_label_set_text(current_version_label, version_text);
    }
    else
    {
        lv_label_set_text(current_version_label, "Current Version: ---");
    }

    char update_url[256] = {0};
    if (ota_manager_get_update_url(update_url, sizeof(update_url)) == ESP_OK)
    {
        char url_text[300];
        snprintf(url_text, sizeof(url_text), "Update URL: %s", update_url);
        lv_label_set_text(url_label, url_text);
    }
    else
    {
        lv_label_set_text(url_label, "Update URL: ---");
    }

    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(progress_label, "Progress: 0%");
    lv_label_set_text(status_label, "Status: Idle");

    if (update_in_progress)
    {
        ota_set_buttons_enabled(false);
    }
    else
    {
        ota_set_buttons_enabled(true);
    }

    bsp_display_unlock();
}

static void ota_settings_hide(void)
{
    ESP_LOGI(TAG, "Hiding OTA settings screen");

    ota_manager_register_callback(NULL, NULL);

    ota_screen = NULL;
    status_label = NULL;
    current_version_label = NULL;
    latest_version_label = NULL;
    url_label = NULL;
    progress_label = NULL;
    progress_bar = NULL;
    check_btn = NULL;
    update_btn = NULL;
}

static void ota_check_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
    {
        return;
    }

    if (update_in_progress)
    {
        ESP_LOGW(TAG, "OTA update already in progress");
        return;
    }

    ota_update_status_text("Status: Checking...", false);

    ota_version_info_t info = {0};
    esp_err_t ret = ota_manager_check_for_update(NULL, &info);
    if (ret == ESP_OK)
    {
        char latest_text[64];
        snprintf(latest_text, sizeof(latest_text), "Latest Version: %s", info.version);
        lv_label_set_text(latest_version_label, latest_text);
        ota_update_status_text("Status: Check complete", false);
    }
    else
    {
        ota_update_status_text("Status: Check failed", false);
    }
}

static void ota_update_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
    {
        return;
    }

    if (update_in_progress)
    {
        ESP_LOGW(TAG, "OTA update already in progress");
        return;
    }

    update_in_progress = true;
    ota_set_buttons_enabled(false);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(progress_label, "Progress: 0%");
    lv_label_set_text(status_label, "Status: Starting...");

    if (xTaskCreate(ota_update_task, "ota_update_task", 4096, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start OTA task");
        update_in_progress = false;
        ota_set_buttons_enabled(true);
        lv_label_set_text(status_label, "Status: Failed to start task");
    }
}

static void ota_update_task(void *param)
{
    (void)param;

    esp_err_t ret = ota_manager_start_update(NULL);
    if (ret != ESP_OK)
    {
        update_in_progress = false;
        ota_update_status_text("Status: Update failed", true);
        ota_set_buttons_enabled(true);
    }

    vTaskDelete(NULL);
}

#else

lv_obj_t *ota_settings_create(lv_obj_t *parent)
{
    (void)parent;
    return NULL;
}

void ota_settings_show(void)
{
}

#endif
