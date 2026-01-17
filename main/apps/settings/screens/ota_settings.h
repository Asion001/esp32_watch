/**
 * @file ota_settings.h
 * @brief OTA update settings screen
 */

#ifndef OTA_SETTINGS_H
#define OTA_SETTINGS_H

#include "lvgl.h"

/**
 * @brief Create OTA settings screen
 *
 * @param parent Parent object (typically screen)
 * @return OTA settings screen object
 */
lv_obj_t *ota_settings_create(lv_obj_t *parent);

/**
 * @brief Show OTA settings screen
 */
void ota_settings_show(void);

#endif // OTA_SETTINGS_H
