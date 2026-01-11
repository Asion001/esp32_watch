/**
 * @file display_settings.h
 * @brief Display Settings Screen
 *
 * Screen for adjusting display brightness and timeout settings
 */

#ifndef DISPLAY_SETTINGS_H
#define DISPLAY_SETTINGS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the display settings screen
 *
 * Creates a screen with:
 * - Brightness slider (0-100%)
 * - Current brightness value display
 * - Sleep timeout selector
 * - Back button
 *
 * @param parent Parent LVGL object
 * @return lv_obj_t* Pointer to the created screen object
 */
lv_obj_t *display_settings_create(lv_obj_t *parent);

/**
 * @brief Show the display settings screen
 */
void display_settings_show(void);

/**
 * @brief Hide the display settings screen
 */
void display_settings_hide(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_SETTINGS_H
