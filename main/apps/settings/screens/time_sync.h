/**
 * @file time_sync.h
 * @brief Time synchronization settings screen
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Create Time & Sync settings screen
     *
     * @param parent Parent object (unused, screen_manager handles screen creation)
     * @return Screen object
     */
    lv_obj_t *time_sync_create(lv_obj_t *parent);

    /**
     * @brief Show Time & Sync settings screen
     */
    void time_sync_show(void);

    /**
     * @brief Refresh Time & Sync status labels
     */
    void time_sync_update_status(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H
