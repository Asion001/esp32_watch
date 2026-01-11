/**
 * @file wifi_settings.h
 * @brief WiFi settings screen - connection status and controls
 */

#ifndef WIFI_SETTINGS_H
#define WIFI_SETTINGS_H

#include "lvgl.h"

/**
 * @brief Create WiFi settings screen
 * 
 * Displays current WiFi connection status, signal strength, IP address.
 * Provides controls for scanning networks, connecting, and disconnecting.
 * 
 * @param parent Parent object (typically screen)
 * @return WiFi settings screen object
 */
lv_obj_t *wifi_settings_create(lv_obj_t *parent);

/**
 * @brief Show WiFi settings screen
 * 
 * Loads the WiFi settings screen and updates status.
 */
void wifi_settings_show(void);

/**
 * @brief Update WiFi status display
 * 
 * Called by wifi_manager callback when connection state changes.
 */
void wifi_settings_update_status(void);

#endif // WIFI_SETTINGS_H
