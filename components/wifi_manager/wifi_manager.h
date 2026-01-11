/**
 * @file wifi_manager.h
 * @brief WiFi Manager Component for ESP32-C6 Watch
 *
 * Provides WiFi connectivity management including scanning, connection,
 * credential storage, and auto-reconnect functionality.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi connection state
 */
typedef enum {
    WIFI_STATE_DISCONNECTED,  ///< Not connected
    WIFI_STATE_CONNECTING,    ///< Connection in progress
    WIFI_STATE_CONNECTED,     ///< Successfully connected
    WIFI_STATE_FAILED,        ///< Connection failed
    WIFI_STATE_SCANNING,      ///< Scan in progress
} wifi_state_t;

/**
 * @brief WiFi access point information
 */
typedef struct {
    char ssid[33];           ///< Network SSID (null-terminated)
    int8_t rssi;             ///< Signal strength in dBm
    wifi_auth_mode_t authmode; ///< Security type
    uint8_t channel;         ///< WiFi channel
} wifi_ap_info_t;

/**
 * @brief WiFi manager callback function type
 * @param state Current WiFi state
 * @param user_data User data pointer passed during registration
 */
typedef void (*wifi_manager_callback_t)(wifi_state_t state, void *user_data);

/**
 * @brief Initialize WiFi manager
 *
 * Must be called before any other WiFi manager functions.
 * Initializes ESP-IDF WiFi stack in station mode.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Deinitialize WiFi manager
 *
 * Stops WiFi, disconnects, and frees resources.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Start WiFi scan for available networks
 *
 * Scan is asynchronous. Results available via wifi_manager_get_scan_results()
 * after scan completes (typically 1-3 seconds).
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_scan_start(void);

/**
 * @brief Get WiFi scan results
 *
 * @param[out] ap_list Pointer to array to store AP information
 * @param[in,out] ap_count Input: max APs to retrieve, Output: actual count
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_scan_results(wifi_ap_info_t *ap_list, uint16_t *ap_count);

/**
 * @brief Connect to WiFi network
 *
 * @param ssid Network SSID (max 32 chars)
 * @param password Network password (8-64 chars for WPA2, empty for open)
 * @param save_credentials If true, save to NVS for auto-reconnect
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password, bool save_credentials);

/**
 * @brief Disconnect from WiFi network
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Get current WiFi connection state
 *
 * @return Current state
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief Check if WiFi is connected
 *
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get connected network SSID
 *
 * @param[out] ssid Buffer to store SSID (min 33 bytes)
 * @param[in] max_len Maximum buffer length
 * @return ESP_OK if connected, ESP_ERR_WIFI_NOT_CONNECT otherwise
 */
esp_err_t wifi_manager_get_connected_ssid(char *ssid, size_t max_len);

/**
 * @brief Get current WiFi signal strength (RSSI)
 *
 * @param[out] rssi Signal strength in dBm
 * @return ESP_OK if connected, ESP_ERR_WIFI_NOT_CONNECT otherwise
 */
esp_err_t wifi_manager_get_rssi(int8_t *rssi);

/**
 * @brief Get IP address of connected network
 *
 * @param[out] ip_str Buffer to store IP as string (min 16 bytes for IPv4)
 * @param[in] max_len Maximum buffer length
 * @return ESP_OK if connected with IP, error code otherwise
 */
esp_err_t wifi_manager_get_ip_address(char *ip_str, size_t max_len);

/**
 * @brief Register callback for WiFi state changes
 *
 * @param callback Callback function pointer
 * @param user_data Optional user data passed to callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_register_callback(wifi_manager_callback_t callback, void *user_data);

/**
 * @brief Clear saved WiFi credentials from NVS
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_clear_credentials(void);

/**
 * @brief Check if WiFi credentials are saved
 *
 * @return true if credentials exist in NVS, false otherwise
 */
bool wifi_manager_has_credentials(void);

/**
 * @brief Attempt to connect using saved credentials
 *
 * Loads SSID and password from NVS and attempts connection.
 * Use on boot for auto-reconnect functionality.
 *
 * @return ESP_OK if credentials exist and connect started, error code otherwise
 */
esp_err_t wifi_manager_auto_connect(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
