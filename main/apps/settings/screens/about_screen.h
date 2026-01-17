/**
 * @file about_screen.h
 * @brief About Screen - System Information
 *
 * Screen showing firmware version, build time, and system statistics
 */

#ifndef ABOUT_SCREEN_H
#define ABOUT_SCREEN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the about screen
 *
 * Creates a screen with:
 * - Firmware version
 * - Build date and time
 * - Total uptime and boot count
 * - ESP-IDF version
 * - Chip information
 * - Back button
 *
 * @param parent Parent LVGL object
 * @return lv_obj_t* Pointer to the created screen object
 */
lv_obj_t *about_screen_create(lv_obj_t *parent);

/**
 * @brief Show the about screen
 */
void about_screen_show(void);

/**
 * @brief Hide the about screen
 */
void about_screen_hide(void);

#ifdef __cplusplus
}
#endif

#endif // ABOUT_SCREEN_H
