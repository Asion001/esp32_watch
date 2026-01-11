/**
 * @file wifi_manager.h
 * @brief WiFi Manager Component
 * 
 * Manages WiFi connection, scanning, and credentials
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum SSID length */
#define WIFI_MANAGER_MAX_SSID_LEN 32

/** Maximum password length */
#define WIFI_MANAGER_MAX_PASSWORD_LEN 64

/** Maximum number of APs in scan result */
#define WIFI_MANAGER_MAX_AP_NUM 20

/** WiFi connection states */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

/** WiFi scan result entry */
typedef struct {
    char ssid[WIFI_MANAGER_MAX_SSID_LEN + 1];
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
    bool is_open;
} wifi_ap_record_simple_t;

/** WiFi connection info callback */
typedef void (*wifi_state_callback_t)(wifi_state_t state, esp_err_t error);

/**
 * @brief Initialize WiFi manager
 * 
 * Initializes WiFi in station mode and loads saved credentials
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Deinitialize WiFi manager
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Start WiFi scan
 * 
 * Scans for available WiFi networks
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_scan_start(void);

/**
 * @brief Get scan results
 * 
 * @param ap_list Array to store scan results
 * @param max_aps Maximum number of APs to return
 * @param num_aps Pointer to store actual number of APs found
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_scan_results(wifi_ap_record_simple_t *ap_list, 
                                         uint16_t max_aps, 
                                         uint16_t *num_aps);

/**
 * @brief Connect to WiFi network
 * 
 * @param ssid SSID of network
 * @param password Password (can be NULL for open networks)
 * @param save_credentials Save credentials to NVS for auto-reconnect
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password, bool save_credentials);

/**
 * @brief Disconnect from WiFi
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Get current WiFi state
 * 
 * @return Current WiFi state
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get connected SSID
 * 
 * @param ssid Buffer to store SSID
 * @param max_len Maximum buffer length
 * @return ESP_OK on success, ESP_ERR_WIFI_NOT_CONNECT if not connected
 */
esp_err_t wifi_manager_get_connected_ssid(char *ssid, size_t max_len);

/**
 * @brief Get signal strength (RSSI)
 * 
 * @param rssi Pointer to store RSSI value
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_rssi(int8_t *rssi);

/**
 * @brief Register state change callback
 * 
 * @param callback Callback function
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_register_callback(wifi_state_callback_t callback);

/**
 * @brief Clear saved WiFi credentials
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_clear_credentials(void);

/**
 * @brief Check if credentials are saved
 * 
 * @return true if credentials exist in NVS
 */
bool wifi_manager_has_credentials(void);

/**
 * @brief Auto-connect using saved credentials
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_auto_connect(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
