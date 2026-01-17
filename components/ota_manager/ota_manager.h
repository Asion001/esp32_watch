/**
 * @file ota_manager.h
 * @brief OTA (Over-The-Air) Update Manager for ESP32-C6 Watch
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OTA_STATE_IDLE,
  OTA_STATE_CHECKING,
  OTA_STATE_DOWNLOADING,
  OTA_STATE_COMPLETE,
  OTA_STATE_FAILED
} ota_state_t;

typedef struct {
  char version[32];
  char url[256];
  uint32_t size;
} ota_version_info_t;

typedef void (*ota_callback_t)(ota_state_t state, uint8_t progress, void *user_data);

esp_err_t ota_manager_init(void);
esp_err_t ota_manager_check_for_update(const char *url, ota_version_info_t *info);
esp_err_t ota_manager_start_update(const char *url);
ota_state_t ota_manager_get_state(void);
uint8_t ota_manager_get_progress(void);
esp_err_t ota_manager_get_current_version(char *version, size_t max_len);
esp_err_t ota_manager_set_update_url(const char *url);
esp_err_t ota_manager_get_update_url(char *url, size_t max_len);
esp_err_t ota_manager_register_callback(ota_callback_t callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif // OTA_MANAGER_H
