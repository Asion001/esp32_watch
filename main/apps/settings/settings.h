/**
 * @file settings.h
 * @brief Settings Application
 *
 * Main settings menu with navigation to different settings categories
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and display the settings menu
 *
 * Creates a list-based settings menu with:
 * - Display settings (brightness, timeout)
 * - Time settings (timezone, NTP)
 * - About (version, uptime)
 *
 * @param parent Parent LVGL object (typically lv_screen_active())
 * @return lv_obj_t* Pointer to the created screen object
 */
lv_obj_t *settings_create(lv_obj_t *parent);

/**
 * @brief Show the settings menu
 *
 * Switches to the settings screen from watchface
 */
void settings_show(void);

/**
 * @brief Hide the settings menu
 *
 * Returns to the watchface screen
 */
void settings_hide(void);

/**
 * @brief Get the settings screen object
 *
 * @return lv_obj_t* Pointer to settings screen, or NULL if not created
 */
lv_obj_t *settings_get_screen(void);

/**
 * @brief Set the tileview for navigation
 *
 * Must be called after creating settings to enable proper back navigation
 *
 * @param tv Pointer to the main tileview widget
 */
void settings_set_tileview(lv_obj_t *tv);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_H
