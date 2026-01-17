/**
 * @file screen_navigation.c
 * @brief Common screen navigation utilities with animations and gestures
 */

#include "screen_navigation.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "ScreenNav";

/**
 * @brief Structure to hold gesture callback and direction
 */
typedef struct {
  screen_hide_cb_t hide_cb;
  lv_dir_t direction;
} gesture_data_t;

/**
 * @brief Gesture event handler wrapper
 */
static void gesture_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_GESTURE) {
    gesture_data_t *data = (gesture_data_t *)lv_event_get_user_data(e);
    if (!data || !data->hide_cb) {
      return;
    }

    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == data->direction) {
      ESP_LOGI(TAG, "Gesture detected - going back (dir: %d)", dir);
      data->hide_cb();
    }
  } else if (code == LV_EVENT_DELETE) {
    // Cleanup gesture data when screen is deleted
    gesture_data_t *data = (gesture_data_t *)lv_event_get_user_data(e);
    if (data) {
      ESP_LOGD(TAG, "Freeing gesture data on screen delete");
      free(data);
    }
  }
}

void screen_nav_load_with_anim(lv_obj_t *new_screen, lv_obj_t **prev_screen) {
  if (!new_screen) {
    ESP_LOGW(TAG, "Invalid screen pointer");
    return;
  }

  // Save reference to previous screen
  if (prev_screen) {
    *prev_screen = lv_scr_act();
  }

  // Load new screen with slide animation from bottom
  lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM,
                   SCREEN_ANIM_DURATION, 0, false);
}

void screen_nav_load_horizontal(lv_obj_t *new_screen, lv_obj_t **prev_screen) {
  if (!new_screen) {
    ESP_LOGW(TAG, "Invalid screen pointer");
    return;
  }

  // Save reference to previous screen
  if (prev_screen) {
    *prev_screen = lv_scr_act();
  }

  // Load new screen with slide animation from right
  lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, SCREEN_ANIM_DURATION,
                   0, false);
}

void screen_nav_go_back_with_anim(lv_obj_t *current_screen,
                                  lv_obj_t *prev_screen) {
  if (!prev_screen) {
    ESP_LOGW(TAG, "Invalid previous screen pointer");
    return;
  }

  // Return to previous screen with slide animation to bottom
  lv_scr_load_anim(prev_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, SCREEN_ANIM_DURATION,
                   0, false);
}

void screen_nav_go_back_horizontal(lv_obj_t *current_screen,
                                   lv_obj_t *prev_screen) {
  if (!prev_screen) {
    ESP_LOGW(TAG, "Invalid previous screen pointer");
    return;
  }

  // Return to previous screen with slide animation to right
  lv_scr_load_anim(prev_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                   SCREEN_ANIM_DURATION, 0, false);
}

void screen_nav_setup_gestures(lv_obj_t *screen, screen_hide_cb_t hide_cb,
                               lv_dir_t direction) {
  if (!screen || !hide_cb) {
    ESP_LOGW(TAG, "Invalid parameters for gesture setup");
    return;
  }

  // Allocate gesture data
  gesture_data_t *data = malloc(sizeof(gesture_data_t));
  if (!data) {
    ESP_LOGE(TAG, "Failed to allocate gesture data");
    return;
  }

  data->hide_cb = hide_cb;
  data->direction = direction;

  // Enable gesture detection (callback will also handle cleanup on delete)
  lv_obj_add_event_cb(screen, gesture_event_cb, LV_EVENT_GESTURE, data);
  lv_obj_add_event_cb(screen, gesture_event_cb, LV_EVENT_DELETE, data);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void screen_nav_cleanup_gestures(lv_obj_t *screen) {
  if (!screen) {
    ESP_LOGW(TAG, "Invalid screen pointer for cleanup");
    return;
  }

  // Note: gesture data is automatically freed by LV_EVENT_DELETE callback
  // This function is provided for explicit cleanup if needed before deletion
  ESP_LOGD(TAG, "Gesture cleanup requested (will be freed on delete)");
}
