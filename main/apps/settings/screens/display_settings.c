/**
 * @file display_settings.c
 * @brief Display Settings Screen Implementation
 */

#include "display_settings.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "settings_storage.h"

static const char *TAG = "DisplaySettings";

// UI elements
static lv_obj_t *display_settings_screen = NULL;
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_label = NULL;
#ifdef CONFIG_SLEEP_MANAGER_ENABLE
static lv_obj_t *sleep_timeout_dropdown = NULL;
#endif

// Current brightness value (0-100)
static int32_t current_brightness = SETTING_DEFAULT_BRIGHTNESS;

/**
 * @brief Apply brightness to display
 */
static void apply_brightness(int32_t brightness)
{
  // Clamp brightness to valid range
  if (brightness < 0)
    brightness = 0;
  if (brightness > 100)
    brightness = 100;

  current_brightness = brightness;

  // Convert percentage to BSP backlight level (0-100)
  esp_err_t ret = bsp_display_brightness_set(brightness);
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to set brightness: %s", esp_err_to_name(ret));
  }
  else
  {
    ESP_LOGD(TAG, "Brightness set to %d%%", (int)brightness);
  }
}

/**
 * @brief Brightness slider event handler
 */
static void brightness_slider_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED)
  {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    // Update label
    lv_label_set_text_fmt(brightness_label, "Brightness: %d%%", (int)value);

    // Apply brightness in real-time
    apply_brightness(value);

    // Save to NVS
    esp_err_t ret = settings_set_int(SETTING_KEY_BRIGHTNESS, value);
    if (ret != ESP_OK)
    {
      ESP_LOGW(TAG, "Failed to save brightness: %s", esp_err_to_name(ret));
    }
  }
}

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
/**
 * @brief Sleep timeout dropdown event handler
 */
static void sleep_timeout_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED)
  {
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);

    // Map selection to timeout values
    int32_t timeout_values[] = {5, 10, 15, 30, 60, 120, 300};
    int32_t timeout = timeout_values[selected];

    ESP_LOGI(TAG, "Sleep timeout set to %d seconds", (int)timeout);

    // Save to NVS
    esp_err_t ret = settings_set_int(SETTING_KEY_SLEEP_TIMEOUT, timeout);
    if (ret != ESP_OK)
    {
      ESP_LOGW(TAG, "Failed to save sleep timeout: %s", esp_err_to_name(ret));
    }
  }
}
#endif // CONFIG_SLEEP_MANAGER_ENABLE

lv_obj_t *display_settings_create(lv_obj_t *parent)
{
  // Return existing screen if already created
  if (display_settings_screen)
  {
    ESP_LOGI(TAG, "Display settings screen already exists, returning existing");
    return display_settings_screen;
  }

  ESP_LOGI(TAG, "Creating display settings screen");

  // Initialize settings storage if not already done
  settings_storage_init();

  // Load current brightness from NVS
  settings_get_int(SETTING_KEY_BRIGHTNESS, SETTING_DEFAULT_BRIGHTNESS,
                   &current_brightness);

  // Create screen using screen_manager
  screen_config_t config = {
      .title = "Display",
      .show_back_button = true,
      .anim_type = SCREEN_ANIM_HORIZONTAL,
      .hide_callback = display_settings_hide,
  };

  display_settings_screen = screen_manager_create(&config);
  if (!display_settings_screen)
  {
    ESP_LOGE(TAG, "Failed to create display settings screen");
    return NULL;
  }

  // Brightness section
  // Brightness label with current value
  brightness_label = lv_label_create(display_settings_screen);
  lv_label_set_text_fmt(brightness_label, "Brightness: %d%%",
                        (int)current_brightness);
  lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
  lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, SAFE_AREA_HORIZONTAL, SAFE_AREA_TOP + 40);

  // Brightness slider
  brightness_slider = lv_slider_create(display_settings_screen);
  lv_obj_set_size(brightness_slider, LV_PCT(80), 20);
  lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP + 70);
  lv_slider_set_range(brightness_slider, 0, 100);
  lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
  // Sleep timeout section
  lv_obj_t *timeout_label = lv_label_create(display_settings_screen);
  lv_label_set_text(timeout_label, "Sleep Timeout:");
  lv_obj_set_style_text_font(timeout_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(timeout_label, lv_color_white(), 0);
  lv_obj_align(timeout_label, LV_ALIGN_TOP_LEFT, SAFE_AREA_HORIZONTAL, SAFE_AREA_TOP + 120);

  // Sleep timeout dropdown
  sleep_timeout_dropdown = lv_dropdown_create(display_settings_screen);
  lv_dropdown_set_options(sleep_timeout_dropdown,
                          "5 sec\n10 sec\n15 sec\n30 sec\n1 min\n2 min\n5 min");
  lv_obj_set_size(sleep_timeout_dropdown, LV_PCT(70), 40);
  lv_obj_align(sleep_timeout_dropdown, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP + 150);
  lv_obj_set_style_text_font(sleep_timeout_dropdown, &lv_font_montserrat_14, 0);
  lv_obj_add_event_cb(sleep_timeout_dropdown, sleep_timeout_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  // Load and set current sleep timeout
  int32_t sleep_timeout;
  settings_get_int(SETTING_KEY_SLEEP_TIMEOUT, SETTING_DEFAULT_SLEEP_TIMEOUT,
                   &sleep_timeout);

  // Map timeout to dropdown index
  uint16_t dropdown_index = 3; // Default to 30 sec
  if (sleep_timeout <= 5)
    dropdown_index = 0;
  else if (sleep_timeout <= 10)
    dropdown_index = 1;
  else if (sleep_timeout <= 15)
    dropdown_index = 2;
  else if (sleep_timeout <= 30)
    dropdown_index = 3;
  else if (sleep_timeout <= 60)
    dropdown_index = 4;
  else if (sleep_timeout <= 120)
    dropdown_index = 5;
  else
    dropdown_index = 6;

  lv_dropdown_set_selected(sleep_timeout_dropdown, dropdown_index);
#endif // CONFIG_SLEEP_MANAGER_ENABLE

  ESP_LOGI(TAG, "Display settings screen created");
  return display_settings_screen;
}

void display_settings_show(void)
{
  if (display_settings_screen)
  {
    ESP_LOGI(TAG, "Showing display settings screen");
    bsp_display_lock(0);
    screen_manager_show(display_settings_screen);
    bsp_display_unlock();
  }
  else
  {
    ESP_LOGW(TAG, "Display settings screen not created");
  }
}

void display_settings_hide(void)
{
  ESP_LOGI(TAG, "Hiding display settings screen");
  // Cleanup only - screen_manager_go_back() is already handling navigation
}
