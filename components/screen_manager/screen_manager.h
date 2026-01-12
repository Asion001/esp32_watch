/**
 * @file screen_manager.h
 * @brief Unified screen management with navigation stack and lifecycle handling
 *
 * Provides centralized screen creation with automatic title, gesture hint bar,
 * gesture setup, animation configuration, and stack-based navigation.
 * Screens are auto-deleted when popped from the navigation stack.
 */

#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "esp_err.h"
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Animation duration for screen transitions (ms) */
#define SCREEN_ANIM_DURATION 300

  /**
   * @brief Animation type for screen transitions
   */
  typedef enum
  {
    SCREEN_ANIM_NONE,      /*!< No animation (instant) */
    SCREEN_ANIM_VERTICAL,  /*!< Vertical animation (bottom-up / top-down) */
    SCREEN_ANIM_HORIZONTAL /*!< Horizontal animation (right-to-left /
                              left-to-right) */
  } screen_anim_type_t;

  /**
   * @brief Screen configuration structure
   */
  typedef struct
  {
    const char *title;            /*!< Screen title (NULL for no title) */
    bool show_back_button;        /*!< Show back button at top-left (auto-calls screen_manager_go_back) */
    screen_anim_type_t anim_type; /*!< Animation type for transitions */
    void (*hide_callback)(void);  /*!< Optional callback when screen hides (called before animation) */
  } screen_config_t;

  /**
   * @brief Initialize screen manager
   *
   * Must be called before using any screen manager functions.
   *
   * @return ESP_OK on success, ESP_FAIL on error
   */
  esp_err_t screen_manager_init(void);

  /**
   * @brief Set the root screen (base of navigation stack)
   *
   * Call this once after loading your main/home screen to register it
   * as the root of the navigation stack. This enables go_back() to work
   * properly for screens shown via screen_manager_show().
   *
   * @param root_screen The root screen object (e.g., tileview screen)
   * @return ESP_OK on success, ESP_FAIL on error
   */
  esp_err_t screen_manager_set_root(lv_obj_t *root_screen);

  /**
   * @brief Create a new managed screen
   *
   * Creates a screen with automatic:
   * - Black background container
   * - Title (if specified, styled with Montserrat 20, white, centered at top y=10)
   * - Back button (if show_back_button=true, 50x40px at top-left (5,5), calls screen_manager_go_back)
   * - Gesture setup (LEFT swipe for horizontal, DOWN swipe for vertical screens)
   * - Registration in navigation stack
   *
   * @param config Screen configuration
   * @return Created screen object, or NULL on error
   */
  lv_obj_t *screen_manager_create(const screen_config_t *config);

  /**
   * @brief Show a screen with animation
   *
   * Pushes screen to navigation stack and loads with appropriate animation.
   *
   * @param screen Screen to show
   * @return ESP_OK on success, ESP_FAIL on error
   */
  esp_err_t screen_manager_show(lv_obj_t *screen);

  /**
   * @brief Go back to previous screen in navigation stack
   *
   * Pops current screen, animates to previous screen, and handles cleanup.
   * No-op if already at root screen.
   *
   * @return ESP_OK on success, ESP_FAIL on error
   */
  esp_err_t screen_manager_go_back(void);

  /**
   * @brief Destroy a screen and clean up resources
   *
   * Removes from navigation stack, frees gesture data, deletes LVGL objects.
   *
   * @param screen Screen to destroy
   * @return ESP_OK on success, ESP_FAIL on error
   */
  esp_err_t screen_manager_destroy(lv_obj_t *screen);

  /**
   * @brief Get the currently active screen
   *
   * @return Current screen object, or NULL if none
   */
  lv_obj_t *screen_manager_get_current(void);

  /**
   * @brief Check if screen is the root (first screen in stack)
   *
   * @param screen Screen to check
   * @return true if root screen, false otherwise
   */
  bool screen_manager_is_root(lv_obj_t *screen);

  /**
   * @brief Get current navigation stack depth
   *
   * @return Stack depth (0 = empty, 1 = root only)
   */
  int screen_manager_get_depth(void);

  /**
   * @brief Pop all screens and return to root
   *
   * Deletes all screens except the root, then loads root screen.
   *
   * @return ESP_OK on success, ESP_FAIL on error
   */
  esp_err_t screen_manager_pop_to_root(void);

  /**
   * @brief Check if back navigation is possible
   *
   * @return true if can go back, false if at root or not initialized
   */
  bool screen_manager_can_go_back(void);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_MANAGER_H
