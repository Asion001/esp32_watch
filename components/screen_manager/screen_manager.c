/**
 * @file screen_manager.c
 * @brief Unified screen management implementation
 */

#include "screen_manager.h"
#include "safe_area.h"
#include "esp_log.h"
#include "screen_navigation.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ScreenManager";

/**
 * @brief Screen metadata stored in user_data
 */
typedef struct
{
  screen_anim_type_t anim_type; /*!< Animation type for this screen */
  void (*hide_callback)(void);  /*!< Hide callback for this screen */
} screen_metadata_t;

/**
 * @brief Screen manager state
 */
typedef struct
{
  lv_obj_t *current_screen;  /*!< Currently active screen */
  lv_obj_t *previous_screen; /*!< Previous screen for back navigation */
  bool initialized;          /*!< Initialization flag */
} screen_manager_state_t;

static screen_manager_state_t s_manager = {.current_screen = NULL,
                                           .previous_screen = NULL,
                                           .initialized = false};

/**
 * @brief Create title label
 */
static void create_title(lv_obj_t *parent, const char *title_text)
{
  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, title_text);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_AREA_TOP);
}

/**
 * @brief Back button event callback
 */
static void back_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    screen_manager_go_back();
  }
}

/**
 * @brief Create back button that calls screen_manager_go_back()
 */
static void create_back_button(lv_obj_t *parent)
{
  // Create back button at top-left within safe area
  lv_obj_t *back_btn = lv_btn_create(parent);
  lv_obj_set_size(back_btn, 50, 40);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, SAFE_AREA_HORIZONTAL, SAFE_AREA_TOP);
  lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

  // Ensure button stays on top and is clickable
  lv_obj_move_foreground(back_btn);
  lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);

  // Add left arrow symbol
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
}

esp_err_t screen_manager_init(void)
{
  if (s_manager.initialized)
  {
    ESP_LOGW(TAG, "Screen manager already initialized");
    return ESP_OK;
  }

  s_manager.current_screen = NULL;
  s_manager.previous_screen = NULL;
  s_manager.initialized = true;

  ESP_LOGI(TAG, "Screen manager initialized");
  return ESP_OK;
}

lv_obj_t *screen_manager_create(const screen_config_t *config)
{
  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return NULL;
  }

  if (!config)
  {
    ESP_LOGE(TAG, "Invalid config (NULL)");
    return NULL;
  }

  // Create screen container
  lv_obj_t *screen = lv_obj_create(NULL);
  if (!screen)
  {
    ESP_LOGE(TAG, "Failed to create screen object");
    return NULL;
  }

  // Allocate and store metadata in user_data
  screen_metadata_t *metadata = malloc(sizeof(screen_metadata_t));
  if (!metadata)
  {
    ESP_LOGE(TAG, "Failed to allocate screen metadata");
    lv_obj_del(screen);
    return NULL;
  }
  metadata->anim_type = config->anim_type;
  metadata->hide_callback = config->hide_callback;
  lv_obj_set_user_data(screen, metadata);

  // Style as black container with no border/padding
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_border_width(screen, 0, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  // Enable scrollbar and vertical scrolling for content
  // This allows child containers to scroll vertically while horizontal gestures
  // are captured by the parent screen
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(screen, LV_DIR_VER);

  // Add title if provided
  if (config->title)
  {
    create_title(screen, config->title);
  }

  // Add back button if requested
  if (config->show_back_button)
  {
    create_back_button(screen);
  }

  // Setup gestures based on animation type
  // Note: Gesture direction should match the swipe direction to close
  // - Vertical screens slide UP to open (MOVE_BOTTOM), so swipe DOWN to close
  // (DIR_TOP)
  // - Horizontal screens slide RIGHT to open (MOVE_LEFT), so swipe LEFT to
  // close (DIR_LEFT)
  if (config->anim_type != SCREEN_ANIM_NONE && config->hide_callback)
  {
    lv_dir_t gesture_dir =
        (config->anim_type == SCREEN_ANIM_VERTICAL) ? LV_DIR_TOP : LV_DIR_LEFT;
    screen_nav_setup_gestures(screen, config->hide_callback, gesture_dir);
  }

  // NOTE: Do NOT update manager state here - only screen_manager_show() should
  // update the navigation stack to avoid overwriting previous_screen incorrectly

  ESP_LOGI(TAG, "Created screen: title='%s', anim=%d",
           config->title ? config->title : "none", config->anim_type);

  return screen;
}

esp_err_t screen_manager_show(lv_obj_t *screen)
{
  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  if (!screen)
  {
    ESP_LOGE(TAG, "Invalid screen (NULL)");
    return ESP_ERR_INVALID_ARG;
  }

  // Update navigation stack: store current screen as previous before switching
  if (s_manager.current_screen != screen)
  {
    s_manager.previous_screen = s_manager.current_screen;
    s_manager.current_screen = screen;
  }

  // Get metadata from screen
  screen_metadata_t *metadata = (screen_metadata_t *)lv_obj_get_user_data(screen);
  if (!metadata)
  {
    ESP_LOGW(TAG, "Screen has no metadata, using defaults");
    lv_scr_load(screen);
    return ESP_OK;
  }

  // Use stored animation type from metadata
  screen_anim_type_t anim_type = metadata->anim_type;

  // Load screen with appropriate animation
  if (anim_type == SCREEN_ANIM_VERTICAL)
  {
    screen_nav_load_with_anim(screen, NULL); // Vertical (bottom-up)
  }
  else if (anim_type == SCREEN_ANIM_HORIZONTAL)
  {
    screen_nav_load_horizontal(screen, NULL); // Horizontal (right-to-left)
  }
  else
  {
    // No animation - instant load
    lv_scr_load(screen);
  }

  ESP_LOGD(TAG, "Showed screen with animation type %d", anim_type);
  return ESP_OK;
}

esp_err_t screen_manager_go_back(void)
{
  ESP_LOGI(TAG, "go_back() called");

  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  // Check if there's a previous screen to go back to
  if (!s_manager.previous_screen)
  {
    ESP_LOGI(TAG, "No previous screen, cannot go back");
    return ESP_OK; // Not an error, just no-op
  }

  lv_obj_t *current = s_manager.current_screen;
  lv_obj_t *previous = s_manager.previous_screen;

  ESP_LOGI(TAG, "Going back: current=%p, previous=%p", current, previous);

  // Get metadata from current screen
  screen_metadata_t *metadata = (screen_metadata_t *)lv_obj_get_user_data(current);
  screen_anim_type_t anim_type = SCREEN_ANIM_NONE;
  void (*hide_callback)(void) = NULL;

  if (metadata)
  {
    anim_type = metadata->anim_type;
    hide_callback = metadata->hide_callback;
    ESP_LOGI(TAG, "Metadata found, anim_type=%d", anim_type);
  }

  // Call hide callback if provided
  if (hide_callback)
  {
    ESP_LOGI(TAG, "Calling hide callback");
    hide_callback();
  }

  // Animate back to previous screen
  ESP_LOGI(TAG, "Animating back with anim_type=%d", anim_type);
  if (anim_type == SCREEN_ANIM_VERTICAL)
  {
    screen_nav_go_back_with_anim(current, previous); // Vertical (top-down)
  }
  else if (anim_type == SCREEN_ANIM_HORIZONTAL)
  {
    screen_nav_go_back_horizontal(current,
                                  previous); // Horizontal (left-to-right)
  }
  else
  {
    lv_scr_load(previous);
  }

  // Update state - now on previous screen
  s_manager.current_screen = previous;
  s_manager.previous_screen =
      NULL; // Clear previous (simple two-level navigation)

  ESP_LOGI(TAG, "go_back() complete");
  return ESP_OK;
}

esp_err_t screen_manager_destroy(lv_obj_t *screen)
{
  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  if (!screen)
  {
    ESP_LOGE(TAG, "Invalid screen (NULL)");
    return ESP_ERR_INVALID_ARG;
  }

  // Clear references if destroying current or previous screen
  if (s_manager.current_screen == screen)
  {
    s_manager.current_screen = NULL;
  }
  if (s_manager.previous_screen == screen)
  {
    s_manager.previous_screen = NULL;
  }

  // Free metadata if present
  screen_metadata_t *metadata = (screen_metadata_t *)lv_obj_get_user_data(screen);
  if (metadata)
  {
    free(metadata);
    lv_obj_set_user_data(screen, NULL);
  }

  // Cleanup gestures
  screen_nav_cleanup_gestures(screen);

  // Delete LVGL object (will trigger LV_EVENT_DELETE for any remaining cleanup)
  lv_obj_delete(screen);

  ESP_LOGD(TAG, "Destroyed screen");
  return ESP_OK;
}

lv_obj_t *screen_manager_get_current(void)
{
  if (!s_manager.initialized)
  {
    return NULL;
  }
  return s_manager.current_screen;
}

bool screen_manager_is_root(lv_obj_t *screen)
{
  if (!s_manager.initialized || !screen)
  {
    return false;
  }

  // Root screen is the one with no previous screen
  return (s_manager.current_screen == screen &&
          s_manager.previous_screen == NULL);
}
