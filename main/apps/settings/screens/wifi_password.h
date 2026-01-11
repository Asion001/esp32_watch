/**
 * @file wifi_password.h
 * @brief WiFi password input screen - keyboard for password entry
 */

#ifndef WIFI_PASSWORD_H
#define WIFI_PASSWORD_H

#include "lvgl.h"

/**
 * @brief Create WiFi password input screen
 * 
 * Displays LVGL keyboard for password entry with show/hide toggle
 * and save credentials option.
 * 
 * @param parent Parent object (typically screen)
 * @param ssid SSID of network to connect to
 * @param is_open True if network is open (no password required)
 * @return WiFi password screen object
 */
lv_obj_t *wifi_password_create(lv_obj_t *parent, const char *ssid, bool is_open);

/**
 * @brief Show WiFi password screen for selected network
 * 
 * @param ssid SSID of network to connect to
 * @param is_open True if network is open (no password required)
 */
void wifi_password_show(const char *ssid, bool is_open);

#endif // WIFI_PASSWORD_H
