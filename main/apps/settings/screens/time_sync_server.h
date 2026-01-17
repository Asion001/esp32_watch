/**
 * @file time_sync_server.h
 * @brief NTP server configuration screen
 */

#ifndef TIME_SYNC_SERVER_H
#define TIME_SYNC_SERVER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Create NTP server configuration screen
     *
     * @return Screen object
     */
    lv_obj_t *time_sync_server_create(void);

    /**
     * @brief Show NTP server configuration screen
     */
    void time_sync_server_show(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_SERVER_H
