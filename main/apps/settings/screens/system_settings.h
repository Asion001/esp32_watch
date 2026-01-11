/**
 * @file system_settings.h
 * @brief System Settings Screen - Factory Reset, Uptime Reset, etc.
 *
 * Provides system management options including:
 * - Reset uptime counter
 * - Factory reset (future)
 * - Flash/RAM statistics (future)
 */

#ifndef SYSTEM_SETTINGS_H
#define SYSTEM_SETTINGS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Create the system settings screen
     *
     * Creates screen with system management options:
     * - Reset Uptime button (with confirmation)
     * - Other system options (to be added)
     *
     * @param parent Parent object (unused, managed by screen_manager)
     * @return Pointer to created screen object
     */
    lv_obj_t *system_settings_create(lv_obj_t *parent);

    /**
     * @brief Show the system settings screen
     */
    void system_settings_show(void);

    /**
     * @brief Hide the system settings screen
     */
    void system_settings_hide(void);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_SETTINGS_H
