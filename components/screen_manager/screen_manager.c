/**
 * @file screen_manager.c
 * @brief Unified screen management implementation with proper navigation stack
 *
 * Rewritten to use a stack-based navigation model with auto-delete,
 * following patterns from LVGL demos for stable back button handling.
 */

#include "screen_manager.h"
#include "safe_area.h"
#include "esp_log.h"
#include "screen_navigation.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ScreenManager";

/** Maximum navigation stack depth */
#define SCREEN_STACK_MAX_DEPTH 8

/**
 * @brief Screen metadata stored in user_data
 */
typedef struct
{
  screen_anim_type_t anim_type; /*!< Animation type for this screen */
  void (*hide_callback)(void);  /*!< Hide callback for this screen */
  bool auto_delete;             /*!< Auto-delete when popped from stack */
} screen_metadata_t;

/**
 * @brief Screen manager state with navigation stack
 */
typedef struct
{
  lv_obj_t *stack[SCREEN_STACK_MAX_DEPTH]; /*!< Screen navigation stack */
  int depth;                               /*!< Current stack depth (0 = empty) */
  bool initialized;                        /*!< Initialization flag */
  bool transition_in_progress;             /*!< Prevent re-entrant navigation */
} screen_manager_state_t;

static screen_manager_state_t s_manager = {
    .depth = 0,
    .initialized = false,
    .transition_in_progress = false};

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
    // Prevent double-clicks during transition
    if (s_manager.transition_in_progress)
    {
      ESP_LOGD(TAG, "Back button ignored - transition in progress");
      return;
    }
    ESP_LOGI(TAG, "Back button clicked");
    screen_manager_go_back();
  }
}

/**
 * @brief Create back button that calls screen_manager_go_back()
 *
 * Uses LV_OBJ_FLAG_FLOATING to keep button fixed during scroll.
 */
static void create_back_button(lv_obj_t *parent)
{
  // Create back button at top-left within safe area
  lv_obj_t *back_btn = lv_btn_create(parent);
  lv_obj_set_size(back_btn, 50, 40);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, SAFE_AREA_HORIZONTAL, SAFE_AREA_TOP);
  lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

  // Critical: Make button floating to stay fixed during scroll
  lv_obj_add_flag(back_btn, LV_OBJ_FLAG_FLOATING);
  lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);

  // Ensure button stays on top
  lv_obj_move_foreground(back_btn);

  // Add left arrow symbol
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  ESP_LOGD(TAG, "Created floating back button");
}

/**
 * @brief Wrapper for screen_manager_go_back to match screen_hide_cb_t signature
 */
static void go_back_wrapper(void)
{
  screen_manager_go_back();
}

esp_err_t screen_manager_init(void)
{
  if (s_manager.initialized)
  {
    ESP_LOGW(TAG, "Screen manager already initialized");
    return ESP_OK;
  }

  // Initialize stack
  for (int i = 0; i < SCREEN_STACK_MAX_DEPTH; i++)
  {
    s_manager.stack[i] = NULL;
  }
  s_manager.depth = 0;
  s_manager.transition_in_progress = false;
  s_manager.initialized = true;

  ESP_LOGI(TAG, "Screen manager initialized (stack depth: %d)", SCREEN_STACK_MAX_DEPTH);
  return ESP_OK;
}

esp_err_t screen_manager_set_root(lv_obj_t *root_screen)
{
  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  if (!root_screen)
  {
    ESP_LOGE(TAG, "Invalid root screen (NULL)");
    return ESP_ERR_INVALID_ARG;
  }

  // Set root screen at bottom of stack
  s_manager.stack[0] = root_screen;
  if (s_manager.depth == 0)
  {
    s_manager.depth = 1;
  }

  ESP_LOGI(TAG, "Root screen set: %p (depth: %d)", root_screen, s_manager.depth);
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
  metadata->auto_delete = true; // Always auto-delete when popped
  lv_obj_set_user_data(screen, metadata);

  // Style as black container with no border/padding
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_border_width(screen, 0, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  // Enable scrollbar and vertical scrolling for content
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
  if (config->anim_type != SCREEN_ANIM_NONE)
  {
    lv_dir_t gesture_dir =
        (config->anim_type == SCREEN_ANIM_VERTICAL) ? LV_DIR_TOP : LV_DIR_LEFT;
    // Use go_back_wrapper as the hide callback for gestures
    screen_nav_setup_gestures(screen, go_back_wrapper, gesture_dir);
  }

  ESP_LOGI(TAG, "Created screen: title='%s', anim=%d, auto_delete=true",
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

  if (s_manager.transition_in_progress)
  {
    ESP_LOGW(TAG, "Transition already in progress, ignoring show request");
    return ESP_ERR_INVALID_STATE;
  }

  // Check stack overflow
  if (s_manager.depth >= SCREEN_STACK_MAX_DEPTH)
  {
    ESP_LOGE(TAG, "Screen stack overflow (max depth: %d)", SCREEN_STACK_MAX_DEPTH);
    return ESP_ERR_NO_MEM;
  }

  // Don't push the same screen twice consecutively
  if (s_manager.depth > 0 && s_manager.stack[s_manager.depth - 1] == screen)
  {
    ESP_LOGW(TAG, "Screen already on top of stack");
    return ESP_OK;
  }

  // Push screen onto stack
  s_manager.stack[s_manager.depth] = screen;
  s_manager.depth++;

  ESP_LOGI(TAG, "Pushed screen to stack (depth: %d)", s_manager.depth);

  // Get metadata from screen
  screen_metadata_t *metadata = (screen_metadata_t *)lv_obj_get_user_data(screen);
  if (!metadata)
  {
    ESP_LOGW(TAG, "Screen has no metadata, using instant load");
    lv_scr_load(screen);
    return ESP_OK;
  }

  // Use stored animation type from metadata
  screen_anim_type_t anim_type = metadata->anim_type;

  // Load screen with appropriate animation
  if (anim_type == SCREEN_ANIM_VERTICAL)
  {
    screen_nav_load_with_anim(screen, NULL);
  }
  else if (anim_type == SCREEN_ANIM_HORIZONTAL)
  {
    screen_nav_load_horizontal(screen, NULL);
  }
  else
  {
    lv_scr_load(screen);
  }

  ESP_LOGD(TAG, "Showed screen with animation type %d", anim_type);
  return ESP_OK;
}

esp_err_t screen_manager_go_back(void)
{
  ESP_LOGI(TAG, "go_back() called (depth: %d)", s_manager.depth);

  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  // Prevent re-entrant calls during transition
  if (s_manager.transition_in_progress)
  {
    ESP_LOGW(TAG, "Transition already in progress, ignoring go_back");
    return ESP_ERR_INVALID_STATE;
  }

  // Check if there's a previous screen to go back to
  if (s_manager.depth <= 1)
  {
    ESP_LOGI(TAG, "Already at root screen, cannot go back");
    return ESP_OK;
  }

  s_manager.transition_in_progress = true;

  // Get current screen (top of stack)
  lv_obj_t *current = s_manager.stack[s_manager.depth - 1];
  lv_obj_t *previous = s_manager.stack[s_manager.depth - 2];

  ESP_LOGI(TAG, "Going back: current=%p, previous=%p", current, previous);

  // Get metadata from current screen
  screen_metadata_t *metadata = NULL;
  screen_anim_type_t anim_type = SCREEN_ANIM_NONE;
  bool auto_delete = true;

  if (current)
  {
    metadata = (screen_metadata_t *)lv_obj_get_user_data(current);
    if (metadata)
    {
      anim_type = metadata->anim_type;
      auto_delete = metadata->auto_delete;
    }
  }

  // Pop current screen from stack
  s_manager.stack[s_manager.depth - 1] = NULL;
  s_manager.depth--;

  // Animate back to previous screen with auto_del parameter
  ESP_LOGI(TAG, "Animating back (anim_type=%d, auto_delete=%d)", anim_type, auto_delete);

  if (anim_type == SCREEN_ANIM_VERTICAL)
  {
    // Use LVGL's auto_del feature to delete current screen after animation
    lv_scr_load_anim(previous, LV_SCR_LOAD_ANIM_MOVE_TOP, SCREEN_ANIM_DURATION, 0, auto_delete);
  }
  else if (anim_type == SCREEN_ANIM_HORIZONTAL)
  {
    lv_scr_load_anim(previous, LV_SCR_LOAD_ANIM_MOVE_RIGHT, SCREEN_ANIM_DURATION, 0, auto_delete);
  }
  else
  {
    lv_scr_load(previous);
    if (auto_delete && current)
    {
      // For instant load, manually delete the old screen
      screen_metadata_t *md = (screen_metadata_t *)lv_obj_get_user_data(current);
      if (md)
      {
        free(md);
        lv_obj_set_user_data(current, NULL);
      }
      lv_obj_del(current);
    }
  }

  // Clear transition flag after a short delay (let animation start)
  s_manager.transition_in_progress = false;

  ESP_LOGI(TAG, "go_back() complete (new depth: %d)", s_manager.depth);
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

  // Remove from stack if present
  for (int i = 0; i < s_manager.depth; i++)
  {
    if (s_manager.stack[i] == screen)
    {
      // Shift remaining screens down
      for (int j = i; j < s_manager.depth - 1; j++)
      {
        s_manager.stack[j] = s_manager.stack[j + 1];
      }
      s_manager.stack[s_manager.depth - 1] = NULL;
      s_manager.depth--;
      ESP_LOGD(TAG, "Removed screen from stack at index %d", i);
      break;
    }
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

  // Delete LVGL object
  lv_obj_delete(screen);

  ESP_LOGD(TAG, "Destroyed screen (new depth: %d)", s_manager.depth);
  return ESP_OK;
}

lv_obj_t *screen_manager_get_current(void)
{
  if (!s_manager.initialized || s_manager.depth == 0)
  {
    return NULL;
  }
  return s_manager.stack[s_manager.depth - 1];
}

bool screen_manager_is_root(lv_obj_t *screen)
{
  if (!s_manager.initialized || !screen || s_manager.depth == 0)
  {
    return false;
  }
  // Root screen is the first one in the stack
  return (s_manager.stack[0] == screen);
}

int screen_manager_get_depth(void)
{
  if (!s_manager.initialized)
  {
    return 0;
  }
  return s_manager.depth;
}

esp_err_t screen_manager_pop_to_root(void)
{
  if (!s_manager.initialized)
  {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  if (s_manager.depth <= 1)
  {
    ESP_LOGI(TAG, "Already at root");
    return ESP_OK;
  }

  // Get root screen
  lv_obj_t *root = s_manager.stack[0];

  // Delete all screens above root
  for (int i = s_manager.depth - 1; i > 0; i--)
  {
    lv_obj_t *screen = s_manager.stack[i];
    if (screen)
    {
      screen_metadata_t *metadata = (screen_metadata_t *)lv_obj_get_user_data(screen);
      if (metadata)
      {
        free(metadata);
        lv_obj_set_user_data(screen, NULL);
      }
      lv_obj_del(screen);
    }
    s_manager.stack[i] = NULL;
  }

  s_manager.depth = 1;

  // Load root screen
  lv_scr_load(root);

  ESP_LOGI(TAG, "Popped to root screen");
  return ESP_OK;
}

bool screen_manager_can_go_back(void)
{
  return s_manager.initialized && s_manager.depth > 1;
}
