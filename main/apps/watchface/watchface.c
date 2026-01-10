/**
 * @file watchface.c
 * @brief Digital Watchface Application Implementation
 */

#include "watchface.h"
#include "rtc_pcf85063.h"
#include "pmu_axp2101.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "Watchface";

// UI elements
static lv_obj_t *screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *battery_label = NULL;
static lv_timer_t *update_timer = NULL;

// Day and month name arrays
static const char *day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * @brief Timer callback to update time and battery every second
 */
static void watchface_timer_cb(lv_timer_t *timer) {
    struct tm time;
    uint8_t battery_percent = 0;
    bool is_charging = false;
    
    // Read time from RTC
    if (rtc_read_time(&time) == ESP_OK) {
        // Update time label (HH:MM format)
        lv_label_set_text_fmt(time_label, "%02d:%02d", time.tm_hour, time.tm_min);
        
        // Update date label (Day, Month DD)
        if (time.tm_wday >= 0 && time.tm_wday < 7 && 
            time.tm_mon >= 0 && time.tm_mon < 12) {
            lv_label_set_text_fmt(date_label, "%s, %s %d", 
                day_names[time.tm_wday], 
                month_names[time.tm_mon], 
                time.tm_mday);
        }
    }
    
    // Read battery status
    if (pmu_get_battery_percent(&battery_percent) == ESP_OK) {
        pmu_is_charging(&is_charging);
        
        // Update battery label with percentage
        if (is_charging) {
            lv_label_set_text_fmt(battery_label, LV_SYMBOL_CHARGE " %d%%", battery_percent);
        } else {
            lv_label_set_text_fmt(battery_label, "%d%%", battery_percent);
        }
        
        // Change color based on battery level
        if (battery_percent > 30) {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0);  // Green
        } else if (battery_percent > 15) {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFF00), 0);  // Yellow
        } else {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFF0000), 0);  // Red
        }
    }
}

lv_obj_t* watchface_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating watchface");
    
    // Initialize I2C peripherals
    i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
    if (!i2c) {
        ESP_LOGE(TAG, "Failed to get I2C handle from BSP");
        return NULL;
    }
    
    // Initialize RTC
    if (rtc_init(i2c) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RTC");
    }
    
    // Initialize PMU
    if (pmu_init(i2c) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PMU");
    }
    
    // Create main screen container
    screen = lv_obj_create(parent);
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Create BIG time label (HH:MM) - centered
    time_label = lv_label_create(screen);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_label_set_text(time_label, "00:00");
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -30);
    
    // Create date label - below time
    date_label = lv_label_create(screen);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x888888), 0);  // Gray
    lv_label_set_text(date_label, "Day, Month DD");
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 30);
    
    // Create battery indicator - top right corner
    battery_label = lv_label_create(screen);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0);  // Green initially
    lv_label_set_text(battery_label, "100%");
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    
    // Create update timer (1000ms = 1 second)
    update_timer = lv_timer_create(watchface_timer_cb, 1000, NULL);
    
    // Do initial update immediately
    watchface_timer_cb(NULL);
    
    ESP_LOGI(TAG, "Watchface created successfully");
    return screen;
}

void watchface_update(void) {
    // Force immediate update
    if (update_timer) {
        lv_timer_ready(update_timer);
    }
}
