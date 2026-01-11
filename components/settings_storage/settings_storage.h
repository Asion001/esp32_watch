/**
 * @file settings_storage.h
 * @brief Generic Settings Storage Layer using NVS
 *
 * Type-safe wrapper around ESP-IDF NVS for storing settings.
 * Provides default values and change notifications.
 */

#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Settings namespace in NVS */
#define SETTINGS_NAMESPACE "settings"

/** Settings keys */
#define SETTING_KEY_BRIGHTNESS "brightness"
#define SETTING_KEY_SLEEP_TIMEOUT "sleep_time"
#define SETTING_KEY_WIFI_SSID "wifi_ssid"
#define SETTING_KEY_WIFI_PASSWORD "wifi_pass"
#define SETTING_KEY_NTP_SERVER "ntp_server"
#define SETTING_KEY_TIMEZONE "timezone"

/** Default values */
#define SETTING_DEFAULT_BRIGHTNESS 80
#define SETTING_DEFAULT_SLEEP_TIMEOUT 30
#define SETTING_DEFAULT_NTP_SERVER "pool.ntp.org"
#define SETTING_DEFAULT_TIMEZONE "UTC+0"

/**
 * @brief Initialize settings storage
 *
 * Initializes NVS partition and prepares settings namespace.
 * Safe to call multiple times.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_storage_init(void);

/**
 * @brief Get integer setting with default value
 *
 * @param key Setting key name
 * @param default_value Default value if key doesn't exist
 * @param value Pointer to store the retrieved value
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 */
esp_err_t settings_get_int(const char *key, int32_t default_value,
                           int32_t *value);

/**
 * @brief Set integer setting
 *
 * @param key Setting key name
 * @param value Value to store
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_set_int(const char *key, int32_t value);

/**
 * @brief Get unsigned integer setting with default value
 *
 * @param key Setting key name
 * @param default_value Default value if key doesn't exist
 * @param value Pointer to store the retrieved value
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 */
esp_err_t settings_get_uint(const char *key, uint32_t default_value,
                            uint32_t *value);

/**
 * @brief Set unsigned integer setting
 *
 * @param key Setting key name
 * @param value Value to store
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_set_uint(const char *key, uint32_t value);

/**
 * @brief Get string setting with default value
 *
 * @param key Setting key name
 * @param default_value Default value if key doesn't exist (can be NULL)
 * @param value Buffer to store the retrieved string
 * @param max_len Maximum length of the buffer
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 */
esp_err_t settings_get_string(const char *key, const char *default_value,
                              char *value, size_t max_len);

/**
 * @brief Set string setting
 *
 * @param key Setting key name
 * @param value String value to store (NULL-terminated)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_set_string(const char *key, const char *value);

/**
 * @brief Get boolean setting with default value
 *
 * @param key Setting key name
 * @param default_value Default value if key doesn't exist
 * @param value Pointer to store the retrieved value
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if key doesn't exist
 */
esp_err_t settings_get_bool(const char *key, bool default_value, bool *value);

/**
 * @brief Set boolean setting
 *
 * @param key Setting key name
 * @param value Value to store
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_set_bool(const char *key, bool value);

/**
 * @brief Erase a specific setting
 *
 * @param key Setting key name
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_erase(const char *key);

/**
 * @brief Erase all settings (factory reset)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t settings_erase_all(void);

/**
 * @brief Check if a setting exists
 *
 * @param key Setting key name
 * @return true if key exists, false otherwise
 */
bool settings_exists(const char *key);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_STORAGE_H
