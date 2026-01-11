/**
 * @file wifi_scan.h
 * @brief WiFi scan results screen - list of available networks
 */

#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include "lvgl.h"

/**
 * @brief Create WiFi scan results screen
 * 
 * Displays list of available WiFi networks with signal strength
 * and security indicators. User can tap any network to connect.
 * 
 * @param parent Parent object (typically screen)
 * @return WiFi scan screen object
 */
lv_obj_t *wifi_scan_create(lv_obj_t *parent);

/**
 * @brief Show WiFi scan screen
 * 
 * Loads the scan screen and starts scanning for networks.
 */
void wifi_scan_show(void);

#endif // WIFI_SCAN_H
