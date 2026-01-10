/**
 * @file watchface.h
 * @brief Digital Watchface Application
 *
 * Large digital clock display with battery indicator
 */

#ifndef WATCHFACE_H
#define WATCHFACE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Create and display the watchface
     *
     * Creates a large digital clock with:
     * - Big HH:MM time display (Montserrat 48)
     * - Date display (Day, Month DD)
     * - Battery percentage indicator in top-right corner
     *
     * @param parent Parent LVGL object (typically lv_screen_active())
     * @return lv_obj_t* Pointer to the created screen object
     */
    lv_obj_t *watchface_create(lv_obj_t *parent);

    /**
     * @brief Force immediate update of watchface
     *
     * Triggers timer to update time and battery status immediately
     */
    void watchface_update(void);

#ifdef __cplusplus
}
#endif

#endif // WATCHFACE_H
