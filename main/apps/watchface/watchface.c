/**
 * @file watchface.c
 * @brief Digital Watchface Application Implementation
 */

#include "watchface.h"
#include "rtc_pcf85063.h"
#include "pmu_axp2101.h"
#include "sleep_manager.h"
#include "uptime_tracker.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "Watchface";

// UI elements
static lv_obj_t *screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *battery_label = NULL;
static lv_obj_t *uptime_label = NULL;
static lv_obj_t *boot_count_label = NULL;
static lv_timer_t *update_timer = NULL;

// Save timer for periodic NVS writes
static uint32_t save_counter = 0;
#define SAVE_INTERVAL_SECONDS 60

// Day and month name arrays
static const char *day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * @brief Touch event callback to reset sleep timer
 */
static void touch_event_cb(lv_event_t *e)
{
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)
    {
        // Reset inactivity timer on any touch
        sleep_manager_reset_timer();
        ESP_LOGD(TAG, "Touch detected, sleep timer reset");
    }
#else
    (void)e; // Suppress unused parameter warning
#endif
}

/**
 * @brief Timer callback to update time and battery every second
 */
static void watchface_timer_cb(lv_timer_t *timer)
{
    struct tm time;
    uint16_t voltage_mv = 0;
    uint8_t battery_percent = 0;
    bool is_charging = false;

    // Update uptime counter
    uptime_tracker_update();

    // Read time from RTC
    if (rtc_read_time(&time) == ESP_OK)
    {
        // Update time label (HH:MM format)
        lv_label_set_text_fmt(time_label, "%02d:%02d", time.tm_hour, time.tm_min);

        // Update date label (Day, Month DD)
        if (time.tm_wday >= 0 && time.tm_wday < 7 &&
            time.tm_mon >= 0 && time.tm_mon < 12)
        {
            lv_label_set_text_fmt(date_label, "%s, %s %d",
                                  day_names[time.tm_wday],
                                  month_names[time.tm_mon],
                                  time.tm_mday);
        }
    }
    else
    {
        // Fallback display if RTC fails
        lv_label_set_text(time_label, "--:--");
        ESP_LOGW(TAG, "Failed to read RTC time");
    }

    // Read battery status using SAFE function with retry logic
    esp_err_t battery_ret = axp2101_get_battery_data_safe(
        &voltage_mv,      // Get voltage
        &battery_percent, // Get percentage
        &is_charging      // Get charging status
    );

    if (battery_ret == ESP_OK)
    {
        // Build battery display string with percentage and voltage
        char battery_str[32];
        if (is_charging)
        {
            snprintf(battery_str, sizeof(battery_str),
                     LV_SYMBOL_CHARGE " %d%% %.2fV", battery_percent, voltage_mv / 1000.0f);
        }
        else
        {
            snprintf(battery_str, sizeof(battery_str),
                     "%d%% %.2fV", battery_percent, voltage_mv / 1000.0f);
        }
        lv_label_set_text(battery_label, battery_str);

        // Change color based on battery level
        if (battery_percent > 30)
        {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0); // Green
        }
        else if (battery_percent > 15)
        {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFF00), 0); // Yellow
        }
        else
        {
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFF0000), 0); // Red
        }
    }
    else
    {
        // Fallback display if battery reading fails completely
        lv_label_set_text(battery_label, "? --%%");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(0x888888), 0); // Gray
        ESP_LOGW(TAG, "Failed to read battery data");
    }

    // Update uptime display
    uptime_stats_t stats;
    if (uptime_tracker_get_stats(&stats) == ESP_OK)
    {
        char uptime_str[32];
        char total_str[32];
        
        uptime_tracker_format_time(stats.current_uptime_sec, uptime_str, sizeof(uptime_str));
        uptime_tracker_format_time(stats.total_uptime_sec, total_str, sizeof(total_str));
        
        // Display current session uptime
        lv_label_set_text_fmt(uptime_label, "Up: %s", uptime_str);
        
        // Display total uptime and boot count
        lv_label_set_text_fmt(boot_count_label, "Total: %s (Boot #%u)", total_str, stats.boot_count);
    }

    // Periodically save uptime to NVS (every 60 seconds)
    save_counter++;
    if (save_counter >= SAVE_INTERVAL_SECONDS)
    {
        save_counter = 0;
        esp_err_t ret = uptime_tracker_save();
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to save uptime: %s", esp_err_to_name(ret));
        }
        else
        {
            ESP_LOGD(TAG, "Uptime saved to NVS");
        }
    }
}

lv_obj_t *watchface_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating watchface");

    // Initialize I2C peripherals
    i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
    if (!i2c)
    {
        ESP_LOGE(TAG, "Failed to get I2C handle from BSP");
        return NULL;
    }

    // Initialize RTC
    if (rtc_init(i2c) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize RTC");
    }

    // Initialize PMU
    if (axp2101_init(i2c) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize PMU");
    }

    // Initialize uptime tracker
    if (uptime_tracker_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize uptime tracker");
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
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x888888), 0); // Gray
    lv_label_set_text(date_label, "Day, Month DD");
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 30);

    // Create battery indicator - top right corner
    battery_label = lv_label_create(screen);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0); // Green initially
    lv_label_set_text(battery_label, "100%");
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -80, 10);

    // Create uptime label - bottom left
    uptime_label = lv_label_create(screen);
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(0x888888), 0); // Gray
    lv_label_set_text(uptime_label, "Up: 0m");
    lv_obj_align(uptime_label, LV_ALIGN_BOTTOM_LEFT, 10, -30);

    // Create boot count and total uptime label - below uptime
    boot_count_label = lv_label_create(screen);
    lv_obj_set_style_text_font(boot_count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(boot_count_label, lv_color_hex(0x666666), 0); // Darker gray
    lv_label_set_text(boot_count_label, "Total: 0m (Boot #1)");
    lv_obj_align(boot_count_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    // Create update timer (1000ms = 1 second)
    update_timer = lv_timer_create(watchface_timer_cb, 1000, NULL);

    // Do initial update immediately
    watchface_timer_cb(NULL);

#ifdef CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER
    // Add touch event callback to screen to reset sleep timer
    lv_obj_add_event_cb(screen, touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, touch_event_cb, LV_EVENT_PRESSING, NULL);
    ESP_LOGI(TAG, "Touch event callbacks registered for sleep timer reset");
#endif

    ESP_LOGI(TAG, "Watchface created successfully");
    return screen;
}

void watchface_update(void)
{
    // Force immediate update
    if (update_timer)
    {
        lv_timer_ready(update_timer);
    }
}

lv_timer_t *watchface_get_timer(void)
{
    return update_timer;
}