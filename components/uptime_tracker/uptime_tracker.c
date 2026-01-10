/**
 * @file uptime_tracker.c
 * @brief Uptime Tracking Component Implementation
 */

#include "uptime_tracker.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "UptimeTracker";

// NVS namespace for uptime storage
#define NVS_NAMESPACE "uptime"
#define NVS_KEY_TOTAL_UPTIME "total_up"
#define NVS_KEY_BOOT_COUNT "boot_cnt"

// Internal state
static bool initialized = false;
static uint64_t total_uptime_seconds = 0;
static uint64_t session_start_time_us = 0;
static uint32_t boot_count = 0;
static uint32_t last_save_time_sec = 0;

/**
 * @brief Load uptime data from NVS
 */
static esp_err_t load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    // Open NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No stored uptime data found (first boot)");
        total_uptime_seconds = 0;
        boot_count = 0;
        return ESP_OK;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read total uptime
    ret = nvs_get_u64(nvs_handle, NVS_KEY_TOTAL_UPTIME, &total_uptime_seconds);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read total uptime: %s", esp_err_to_name(ret));
        total_uptime_seconds = 0;
    }

    // Read boot count
    ret = nvs_get_u32(nvs_handle, NVS_KEY_BOOT_COUNT, &boot_count);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read boot count: %s", esp_err_to_name(ret));
        boot_count = 0;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Loaded from NVS: Total uptime=%llu sec, Boot count=%lu",
             total_uptime_seconds, boot_count);

    return ESP_OK;
}

esp_err_t uptime_tracker_init(void)
{
    if (initialized)
    {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing uptime tracker");

    // Initialize NVS if not already done
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Load stored uptime data
    ret = load_from_nvs();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load uptime data");
        return ret;
    }

    // Increment boot counter
    boot_count++;

    // Record session start time
    session_start_time_us = esp_timer_get_time();

    initialized = true;

    ESP_LOGI(TAG, "Uptime tracker initialized (Boot #%lu)", boot_count);

    // Save the incremented boot count immediately
    return uptime_tracker_save();
}

void uptime_tracker_update(void)
{
    if (!initialized)
    {
        return;
    }

    // This function is called frequently, so it's lightweight
    // Actual calculation happens in get_stats()
}

esp_err_t uptime_tracker_save(void)
{
    if (!initialized)
    {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    // Calculate current total uptime
    uint64_t current_session_sec = (esp_timer_get_time() - session_start_time_us) / 1000000ULL;
    uint64_t current_total = total_uptime_seconds + current_session_sec;

    // Open NVS for writing
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(ret));
        return ret;
    }

    // Save total uptime
    ret = nvs_set_u64(nvs_handle, NVS_KEY_TOTAL_UPTIME, current_total);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write total uptime: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Save boot count
    ret = nvs_set_u32(nvs_handle, NVS_KEY_BOOT_COUNT, boot_count);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write boot count: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);

    last_save_time_sec = (uint32_t)current_session_sec;

    ESP_LOGD(TAG, "Saved uptime: Total=%llu sec, Boot count=%lu", current_total, boot_count);

    return ret;
}

esp_err_t uptime_tracker_get_stats(uptime_stats_t *stats)
{
    if (!initialized)
    {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!stats)
    {
        ESP_LOGE(TAG, "Invalid parameter");
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate current session uptime
    uint64_t current_session_sec = (esp_timer_get_time() - session_start_time_us) / 1000000ULL;

    stats->current_uptime_sec = current_session_sec;
    stats->total_uptime_sec = total_uptime_seconds + current_session_sec;
    stats->boot_count = boot_count;
    stats->last_boot_time = 0; // Not tracking RTC time for now

    return ESP_OK;
}

int uptime_tracker_format_time(uint64_t uptime_sec, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
    {
        return 0;
    }

    uint32_t days = uptime_sec / 86400;
    uint32_t hours = (uptime_sec % 86400) / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;

    if (days > 0)
    {
        return snprintf(buffer, buffer_size, "%ud %uh %um", days, hours, minutes);
    }
    else if (hours > 0)
    {
        return snprintf(buffer, buffer_size, "%uh %um", hours, minutes);
    }
    else
    {
        return snprintf(buffer, buffer_size, "%um", minutes);
    }
}

esp_err_t uptime_tracker_reset(void)
{
    ESP_LOGW(TAG, "Resetting all uptime data");

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    // Open NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Erase all keys in namespace
    ret = nvs_erase_all(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Commit changes
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    // Reset internal state
    total_uptime_seconds = 0;
    boot_count = 0;
    session_start_time_us = esp_timer_get_time();

    ESP_LOGI(TAG, "Uptime data reset complete");

    return ret;
}
