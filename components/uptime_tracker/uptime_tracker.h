/**
 * @file uptime_tracker.h
 * @brief Uptime Tracking Component for ESP32-C6 Watch
 *
 * Tracks device uptime across reboots using NVS storage.
 * Useful for battery consumption testing and reliability monitoring.
 */

#ifndef UPTIME_TRACKER_H
#define UPTIME_TRACKER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Uptime statistics structure
     */
    typedef struct
    {
        uint64_t total_uptime_sec;   ///< Total uptime across all boots (seconds)
        uint64_t current_uptime_sec; ///< Current session uptime (seconds)
        uint32_t boot_count;         ///< Number of times device has booted
        uint32_t last_boot_time;     ///< Unix timestamp of last boot (if available)
    } uptime_stats_t;

    /**
     * @brief Initialize uptime tracker
     *
     * Loads stored uptime data from NVS and increments boot counter.
     * Must be called once at startup.
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t uptime_tracker_init(void);

    /**
     * @brief Update uptime counters
     *
     * Should be called periodically (e.g., every second) to update counters.
     * This function is lightweight and safe to call frequently.
     */
    void uptime_tracker_update(void);

    /**
     * @brief Save current uptime to NVS
     *
     * Persists current uptime data to NVS. Should be called periodically
     * (e.g., every 60 seconds) or before entering deep sleep.
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t uptime_tracker_save(void);

    /**
     * @brief Get current uptime statistics
     *
     * @param stats Pointer to structure to fill with uptime data
     * @return ESP_OK on success, ESP_ERR_INVALID_ARG if stats is NULL
     */
    esp_err_t uptime_tracker_get_stats(uptime_stats_t *stats);

    /**
     * @brief Format uptime into human-readable string
     *
     * Formats uptime as "Xd Xh Xm" (days, hours, minutes)
     *
     * @param uptime_sec Uptime in seconds
     * @param buffer Output buffer for formatted string
     * @param buffer_size Size of output buffer
     * @return Number of characters written (excluding null terminator)
     */
    int uptime_tracker_format_time(uint64_t uptime_sec, char *buffer, size_t buffer_size);

    /**
     * @brief Reset all uptime counters
     *
     * Clears all stored uptime data and boot count. Use with caution!
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t uptime_tracker_reset(void);

#ifdef __cplusplus
}
#endif

#endif // UPTIME_TRACKER_H
