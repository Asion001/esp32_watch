/**
 * @file ntp_client.h
 * @brief NTP client for time synchronization and RTC updates
 */

#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the NTP client
     *
     * Loads settings (server, timezone, DST) and prepares SNTP.
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_init(void);

    /**
     * @brief Deinitialize the NTP client
     *
     * Stops SNTP service.
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_deinit(void);

    /**
     * @brief Trigger an immediate NTP sync
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_sync_now(void);

    /**
     * @brief Notify NTP client that WiFi is connected
     *
     * Applies auto-sync policy when enabled.
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_on_wifi_connected(void);

    /**
     * @brief Get last successful sync time (UTC epoch)
     *
     * @param[out] last_sync Output time_t value
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_get_last_sync(time_t *last_sync);

    /**
     * @brief Set NTP server hostname
     *
     * Saves to settings storage and updates SNTP if running.
     *
     * @param server Hostname string
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_set_ntp_server(const char *server);

    /**
     * @brief Get configured NTP server hostname
     *
     * @param[out] server Output buffer
     * @param[in] max_len Buffer size
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_get_ntp_server(char *server, size_t max_len);

    /**
     * @brief Set time zone string (UTC offset format)
     *
     * @param timezone String in UTC±H or UTC±H:MM format
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_set_timezone(const char *timezone);

    /**
     * @brief Get configured time zone string
     *
     * @param[out] timezone Output buffer
     * @param[in] max_len Buffer size
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_get_timezone(char *timezone, size_t max_len);

    /**
     * @brief Enable or disable Daylight Saving Time offset
     *
     * @param enabled True to enable DST (+1 hour)
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_set_dst_enabled(bool enabled);

    /**
     * @brief Get current DST enabled state
     *
     * @param[out] enabled Output value
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_get_dst_enabled(bool *enabled);

    /**
     * @brief Convert UTC time to local time based on settings
     *
     * @param utc_time UTC time_t value
     * @param[out] local_time Output struct tm in local time
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t ntp_client_get_local_time_from_utc(time_t utc_time,
                                                 struct tm *local_time);

#ifdef __cplusplus
}
#endif

#endif // NTP_CLIENT_H
