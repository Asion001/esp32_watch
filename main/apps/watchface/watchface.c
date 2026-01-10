/**
 * @file watchface.c
 * @brief Digital Watchface Application Implementation
 */

#include "watchface.h"
#include "settings.h"
#include "rtc_pcf85063.h"
#include "pmu_axp2101.h"
#include "sleep_manager.h"
#include "uptime_tracker.h"
#include "screen_manager.h"
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

// Safe area padding for round display (distance from edge to safe zone)
#define SAFE_AREA_TOP 40
#define SAFE_AREA_BOTTOM SAFE_AREA_TOP
#define SAFE_AREA_HORIZONTAL 50

/**
 * @brief Widget configuration structure for UI builder
 */
typedef struct
{
    lv_obj_t **obj_ptr;       // Pointer to store created object
    const lv_font_t *font;    // Font to use
    uint32_t color;           // Text color (hex)
    const char *initial_text; // Initial text to display
    lv_align_t align;         // Alignment type
    lv_coord_t width;         // Widget width (LV_SIZE_CONTENT for auto)
    lv_coord_t height;        // Widget height (LV_SIZE_CONTENT for auto)
    int32_t padding;          // Additional padding from safe area edge
} widget_config_t;

/**
 * @brief Widget configuration table
 */
static const widget_config_t widget_configs[] = {
    // Time label - centered
    {
        .obj_ptr = &time_label,
        .font = &lv_font_montserrat_48,
        .color = 0xFFFFFF,
        .initial_text = "00:00",
        .align = LV_ALIGN_CENTER,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = -30 // Move up slightly
    },
    // Date label - below time
    {
        .obj_ptr = &date_label,
        .font = &lv_font_montserrat_20,
        .color = 0x888888,
        .initial_text = "Day, Month DD",
        .align = LV_ALIGN_CENTER,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 30 // Move down slightly
    },
    // Battery label - top right corner
    {
        .obj_ptr = &battery_label,
        .font = &lv_font_montserrat_14,
        .color = 0x00FF00,
        .initial_text = "100%",
        .align = LV_ALIGN_TOP_RIGHT,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 0 // Use default safe area
    },
    // Uptime label - bottom left
    {
        .obj_ptr = &uptime_label,
        .font = &lv_font_montserrat_14,
        .color = 0x888888,
        .initial_text = "Up: 0m",
        .align = LV_ALIGN_BOTTOM_LEFT,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 0 // Use default safe area
    },
    // Boot count label - bottom left (below uptime)
    {
        .obj_ptr = &boot_count_label,
        .font = &lv_font_montserrat_14,
        .color = 0x666666,
        .initial_text = "Total: 0m (Boot #1)",
        .align = LV_ALIGN_BOTTOM_LEFT,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 20 // Additional padding below uptime label
    }};

#define WIDGET_COUNT (sizeof(widget_configs) / sizeof(widget_configs[0]))

#ifdef CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER
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
#endif
}
#endif

/**
 * @brief Gesture event callback for opening settings
 */
static void swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    ESP_LOGI(TAG, "Event received: code=%d", code);

    if (code == LV_EVENT_GESTURE)
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        ESP_LOGI(TAG, "Gesture detected: direction=%d (BOTTOM=%d)", dir, LV_DIR_BOTTOM);
        if (dir == LV_DIR_BOTTOM)
        {
            ESP_LOGI(TAG, "Swipe down gesture detected - opening settings");
            settings_show();
        }
    }
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

    // Create screen using screen_manager (root screen - no gestures from manager)
    screen_config_t config = {
        .title = NULL,                 // No title for watchface
        .anim_type = SCREEN_ANIM_NONE, // Root screen, no animation for itself
        .hide_callback = NULL,         // No hide callback for root screen
        .has_gesture_hint = false      // No hint bar for root screen
    };

    screen = screen_manager_create(&config);
    if (!screen)
    {
        ESP_LOGE(TAG, "Failed to create screen");
        return NULL;
    }

    // Manually add swipe down gesture to open settings
    ESP_LOGI(TAG, "Setting up gesture detection for watchface");
    lv_obj_add_event_cb(screen, swipe_event_cb, LV_EVENT_GESTURE, NULL);

    // Build all widgets from configuration table
    for (size_t i = 0; i < WIDGET_COUNT; i++)
    {
        const widget_config_t *config = &widget_configs[i];

        // Create label
        lv_obj_t *label = lv_label_create(screen);

        // Set size
        lv_obj_set_size(label, config->width, config->height);

        // Apply styling
        lv_obj_set_style_text_font(label, config->font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(config->color), 0);

        // Set initial text
        lv_label_set_text(label, config->initial_text);

        // Calculate position based on alignment and safe area
        int32_t x_offset = 0;
        int32_t y_offset = 0;

        switch (config->align)
        {
        case LV_ALIGN_TOP_LEFT:
            x_offset = SAFE_AREA_HORIZONTAL;
            y_offset = SAFE_AREA_TOP;
            break;
        case LV_ALIGN_TOP_RIGHT:
            x_offset = -SAFE_AREA_HORIZONTAL;
            y_offset = SAFE_AREA_TOP;
            break;
        case LV_ALIGN_BOTTOM_LEFT:
            x_offset = SAFE_AREA_HORIZONTAL;
            y_offset = -SAFE_AREA_BOTTOM;
            break;
        case LV_ALIGN_BOTTOM_RIGHT:
            x_offset = -SAFE_AREA_HORIZONTAL;
            y_offset = -SAFE_AREA_BOTTOM;
            break;
        case LV_ALIGN_CENTER:
        default:
            // Center alignment uses padding directly as offset
            x_offset = 0;
            y_offset = 0;
            break;
        }

        // Add custom padding (can be used for stacking or fine-tuning)
        y_offset += config->padding;

        // Position widget
        lv_obj_align(label, config->align, x_offset, y_offset);

        // Store reference
        *(config->obj_ptr) = label;
    }

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