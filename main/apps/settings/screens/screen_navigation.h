/**
 * @file screen_navigation.h
 * @brief Common screen navigation utilities with animations and gestures
 */

#ifndef SCREEN_NAVIGATION_H
#define SCREEN_NAVIGATION_H

#include "lvgl.h"

/**
 * @brief Screen transition animation duration in milliseconds
 */
#define SCREEN_ANIM_DURATION 300

/**
 * @brief Callback function type for screen hide events
 */
typedef void (*screen_hide_cb_t)(void);

/**
 * @brief Load a screen with slide-in animation from right
 *
 * @param new_screen Screen to load
 * @param prev_screen Current screen (will be saved for back navigation)
 */
void screen_nav_load_with_anim(lv_obj_t *new_screen, lv_obj_t **prev_screen);

/**
 * @brief Load a screen with horizontal slide-in animation from right
 *
 * @param new_screen Screen to load
 * @param prev_screen Current screen (will be saved for back navigation)
 */
void screen_nav_load_horizontal(lv_obj_t *new_screen, lv_obj_t **prev_screen);

/**
 * @brief Go back to previous screen with slide-out animation to right
 *
 * @param current_screen Current screen
 * @param prev_screen Previous screen to return to
 */
void screen_nav_go_back_with_anim(lv_obj_t *current_screen, lv_obj_t *prev_screen);

/**
 * @brief Go back to previous screen with horizontal slide animation
 *
 * @param current_screen Current screen
 * @param prev_screen Previous screen to return to
 */
void screen_nav_go_back_horizontal(lv_obj_t *current_screen, lv_obj_t *prev_screen);

/**
 * @brief Setup gesture detection for swipe to go back
 *
 * @param screen Screen to add gesture detection to
 * @param hide_cb Callback function to call when gesture is detected
 * @param direction Gesture direction to detect (e.g., LV_DIR_BOTTOM, LV_DIR_RIGHT)
 */
void screen_nav_setup_gestures(lv_obj_t *screen, screen_hide_cb_t hide_cb, lv_dir_t direction);

/**
 * @brief Cleanup gesture data for a screen
 *
 * Frees allocated gesture data to prevent memory leaks.
 * Automatically called on LV_EVENT_DELETE.
 *
 * @param screen Screen to cleanup gestures for
 */
void screen_nav_cleanup_gestures(lv_obj_t *screen);

#endif // SCREEN_NAVIGATION_H
