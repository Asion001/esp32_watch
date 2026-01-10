/**
 * @file about_screen.c
 * @brief About Screen Implementation
 */

#include "about_screen.h"
#include "uptime_tracker.h"
#include "build_time.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "AboutScreen";

// Firmware version (update this with each release)
#define FIRMWARE_VERSION "0.2.0-dev"

// UI elements
static lv_obj_t *about_screen = NULL;
static lv_obj_t *info_label = NULL;
static lv_obj_t *previous_screen = NULL;

/**
 * @brief Back button event handler
 */
static void back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "Back button clicked");
        about_screen_hide();
    }
}

/**
 * @brief Build the system information text
 */
static void build_info_text(char *buffer, size_t buffer_size)
{
    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    // Get build time
    struct tm build_time;
    get_build_time(&build_time);
    
    // Get uptime stats
    uptime_stats_t uptime_stats;
    uptime_tracker_get_stats(&uptime_stats);
    
    // Format uptime
    char uptime_str[32];
    char total_uptime_str[32];
    uptime_tracker_format_time(uptime_stats.current_uptime_sec, uptime_str, sizeof(uptime_str));
    uptime_tracker_format_time(uptime_stats.total_uptime_sec, total_uptime_str, sizeof(total_uptime_str));
    
    // Build the information string
    snprintf(buffer, buffer_size,
        "ESP32-C6 Watch\n"
        "Version: %s\n\n"
        "Build: %04d-%02d-%02d %02d:%02d\n\n"
        "Uptime: %s\n"
        "Total: %s\n"
        "Boots: %u\n\n"
        "ESP-IDF: v%d.%d.%d\n"
        "Chip: %s Rev %d\n"
        "Cores: %d\n"
        "Flash: %dMB %s",
        FIRMWARE_VERSION,
        build_time.tm_year + 1900, build_time.tm_mon + 1, build_time.tm_mday,
        build_time.tm_hour, build_time.tm_min,
        uptime_str,
        total_uptime_str,
        uptime_stats.boot_count,
        ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH,
        (chip_info.model == CHIP_ESP32C6) ? "ESP32-C6" : "Unknown",
        chip_info.revision,
        chip_info.cores,
        spi_flash_get_chip_size() / (1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external"
    );
}

lv_obj_t *about_screen_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating about screen");

    // Create screen container
    about_screen = lv_obj_create(parent);
    lv_obj_set_size(about_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(about_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(about_screen, 0, 0);
    lv_obj_set_style_pad_all(about_screen, 10, 0);
    
    // Hide by default
    lv_obj_add_flag(about_screen, LV_OBJ_FLAG_HIDDEN);

    // Create title label
    lv_obj_t *title = lv_label_create(about_screen);
    lv_label_set_text(title, "About");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Create back button
    lv_obj_t *back_btn = lv_btn_create(about_screen);
    lv_obj_set_size(back_btn, 60, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_20, 0);
    lv_obj_center(back_label);

    // Create scrollable container for info
    lv_obj_t *info_container = lv_obj_create(about_screen);
    lv_obj_set_size(info_container, LV_PCT(90), LV_PCT(75));
    lv_obj_align(info_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(info_container, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(info_container, 1, 0);
    lv_obj_set_style_border_color(info_container, lv_color_hex(0x444444), 0);
    lv_obj_set_scrollbar_mode(info_container, LV_SCROLLBAR_MODE_AUTO);

    // Create info label
    info_label = lv_label_create(info_container);
    lv_obj_set_width(info_label, LV_PCT(95));
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 10, 10);
    
    // Build and set info text
    char info_text[512];
    build_info_text(info_text, sizeof(info_text));
    lv_label_set_text(info_label, info_text);

    ESP_LOGI(TAG, "About screen created");
    return about_screen;
}

void about_screen_show(void)
{
    if (about_screen)
    {
        ESP_LOGI(TAG, "Showing about screen");
        
        // Save reference to previous screen
        previous_screen = lv_scr_act();
        
        // Update info text with latest values
        char info_text[512];
        build_info_text(info_text, sizeof(info_text));
        lv_label_set_text(info_label, info_text);
        
        // Show screen
        lv_obj_clear_flag(about_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(about_screen);
    }
    else
    {
        ESP_LOGW(TAG, "About screen not created");
    }
}

void about_screen_hide(void)
{
    if (about_screen && previous_screen)
    {
        ESP_LOGI(TAG, "Hiding about screen");
        
        // Return to previous screen
        lv_scr_load(previous_screen);
        lv_obj_add_flag(about_screen, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        ESP_LOGW(TAG, "Cannot hide about screen: screen refs invalid");
    }
}
