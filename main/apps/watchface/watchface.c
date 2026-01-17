/**
 * @file watchface.c
 * @brief Digital Watchface Application Implementation
 */

#include "watchface.h"
#include "bsp/esp-bsp.h"
#include "safe_area.h"
#include "esp_log.h"
#include "pmu_axp2101.h"
#include "rtc_pcf85063.h"
#include "screen_manager.h"
#include "settings.h"
#include "sleep_manager.h"
#include "uptime_tracker.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <time.h>

static const char *TAG = "Watchface";

// UI elements
static lv_obj_t *screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *battery_label = NULL;
static lv_obj_t *uptime_label = NULL;
static lv_obj_t *boot_count_label = NULL;
#ifdef CONFIG_SLEEP_MANAGER_SLEEP_INDICATOR
static lv_obj_t *sleep_indicator_label = NULL;
#endif
static lv_timer_t *update_timer = NULL;
static TaskHandle_t data_task_handle = NULL;
static SemaphoreHandle_t data_mutex = NULL;

typedef struct
{
  struct tm time;
  bool time_valid;
  uint16_t voltage_mv;
  uint8_t battery_percent;
  bool is_charging;
  bool battery_valid;
} watchface_data_t;

static watchface_data_t cached_data = {0};

// Save timer for periodic NVS writes
static uint32_t save_counter = 0;
#define SAVE_INTERVAL_SECONDS 60

// Day and month name arrays
static const char *day_names[] = {"Sun", "Mon", "Tue", "Wed",
                                  "Thu", "Fri", "Sat"};
static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void watchface_data_task(void *param);
static bool watchface_get_cached_data(watchface_data_t *out);

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
        .padding = -30, // Move up slightly
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
        .padding = 30, // Move down slightly
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
        .padding = 0, // Use default safe area
    },
    // Uptime label - bottom left
    {
        .obj_ptr = &uptime_label,
        .font = &lv_font_montserrat_14,
        .color = 0x888888,
        .initial_text = "Up: 0m",
        .align = LV_ALIGN_TOP_LEFT,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 0,
    },
    // Boot count label - bottom left (below uptime)
    {
        .obj_ptr = &boot_count_label,
        .font = &lv_font_montserrat_14,
        .color = 0x666666,
        .initial_text = "T0m(B1)",
        .align = LV_ALIGN_TOP_LEFT,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 20, // Additional padding below uptime label
    }
#ifdef CONFIG_SLEEP_MANAGER_SLEEP_INDICATOR
    ,
    // Sleep countdown indicator - bottom center
    {
        .obj_ptr = &sleep_indicator_label,
        .font = &lv_font_montserrat_14,
        .color = 0xFF8800,
        .initial_text = "",
        .align = LV_ALIGN_BOTTOM_MID,
        .width = LV_SIZE_CONTENT,
        .height = LV_SIZE_CONTENT,
        .padding = 0,
    }
#endif
};

#define WIDGET_COUNT (sizeof(widget_configs) / sizeof(widget_configs[0]))

/**
 * @brief Timer callback to update time and battery every second
 */
static void watchface_timer_cb(lv_timer_t *timer)
{
  watchface_data_t data = {0};
  bool has_data = watchface_get_cached_data(&data);

  // Update uptime counter
  uptime_tracker_update();

  // Update time from cached data
  if (has_data && data.time_valid)
  {
    // Update time label (HH:MM format)
    lv_label_set_text_fmt(time_label, "%02d:%02d", data.time.tm_hour,
                          data.time.tm_min);

    // Update date label (Day, Month DD)
    if (data.time.tm_wday >= 0 && data.time.tm_wday < 7 &&
        data.time.tm_mon >= 0 && data.time.tm_mon < 12)
    {
      lv_label_set_text_fmt(date_label, "%s, %s %d",
                            day_names[data.time.tm_wday],
                            month_names[data.time.tm_mon], data.time.tm_mday);
    }
  }
  else
  {
    // Fallback display if RTC fails
    lv_label_set_text(time_label, "--:--");
    ESP_LOGW(TAG, "Failed to read RTC time");
  }

  if (has_data && data.battery_valid)
  {
    // Build battery display string with percentage and voltage
    char battery_str[32];
    uint16_t volts_int = data.voltage_mv / 1000;
    uint16_t volts_frac = (data.voltage_mv % 1000) / 10;

    if (data.is_charging)
    {
      snprintf(battery_str, sizeof(battery_str),
               LV_SYMBOL_CHARGE " %d%% %u.%02uV", data.battery_percent,
               volts_int, volts_frac);
    }
    else
    {
      snprintf(battery_str, sizeof(battery_str), "%d%% %u.%02uV",
               data.battery_percent, volts_int, volts_frac);
    }
    lv_label_set_text(battery_label, battery_str);

    // Change color based on battery level
    if (data.battery_percent > 30)
    {
      lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00),
                                  0); // Green
    }
    else if (data.battery_percent > 15)
    {
      lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFF00),
                                  0); // Yellow
    }
    else
    {
      lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFF0000),
                                  0); // Red
    }
  }
  else
  {
    // Fallback display if battery reading fails completely
    lv_label_set_text(battery_label, "? --%%");
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x888888),
                                0); // Gray
    ESP_LOGW(TAG, "Failed to read battery data");
  }

  // Update uptime display
  uptime_stats_t stats;
  if (uptime_tracker_get_stats(&stats) == ESP_OK)
  {
    char uptime_str[32];
    char total_str[32];

    uptime_tracker_format_time(stats.current_uptime_sec, uptime_str,
                               sizeof(uptime_str));
    uptime_tracker_format_time(stats.total_uptime_sec, total_str,
                               sizeof(total_str));

    // Display current session uptime
    lv_label_set_text_fmt(uptime_label, "Up: %s", uptime_str);

    // Display total uptime and boot count
    lv_label_set_text_fmt(boot_count_label, "T%s(B%u)", total_str,
                          stats.boot_count);
  }

#if defined(CONFIG_SLEEP_MANAGER_ENABLE) && defined(CONFIG_SLEEP_MANAGER_SLEEP_INDICATOR)
  if (sleep_indicator_label)
  {
    uint32_t inactive_ms = sleep_manager_get_inactive_time();
    uint32_t timeout_ms = SLEEP_TIMEOUT_MS;
    uint32_t remaining_ms = (inactive_ms >= timeout_ms)
                                ? 0
                                : (timeout_ms - inactive_ms);
    uint32_t remaining_sec = (remaining_ms + 999) / 1000;

    if (remaining_sec <= CONFIG_SLEEP_MANAGER_SLEEP_INDICATOR_THRESHOLD_SECONDS)
    {
      lv_label_set_text_fmt(sleep_indicator_label, "Sleep in %us",
                            remaining_sec);
      lv_obj_clear_flag(sleep_indicator_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_add_flag(sleep_indicator_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
#endif

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

  if (!data_mutex)
  {
    data_mutex = xSemaphoreCreateMutex();
    if (!data_mutex)
    {
      ESP_LOGE(TAG, "Failed to create watchface data mutex");
    }
  }

  if (!data_task_handle)
  {
    BaseType_t task_ret =
        xTaskCreate(watchface_data_task, "watchface_data", 4096, NULL, 4,
                    &data_task_handle);
    if (task_ret != pdPASS)
    {
      ESP_LOGE(TAG, "Failed to create watchface data task");
      data_task_handle = NULL;
    }
  }

  // Use parent tile directly as the screen (no screen_manager needed for tiles)
  screen = parent;
  ESP_LOGI(TAG, "Using parent tile as screen: %p", screen);

  // Build all widgets from configuration table
  for (size_t i = 0; i < WIDGET_COUNT; i++)
  {
    const widget_config_t *config = &widget_configs[i];

    // Create label on the tile
    lv_obj_t *label = lv_label_create(screen);

    // Set size
    lv_obj_set_size(label, config->width, config->height);

    // Apply styling
    lv_obj_set_style_text_font(label, config->font, 0);
    lv_obj_set_style_transform_scale_x(label, 256, 0); // No scaling
    lv_obj_set_style_transform_scale_y(label, 256, 0); // No scaling
    lv_obj_set_style_text_color(label, lv_color_hex(config->color), 0);

    // Set initial text
    lv_label_set_text(label, config->initial_text);

    // Make label transparent to touch events - don't intercept them
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);

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
    case LV_ALIGN_BOTTOM_MID:
      x_offset = 0;
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

#ifdef CONFIG_SLEEP_MANAGER_SLEEP_INDICATOR
  if (sleep_indicator_label)
  {
    lv_obj_add_flag(sleep_indicator_label, LV_OBJ_FLAG_HIDDEN);
  }
#endif

  // Create update timer (1000ms = 1 second)
  update_timer = lv_timer_create(watchface_timer_cb, 1000, NULL);

  // Do initial update immediately
  watchface_timer_cb(NULL);

  ESP_LOGI(TAG, "Watchface created successfully (gestures will be set up after "
                "screen is shown)");
  return screen;
}

static void watchface_data_task(void *param)
{
  (void)param;

  while (1)
  {
    watchface_data_t new_data = {0};

    if (rtc_read_time(&new_data.time) == ESP_OK)
    {
      new_data.time_valid = true;
    }

    esp_err_t battery_ret =
        axp2101_get_battery_data_safe(&new_data.voltage_mv,
                                      &new_data.battery_percent,
                                      &new_data.is_charging);
    if (battery_ret == ESP_OK)
    {
      new_data.battery_valid = true;
    }

    if (data_mutex && xSemaphoreTake(data_mutex, pdMS_TO_TICKS(200)))
    {
      cached_data = new_data;
      xSemaphoreGive(data_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static bool watchface_get_cached_data(watchface_data_t *out)
{
  if (!out || !data_mutex)
  {
    return false;
  }

  if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
  {
    return false;
  }

  *out = cached_data;
  xSemaphoreGive(data_mutex);
  return true;
}

void watchface_update(void)
{
  // Force immediate update
  if (update_timer)
  {
    lv_timer_ready(update_timer);
  }
}

lv_timer_t *watchface_get_timer(void) { return update_timer; }