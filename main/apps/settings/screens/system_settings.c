/**
 * @file system_settings.c
 * @brief System Settings Screen Implementation
 */

#include "system_settings.h"
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "safe_area.h"
#include "screen_manager.h"
#include "settings_storage.h"
#include "uptime_tracker.h"

static const char *TAG = "SystemSettings";

// Helper to parse flash size from config string
static uint32_t get_flash_size_mb(void)
{
  const char *flash_size = CONFIG_ESPTOOLPY_FLASHSIZE;
  uint32_t size_mb = 0;
  sscanf(flash_size, "%uMB", &size_mb);
  return size_mb;
}

// UI elements
static lv_obj_t *system_settings_screen = NULL;
static lv_obj_t *confirmation_msgbox = NULL;

/**
 * @brief Factory reset yes button callback
 */
static void factory_reset_yes_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    ESP_LOGI(TAG, "User confirmed factory reset");

    // Perform factory reset
    esp_err_t ret = settings_erase_all();
    if (ret == ESP_OK)
    {
      ESP_LOGI(TAG, "Settings erased successfully");

      // Also reset uptime
      uptime_tracker_reset();

      // Show success message
      lv_obj_t *success_msgbox = lv_msgbox_create(lv_layer_top());
      lv_msgbox_add_title(success_msgbox, "Success");
      lv_msgbox_add_text(success_msgbox,
                         "All settings cleared.\nDevice will restart.");
      lv_msgbox_add_close_button(success_msgbox);
      lv_obj_center(success_msgbox);

      // Schedule restart after 3 seconds
      ESP_LOGI(TAG, "Restarting in 3 seconds...");
      vTaskDelay(pdMS_TO_TICKS(3000));
      esp_restart();
    }
    else
    {
      ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(ret));

      // Show error message
      lv_obj_t *error_msgbox = lv_msgbox_create(lv_layer_top());
      lv_msgbox_add_title(error_msgbox, "Error");
      lv_msgbox_add_text(error_msgbox, "Failed to reset settings.");
      lv_msgbox_add_close_button(error_msgbox);
      lv_obj_center(error_msgbox);
    }

    // Close confirmation dialog
    if (confirmation_msgbox)
    {
      lv_msgbox_close(confirmation_msgbox);
      confirmation_msgbox = NULL;
    }
  }
}

/**
 * @brief Factory reset no button callback
 */
static void factory_reset_no_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    ESP_LOGI(TAG, "User cancelled factory reset");

    // Close confirmation dialog
    if (confirmation_msgbox)
    {
      lv_msgbox_close(confirmation_msgbox);
      confirmation_msgbox = NULL;
    }
  }
}

/**
 * @brief Factory reset button callback
 */
static void factory_reset_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    ESP_LOGI(TAG, "Factory reset button clicked");

    // Create confirmation dialog
    confirmation_msgbox = lv_msgbox_create(lv_layer_top());
    lv_msgbox_add_title(confirmation_msgbox, "Factory Reset");
    lv_msgbox_add_text(confirmation_msgbox, "This will erase ALL settings:\n"
                                            "- Display settings\n"
                                            "- WiFi credentials\n"
                                            "- Uptime data\n\n"
                                            "Device will restart.\n\n"
                                            "Continue?");

    lv_obj_t *yes_btn = lv_msgbox_add_footer_button(confirmation_msgbox, "Yes");
    lv_obj_t *no_btn = lv_msgbox_add_footer_button(confirmation_msgbox, "No");

    lv_obj_add_event_cb(yes_btn, factory_reset_yes_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(no_btn, factory_reset_no_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_style_bg_color(yes_btn, lv_color_hex(0xFF0000), 0);
    lv_obj_center(confirmation_msgbox);
  }
}

/**
 * @brief Yes button callback
 */
static void confirm_yes_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    ESP_LOGI(TAG, "User confirmed uptime reset");

    // Reset uptime
    esp_err_t ret = uptime_tracker_reset();
    if (ret == ESP_OK)
    {
      ESP_LOGI(TAG, "Uptime reset successful");

      // Show success message
      lv_obj_t *success_msgbox = lv_msgbox_create(lv_layer_top());
      lv_msgbox_add_title(success_msgbox, "Success");
      lv_msgbox_add_text(success_msgbox, "Uptime counter has been reset.");
      lv_msgbox_add_close_button(success_msgbox);
      lv_obj_center(success_msgbox);
    }
    else
    {
      ESP_LOGE(TAG, "Uptime reset failed: %s", esp_err_to_name(ret));

      // Show error message
      lv_obj_t *error_msgbox = lv_msgbox_create(lv_layer_top());
      lv_msgbox_add_title(error_msgbox, "Error");
      lv_msgbox_add_text(error_msgbox, "Failed to reset uptime.");
      lv_msgbox_add_close_button(error_msgbox);
      lv_obj_center(error_msgbox);
    }

    // Close confirmation dialog
    if (confirmation_msgbox)
    {
      lv_msgbox_close(confirmation_msgbox);
      confirmation_msgbox = NULL;
    }
  }
}

/**
 * @brief No button callback
 */
static void confirm_no_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    ESP_LOGI(TAG, "User cancelled uptime reset");

    // Close confirmation dialog
    if (confirmation_msgbox)
    {
      lv_msgbox_close(confirmation_msgbox);
      confirmation_msgbox = NULL;
    }
  }
}

/**
 * @brief Reset uptime button callback
 */
static void reset_uptime_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_CLICKED)
  {
    ESP_LOGI(TAG, "Reset uptime button clicked");

    // Create confirmation dialog
    confirmation_msgbox = lv_msgbox_create(lv_layer_top());
    lv_msgbox_add_title(confirmation_msgbox, "Confirm Reset");
    lv_msgbox_add_text(
        confirmation_msgbox,
        "Reset uptime counter and boot count?\n\nThis cannot be undone.");

    // Add Yes and No buttons
    lv_obj_t *yes_btn = lv_msgbox_add_footer_button(confirmation_msgbox, "Yes");
    lv_obj_t *no_btn = lv_msgbox_add_footer_button(confirmation_msgbox, "No");

    // Add event callbacks to buttons
    lv_obj_add_event_cb(yes_btn, confirm_yes_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(no_btn, confirm_no_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_center(confirmation_msgbox);
  }
}

/**
 * @brief Create system settings UI elements
 */
static void create_system_settings_ui(lv_obj_t *parent)
{
  // Create container for buttons
  lv_obj_t *container = lv_obj_create(parent);
  lv_obj_set_size(container, LV_PCT(90), LV_PCT(70));
  lv_obj_align(container, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP + 30);
  lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(container, 1, 0);
  lv_obj_set_style_border_color(container, lv_color_hex(0x444444), 0);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(container, 10, 0);
  lv_obj_set_style_pad_all(container, 20, 0);

  // Reset Uptime Button
  lv_obj_t *reset_uptime_btn = lv_btn_create(container);
  lv_obj_set_size(reset_uptime_btn, LV_PCT(90), 50);
  lv_obj_set_style_bg_color(reset_uptime_btn, lv_color_hex(0xFF6600), 0);
  lv_obj_add_event_cb(reset_uptime_btn, reset_uptime_cb, LV_EVENT_CLICKED,
                      NULL);

  lv_obj_t *reset_label = lv_label_create(reset_uptime_btn);
  lv_label_set_text(reset_label, LV_SYMBOL_REFRESH " Reset Uptime");
  lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_18, 0);
  lv_obj_center(reset_label);

  // Description label
  lv_obj_t *desc_label = lv_label_create(container);
  lv_label_set_text(desc_label, "Reset uptime counter and boot count.\n"
                                "Useful for battery tests.");
  lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(desc_label, lv_color_hex(0x888888), 0);
  lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(desc_label, LV_PCT(85));

  // Factory Reset Button
  lv_obj_t *factory_reset_btn = lv_btn_create(container);
  lv_obj_set_size(factory_reset_btn, LV_PCT(90), 50);
  lv_obj_set_style_bg_color(factory_reset_btn, lv_color_hex(0xFF0000), 0);
  lv_obj_add_event_cb(factory_reset_btn, factory_reset_cb, LV_EVENT_CLICKED,
                      NULL);

  lv_obj_t *factory_label = lv_label_create(factory_reset_btn);
  lv_label_set_text(factory_label, LV_SYMBOL_TRASH " Factory Reset");
  lv_obj_set_style_text_font(factory_label, &lv_font_montserrat_18, 0);
  lv_obj_center(factory_label);

  // Factory reset description
  lv_obj_t *factory_desc = lv_label_create(container);
  lv_label_set_text(factory_desc, "Erase all settings and restart.\n"
                                  "Use with caution!");
  lv_obj_set_style_text_font(factory_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(factory_desc, lv_color_hex(0xFF8888), 0);
  lv_label_set_long_mode(factory_desc, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(factory_desc, LV_PCT(85));

  // Storage Information Section
  lv_obj_t *storage_title = lv_label_create(container);
  lv_label_set_text(storage_title, "\nStorage Information:");
  lv_obj_set_style_text_font(storage_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(storage_title, lv_color_white(), 0);

  // Get RAM info
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

  // Get flash info
  uint32_t flash_size_mb = get_flash_size_mb();

  // Create storage info text
  char storage_info[256];
  snprintf(storage_info, sizeof(storage_info),
           "RAM Free: %u KB / %u KB\n"
           "RAM Min Free: %u KB\n"
           "Flash: %u MB",
           (unsigned)(free_heap / 1024), (unsigned)(total_heap / 1024),
           (unsigned)(min_free_heap / 1024), flash_size_mb);

  lv_obj_t *storage_info_label = lv_label_create(container);
  lv_label_set_text(storage_info_label, storage_info);
  lv_obj_set_style_text_font(storage_info_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(storage_info_label, lv_color_hex(0x888888), 0);
  lv_label_set_long_mode(storage_info_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(storage_info_label, LV_PCT(85));

  ESP_LOGI(TAG, "System settings UI created");
}

lv_obj_t *system_settings_create(lv_obj_t *parent)
{
  // Return existing screen if already created
  if (system_settings_screen)
  {
    ESP_LOGI(TAG, "System settings screen already exists, returning existing");
    return system_settings_screen;
  }

  ESP_LOGI(TAG, "Creating system settings screen");

  // Create screen using screen_manager
  screen_config_t config = {
      .title = "System",
      .show_back_button = true,
      .anim_type = SCREEN_ANIM_HORIZONTAL,
      .hide_callback = system_settings_hide,
  };

  system_settings_screen = screen_manager_create(&config);
  if (!system_settings_screen)
  {
    ESP_LOGE(TAG, "Failed to create system settings screen");
    return NULL;
  }

  // Create UI elements
  create_system_settings_ui(system_settings_screen);

  ESP_LOGI(TAG, "System settings screen created");
  return system_settings_screen;
}

void system_settings_show(void)
{
  if (system_settings_screen)
  {
    ESP_LOGI(TAG, "Showing system settings screen");
    screen_manager_show(system_settings_screen);
  }
  else
  {
    ESP_LOGW(TAG, "System settings screen not created");
  }
}

void system_settings_hide(void)
{
  ESP_LOGI(TAG, "Hiding system settings screen");
  // Cleanup only - screen_manager_go_back() is already handling navigation
}
