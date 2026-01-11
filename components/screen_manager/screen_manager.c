/**
 * @file screen_manager.c
 * @brief Unified screen management implementation
 */

#include "screen_manager.h"
#include "esp_log.h"
#include "screen_navigation.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ScreenManager";

/**
 * @brief Screen manager state
 */
typedef struct {
  lv_obj_t *current_screen;        /*!< Currently active screen */
  lv_obj_t *previous_screen;       /*!< Previous screen for back navigation */
  screen_anim_type_t current_anim; /*!< Current screen animation type */
  void (*current_hide_cb)(void);   /*!< Current screen hide callback */
  bool initialized;                /*!< Initialization flag */
} screen_manager_state_t;

static screen_manager_state_t s_manager = {.current_screen = NULL,
                                           .previous_screen = NULL,
                                           .current_anim = SCREEN_ANIM_NONE,
                                           .current_hide_cb = NULL,
                                           .initialized = false};

/**
 * @brief Create title label
 */
static void create_title(lv_obj_t *parent, const char *title_text) {
  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, title_text);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
}

esp_err_t screen_manager_init(void) {
  if (s_manager.initialized) {
    ESP_LOGW(TAG, "Screen manager already initialized");
    return ESP_OK;
  }

  s_manager.current_screen = NULL;
  s_manager.previous_screen = NULL;
  s_manager.current_anim = SCREEN_ANIM_NONE;
  s_manager.current_hide_cb = NULL;
  s_manager.initialized = true;

  ESP_LOGI(TAG, "Screen manager initialized");
  return ESP_OK;
}

lv_obj_t *screen_manager_create(const screen_config_t *config) {
  if (!s_manager.initialized) {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return NULL;
  }

  if (!config) {
    ESP_LOGE(TAG, "Invalid config (NULL)");
    return NULL;
  }

  // Create screen container
  lv_obj_t *screen = lv_obj_create(NULL);
  if (!screen) {
    ESP_LOGE(TAG, "Failed to create screen object");
    return NULL;
  }

  // Style as black container with no border/padding
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_border_width(screen, 0, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  // Add title if provided
  if (config->title) {
    create_title(screen, config->title);
  }

  // Setup gestures based on animation type
  // Note: Gesture direction should match the swipe direction to close
  // - Vertical screens slide UP to open (MOVE_BOTTOM), so swipe DOWN to close
  // (DIR_TOP)
  // - Horizontal screens slide RIGHT to open (MOVE_LEFT), so swipe LEFT to
  // close (DIR_LEFT)
  if (config->anim_type != SCREEN_ANIM_NONE && config->hide_callback) {
    lv_dir_t gesture_dir =
        (config->anim_type == SCREEN_ANIM_VERTICAL) ? LV_DIR_TOP : LV_DIR_LEFT;
    screen_nav_setup_gestures(screen, config->hide_callback, gesture_dir);
  }

  // Store previous screen before switching
  s_manager.previous_screen = s_manager.current_screen;

  // Set as current screen
  s_manager.current_screen = screen;
  s_manager.current_anim = config->anim_type;
  s_manager.current_hide_cb = config->hide_callback;

  ESP_LOGI(TAG, "Created screen: title='%s', anim=%d",
           config->title ? config->title : "none", config->anim_type);

  return screen;
}

esp_err_t screen_manager_show(lv_obj_t *screen) {
  if (!s_manager.initialized) {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  if (!screen) {
    ESP_LOGE(TAG, "Invalid screen (NULL)");
    return ESP_ERR_INVALID_ARG;
  }

  // Use stored animation type
  screen_anim_type_t anim_type = s_manager.current_anim;

  // Load screen with appropriate animation
  if (anim_type == SCREEN_ANIM_VERTICAL) {
    screen_nav_load_with_anim(screen, NULL); // Vertical (bottom-up)
  } else if (anim_type == SCREEN_ANIM_HORIZONTAL) {
    screen_nav_load_horizontal(screen, NULL); // Horizontal (right-to-left)
  } else {
    // No animation - instant load
    lv_scr_load(screen);
  }

  ESP_LOGD(TAG, "Showed screen with animation type %d", anim_type);
  return ESP_OK;
}

esp_err_t screen_manager_go_back(void) {
  if (!s_manager.initialized) {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  // Check if there's a previous screen to go back to
  if (!s_manager.previous_screen) {
    ESP_LOGD(TAG, "No previous screen, cannot go back");
    return ESP_OK; // Not an error, just no-op
  }

  lv_obj_t *current = s_manager.current_screen;
  lv_obj_t *previous = s_manager.previous_screen;
  screen_anim_type_t anim_type = s_manager.current_anim;

  // Call hide callback if provided
  if (s_manager.current_hide_cb) {
    s_manager.current_hide_cb();
  }

  // Animate back to previous screen
  if (anim_type == SCREEN_ANIM_VERTICAL) {
    screen_nav_go_back_with_anim(current, previous); // Vertical (top-down)
  } else if (anim_type == SCREEN_ANIM_HORIZONTAL) {
    screen_nav_go_back_horizontal(current,
                                  previous); // Horizontal (left-to-right)
  } else {
    lv_scr_load(previous);
  }

  // Update state - now on previous screen
  s_manager.current_screen = previous;
  s_manager.previous_screen =
      NULL; // Clear previous (simple two-level navigation)
  s_manager.current_anim = SCREEN_ANIM_NONE;
  s_manager.current_hide_cb = NULL;

  ESP_LOGD(TAG, "Navigated back");
  return ESP_OK;
}

esp_err_t screen_manager_destroy(lv_obj_t *screen) {
  if (!s_manager.initialized) {
    ESP_LOGE(TAG, "Screen manager not initialized");
    return ESP_FAIL;
  }

  if (!screen) {
    ESP_LOGE(TAG, "Invalid screen (NULL)");
    return ESP_ERR_INVALID_ARG;
  }

  // Clear references if destroying current or previous screen
  if (s_manager.current_screen == screen) {
    s_manager.current_screen = NULL;
    s_manager.current_anim = SCREEN_ANIM_NONE;
    s_manager.current_hide_cb = NULL;
  }
  if (s_manager.previous_screen == screen) {
    s_manager.previous_screen = NULL;
  }

  // Cleanup gestures
  screen_nav_cleanup_gestures(screen);

  // Delete LVGL object (will trigger LV_EVENT_DELETE for any remaining cleanup)
  lv_obj_delete(screen);

  ESP_LOGD(TAG, "Destroyed screen");
  return ESP_OK;
}

lv_obj_t *screen_manager_get_current(void) {
  if (!s_manager.initialized) {
    return NULL;
  }
  return s_manager.current_screen;
}

bool screen_manager_is_root(lv_obj_t *screen) {
  if (!s_manager.initialized || !screen) {
    return false;
  }

  // Root screen is the one with no previous screen
  return (s_manager.current_screen == screen &&
          s_manager.previous_screen == NULL);
}
