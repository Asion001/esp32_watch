/**
 * @file settings.c
 * @brief Settings Application Implementation
 */

#include "settings.h"
#include "screens/display_settings.h"
#include "screens/about_screen.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include <string.h>

static const char *TAG = "Settings";

// UI elements
static lv_obj_t *settings_screen = NULL;
static lv_obj_t *main_menu_list = NULL;
static lv_obj_t *watchface_screen = NULL;

/**
 * @brief Back button event handler
 */
static void back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "Back button clicked, returning to watchface");
        settings_hide();
    }
}

/**
 * @brief Menu item click handler
 */
static void menu_item_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t *item = lv_event_get_target(e);
        const char *text = lv_list_get_btn_text(main_menu_list, item);
        ESP_LOGI(TAG, "Menu item clicked: %s", text ? text : "Unknown");
        
        // Navigate to specific settings screens
        if (text && strcmp(text, "Display") == 0)
        {
            bsp_display_lock(0);
            display_settings_show();
            bsp_display_unlock();
        }
        else if (text && strcmp(text, "Time & Sync") == 0)
        {
            ESP_LOGI(TAG, "Time & Sync settings - not yet implemented");
            // TODO: Navigate to time settings
        }
        else if (text && strcmp(text, "WiFi") == 0)
        {
            ESP_LOGI(TAG, "WiFi settings - not yet implemented");
            // TODO: Navigate to WiFi settings
        }
        else if (text && strcmp(text, "About") == 0)
        {
            bsp_display_lock(0);
            about_screen_show();
            bsp_display_unlock();
        }
    }
}

/**
 * @brief Create the main settings menu
 */
static void create_main_menu(lv_obj_t *parent)
{
    // Create title label
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Create back button
    lv_obj_t *back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 60, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);

    // Create menu list
    main_menu_list = lv_list_create(parent);
    lv_obj_set_size(main_menu_list, LV_PCT(90), LV_PCT(70));
    lv_obj_align(main_menu_list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(main_menu_list, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(main_menu_list, 1, 0);
    lv_obj_set_style_border_color(main_menu_list, lv_color_hex(0x444444), 0);

    // Add menu items with larger size for easier touch
    lv_obj_t *item;
    
    // Display settings
    item = lv_list_add_btn(main_menu_list, LV_SYMBOL_EYE_OPEN, "Display");
    lv_obj_add_event_cb(item, menu_item_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_text_font(item, &lv_font_montserrat_20, 0);
    lv_obj_set_height(item, 60);  // Larger height for easier touch
    
    // Time settings
    item = lv_list_add_btn(main_menu_list, LV_SYMBOL_REFRESH, "Time & Sync");
    lv_obj_add_event_cb(item, menu_item_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_text_font(item, &lv_font_montserrat_20, 0);
    lv_obj_set_height(item, 60);
    
    // WiFi settings
    item = lv_list_add_btn(main_menu_list, LV_SYMBOL_WIFI, "WiFi");
    lv_obj_add_event_cb(item, menu_item_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_text_font(item, &lv_font_montserrat_20, 0);
    lv_obj_set_height(item, 60);
    
    // About / System info
    item = lv_list_add_btn(main_menu_list, LV_SYMBOL_SETTINGS, "About");
    lv_obj_add_event_cb(item, menu_item_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_text_font(item, &lv_font_montserrat_20, 0);
    lv_obj_set_height(item, 60);

    ESP_LOGI(TAG, "Main menu created");
}

lv_obj_t *settings_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating settings screen");

    // Create settings screen container
    settings_screen = lv_obj_create(parent);
    lv_obj_set_size(settings_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(settings_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(settings_screen, 0, 0);
    lv_obj_set_style_pad_all(settings_screen, 0, 0);
    
    // Hide by default (start on watchface)
    lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);

    // Create main menu
    create_main_menu(settings_screen);

    ESP_LOGI(TAG, "Settings screen created");
    return settings_screen;
}

void settings_show(void)
{
    if (settings_screen)
    {
        ESP_LOGI(TAG, "Showing settings screen");
        
        // Save reference to watchface screen
        watchface_screen = lv_scr_act();
        
        // Show settings screen
        lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(settings_screen);
    }
    else
    {
        ESP_LOGW(TAG, "Settings screen not created");
    }
}

void settings_hide(void)
{
    if (settings_screen && watchface_screen)
    {
        ESP_LOGI(TAG, "Hiding settings screen");
        
        // Return to watchface
        lv_scr_load(watchface_screen);
        lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        ESP_LOGW(TAG, "Cannot hide settings: screen refs invalid");
    }
}
