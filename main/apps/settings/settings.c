/**
 * @file settings.c
 * @brief Settings Application Implementation
 */

#include "settings.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "screen_manager.h"
#include "screens/about_screen.h"
#include "screens/display_settings.h"
#include "screens/system_settings.h"
#include "screens/time_sync.h"
#ifdef CONFIG_ENABLE_WIFI
#include "screens/wifi_settings.h"
#endif
#include <string.h>

static const char *TAG = "Settings";

// UI elements
static lv_obj_t *settings_screen = NULL;
static lv_obj_t *main_menu_list = NULL;
static lv_obj_t *tileview = NULL; // Reference to main tileview for navigation

// Forward declarations
static void menu_item_event_cb(lv_event_t *e);

/**
 * @brief Helper function to create a menu item with consistent styling
 */
static void add_menu_item(const char *icon, const char *text)
{
  lv_obj_t *item = lv_list_add_btn(main_menu_list, icon, text);
  lv_obj_add_event_cb(item, menu_item_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_style_text_font(item, &lv_font_montserrat_20, 0);
  lv_obj_set_height(item, 60);
}

/**
 * @brief Menu item click handler
 */
static void menu_item_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code != LV_EVENT_CLICKED)
  {
    return;
  }

  lv_obj_t *item = lv_event_get_target(e);
  const char *text = lv_list_get_btn_text(main_menu_list, item);

  // Early return if text is NULL for safety
  if (!text)
  {
    ESP_LOGW(TAG, "Menu item clicked but text is NULL");
    return;
  }

  ESP_LOGI(TAG, "Menu item clicked: %s", text);

  // Navigate to specific settings screens
  // Create screens on-demand to avoid navigation stack corruption
  if (strcmp(text, "Display") == 0)
  {
    display_settings_create(settings_screen);
    display_settings_show();
  }
  else if (strcmp(text, "System") == 0)
  {
    system_settings_create(settings_screen);
    system_settings_show();
  }
  else if (strcmp(text, "Time & Sync") == 0)
  {
#ifdef CONFIG_NTP_CLIENT_ENABLE
    time_sync_create(settings_screen);
    time_sync_show();
#else
    ESP_LOGI(TAG, "Time & Sync disabled in menuconfig");
#endif
  }
  else if (strcmp(text, "WiFi") == 0)
  {
#ifdef CONFIG_ENABLE_WIFI
    wifi_settings_create(settings_screen);
    wifi_settings_show();
#else
    ESP_LOGI(TAG, "WiFi disabled in menuconfig");
#endif
  }
  else if (strcmp(text, "About") == 0)
  {
    bsp_display_lock(0);
    about_screen_create(settings_screen);
    about_screen_show();
    bsp_display_unlock();
  }
}

/**
 * @brief Create the main settings menu
 */
static void create_main_menu(lv_obj_t *parent)
{
  // Create menu list
  main_menu_list = lv_list_create(parent);
  lv_obj_set_size(main_menu_list, LV_PCT(90), LV_PCT(70));
  lv_obj_align(main_menu_list, LV_ALIGN_CENTER, 0, 20);
  lv_obj_set_style_bg_color(main_menu_list, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(main_menu_list, 1, 0);
  lv_obj_set_style_border_color(main_menu_list, lv_color_hex(0x444444), 0);

  // Add menu items
  add_menu_item(LV_SYMBOL_EYE_OPEN, "Display");
  add_menu_item(LV_SYMBOL_SETTINGS, "System");
  add_menu_item(LV_SYMBOL_REFRESH, "Time & Sync");
#ifdef CONFIG_ENABLE_WIFI
  add_menu_item(LV_SYMBOL_WIFI, "WiFi");
#endif
  add_menu_item(LV_SYMBOL_LIST, "About");

  ESP_LOGI(TAG, "Main menu created");
}

lv_obj_t *settings_create(lv_obj_t *parent)
{
  ESP_LOGI(TAG, "Creating settings on tile");

  // Use parent tile directly
  settings_screen = parent;

  // Add title label at top
  lv_obj_t *title = lv_label_create(settings_screen);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  // Create main menu
  create_main_menu(settings_screen);

  ESP_LOGI(TAG, "Settings created on tile");
  return settings_screen;
}

void settings_show(void)
{
  if (settings_screen && tileview)
  {
    ESP_LOGI(TAG, "Navigating to settings tile");
    bsp_display_lock(0);
    lv_tileview_set_tile_by_index(tileview, 0, 1, LV_ANIM_ON);
    bsp_display_unlock();
  }
  else
  {
    ESP_LOGW(TAG, "Settings tile or tileview not set");
  }
}

void settings_hide(void)
{
  if (tileview)
  {
    ESP_LOGI(TAG, "Returning to watchface tile");
    bsp_display_lock(0);
    lv_tileview_set_tile_by_index(tileview, 0, 0, LV_ANIM_ON);
    bsp_display_unlock();
  }
}

lv_obj_t *settings_get_screen(void) { return settings_screen; }

void settings_set_tileview(lv_obj_t *tv)
{
  tileview = tv;
  ESP_LOGI(TAG, "Tileview reference set: %p", tileview);
}
