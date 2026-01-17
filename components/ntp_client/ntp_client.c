/**
 * @file ntp_client.c
 * @brief NTP client implementation
 */

#include "ntp_client.h"
#include "rtc_pcf85063.h"
#include "settings_storage.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#ifdef CONFIG_NTP_CLIENT_ENABLE

static const char *TAG = "ntp_client";

#define NTP_SERVER_MAX_LEN 63
#define TIMEZONE_MAX_LEN 16

typedef struct
{
    bool initialized;
    bool sync_task_running;
    bool sntp_running;
    time_t last_sync;
    bool dst_enabled;
    int32_t tz_offset_minutes;
    char ntp_server[NTP_SERVER_MAX_LEN + 1];
    char timezone[TIMEZONE_MAX_LEN];
} ntp_client_state_t;

static ntp_client_state_t ntp_state = {
    .initialized = false,
    .sntp_running = false,
    .sync_task_running = false,
    .last_sync = 0,
    .dst_enabled = false,
    .tz_offset_minutes = 0,
    .ntp_server = {0},
    .timezone = {0},
};

static esp_err_t parse_timezone_offset(const char *timezone,
                                       int32_t *offset_minutes)
{
    if (!timezone || !offset_minutes)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (strncmp(timezone, "UTC", 3) != 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (timezone[3] == '\0')
    {
        *offset_minutes = 0;
        return ESP_OK;
    }

    int sign = 1;
    if (timezone[3] == '+')
    {
        sign = 1;
    }
    else if (timezone[3] == '-')
    {
        sign = -1;
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    const char *offset_str = timezone + 4;
    int hours = 0;
    int minutes = 0;

    if (strchr(offset_str, ':'))
    {
        if (sscanf(offset_str, "%d:%d", &hours, &minutes) != 2)
        {
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        if (sscanf(offset_str, "%d", &hours) != 1)
        {
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (hours < 0 || hours > 14 || minutes < 0 || minutes > 59)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *offset_minutes = sign * ((hours * 60) + minutes);
    return ESP_OK;
}

static void apply_timezone_settings(void)
{
    int32_t offset_minutes = 0;
    if (parse_timezone_offset(ntp_state.timezone, &offset_minutes) == ESP_OK)
    {
        ntp_state.tz_offset_minutes = offset_minutes;
    }
    else
    {
        ESP_LOGW(TAG, "Invalid timezone '%s', defaulting to UTC+0",
                 ntp_state.timezone);
        strncpy(ntp_state.timezone, "UTC+0", sizeof(ntp_state.timezone) - 1);
        ntp_state.tz_offset_minutes = 0;
    }

    setenv("TZ", ntp_state.timezone, 1);
    tzset();
}

static int32_t get_total_offset_seconds(void)
{
    int32_t offset_seconds = ntp_state.tz_offset_minutes * 60;
    if (ntp_state.dst_enabled)
    {
        offset_seconds += 3600;
    }
    return offset_seconds;
}

static void ntp_time_sync_cb(struct timeval *tv)
{
    if (!tv)
    {
        return;
    }

    ntp_state.last_sync = tv->tv_sec;
    settings_set_uint(SETTING_KEY_LAST_SYNC, (uint32_t)ntp_state.last_sync);

    struct tm local_time = {0};
    if (ntp_client_get_local_time_from_utc(ntp_state.last_sync, &local_time) ==
        ESP_OK)
    {
        esp_err_t rtc_ret = rtc_write_time(&local_time);
        if (rtc_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "RTC update failed: %s", esp_err_to_name(rtc_ret));
        }
    }

#ifdef CONFIG_NTP_DEBUG_LOGS
    ESP_LOGI(TAG, "Time synchronized: %ld", (long)ntp_state.last_sync);
#endif
}

static esp_err_t update_rtc_from_system_time(void)
{
    time_t now = time(NULL);
    if (now < 1600000000)
    {
        return ESP_ERR_INVALID_STATE;
    }

    struct tm local_time = {0};
    if (ntp_client_get_local_time_from_utc(now, &local_time) != ESP_OK)
    {
        return ESP_FAIL;
    }

    esp_err_t rtc_ret = rtc_write_time(&local_time);
    if (rtc_ret != ESP_OK)
    {
        return rtc_ret;
    }

    ntp_state.last_sync = now;
    settings_set_uint(SETTING_KEY_LAST_SYNC, (uint32_t)ntp_state.last_sync);
    return ESP_OK;
}

static void ntp_sync_wait_task(void *arg)
{
    (void)arg;
    const int max_attempts = 40; // ~20 seconds at 500ms intervals
    int attempts = 0;

    while (attempts < max_attempts)
    {
        sntp_sync_status_t status = esp_sntp_get_sync_status();
        if (status == SNTP_SYNC_STATUS_COMPLETED)
        {
            esp_err_t rtc_ret = update_rtc_from_system_time();
            if (rtc_ret == ESP_OK)
            {
                ESP_LOGI(TAG, "RTC updated after NTP sync");
                break;
            }

            ESP_LOGW(TAG, "RTC update pending: %s", esp_err_to_name(rtc_ret));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
    }

    if (attempts >= max_attempts)
    {
        ESP_LOGW(TAG, "NTP sync did not complete in time");
    }

    ntp_state.sync_task_running = false;
    vTaskDelete(NULL);
}

static void start_sntp(void)
{
    esp_sntp_stop();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_set_time_sync_notification_cb(ntp_time_sync_cb);

    esp_sntp_set_sync_interval(
        (uint32_t)CONFIG_NTP_SYNC_INTERVAL_MIN * 60U * 1000U);

    if (ntp_state.ntp_server[0] == '\0')
    {
        strncpy(ntp_state.ntp_server, CONFIG_NTP_DEFAULT_SERVER,
                sizeof(ntp_state.ntp_server) - 1);
    }

    esp_sntp_setservername(0, ntp_state.ntp_server);
    esp_sntp_init();
    ntp_state.sntp_running = true;

    ESP_LOGI(TAG, "SNTP started with server: %s", ntp_state.ntp_server);
}

esp_err_t ntp_client_init(void)
{
    if (ntp_state.initialized)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing NTP client");

    char server[NTP_SERVER_MAX_LEN + 1] = {0};
    if (settings_get_string(SETTING_KEY_NTP_SERVER, CONFIG_NTP_DEFAULT_SERVER,
                            server, sizeof(server)) == ESP_OK)
    {
        strncpy(ntp_state.ntp_server, server, sizeof(ntp_state.ntp_server) - 1);
    }
    else
    {
        strncpy(ntp_state.ntp_server, CONFIG_NTP_DEFAULT_SERVER,
                sizeof(ntp_state.ntp_server) - 1);
    }

    char timezone[TIMEZONE_MAX_LEN] = {0};
    if (settings_get_string(SETTING_KEY_TIMEZONE, CONFIG_NTP_DEFAULT_TIMEZONE,
                            timezone, sizeof(timezone)) == ESP_OK)
    {
        strncpy(ntp_state.timezone, timezone, sizeof(ntp_state.timezone) - 1);
    }
    else
    {
        strncpy(ntp_state.timezone, CONFIG_NTP_DEFAULT_TIMEZONE,
                sizeof(ntp_state.timezone) - 1);
    }

    bool dst_enabled = false;
#ifdef CONFIG_NTP_DEFAULT_DST
    bool default_dst = true;
#else
    bool default_dst = false;
#endif
    if (settings_get_bool(SETTING_KEY_DST_ENABLED, default_dst, &dst_enabled) ==
        ESP_OK)
    {
        ntp_state.dst_enabled = dst_enabled;
    }
    else
    {
        ntp_state.dst_enabled = default_dst;
    }

    uint32_t last_sync = 0;
    if (settings_get_uint(SETTING_KEY_LAST_SYNC, 0, &last_sync) == ESP_OK)
    {
        ntp_state.last_sync = (time_t)last_sync;
    }

    apply_timezone_settings();
    start_sntp();

    ntp_state.initialized = true;
    ESP_LOGI(TAG, "NTP client initialized");
    return ESP_OK;
}

esp_err_t ntp_client_deinit(void)
{
    if (!ntp_state.initialized)
    {
        return ESP_OK;
    }

    esp_sntp_stop();
    ntp_state.sntp_running = false;
    ntp_state.initialized = false;
    return ESP_OK;
}

esp_err_t ntp_client_sync_now(void)
{
    if (!ntp_state.initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ntp_state.sntp_running)
    {
        start_sntp();
    }

    if (!esp_sntp_restart())
    {
        start_sntp();
    }

    if (!ntp_state.sync_task_running)
    {
        ntp_state.sync_task_running = true;
        xTaskCreate(ntp_sync_wait_task, "ntp_sync_wait", 4096, NULL,
                    tskIDLE_PRIORITY + 1, NULL);
    }

    return ESP_OK;
}

esp_err_t ntp_client_on_wifi_connected(void)
{
#ifndef CONFIG_NTP_SYNC_ON_WIFI_CONNECT
    return ESP_OK;
#else
    if (!ntp_state.initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    time_t now = time(NULL);
    if (now < 1600000000 || ntp_state.last_sync == 0)
    {
        return ntp_client_sync_now();
    }

    int64_t elapsed = (int64_t)now - (int64_t)ntp_state.last_sync;
    int64_t min_interval = (int64_t)CONFIG_NTP_SYNC_INTERVAL_MIN * 60;
    if (elapsed >= min_interval)
    {
        return ntp_client_sync_now();
    }

    return ESP_OK;
#endif
}

esp_err_t ntp_client_get_last_sync(time_t *last_sync)
{
    if (!last_sync)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *last_sync = ntp_state.last_sync;
    return ESP_OK;
}

esp_err_t ntp_client_set_ntp_server(const char *server)
{
    if (!server)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strnlen(server, NTP_SERVER_MAX_LEN + 1);
    if (len == 0 || len > NTP_SERVER_MAX_LEN)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(ntp_state.ntp_server, server, sizeof(ntp_state.ntp_server) - 1);
    ntp_state.ntp_server[sizeof(ntp_state.ntp_server) - 1] = '\0';

    settings_set_string(SETTING_KEY_NTP_SERVER, ntp_state.ntp_server);

    if (ntp_state.initialized)
    {
        esp_sntp_setservername(0, ntp_state.ntp_server);
        esp_sntp_restart();
    }

    return ESP_OK;
}

esp_err_t ntp_client_get_ntp_server(char *server, size_t max_len)
{
    if (!server || max_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(server, ntp_state.ntp_server, max_len - 1);
    server[max_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t ntp_client_set_timezone(const char *timezone)
{
    if (!timezone)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strnlen(timezone, TIMEZONE_MAX_LEN);
    if (len == 0 || len >= TIMEZONE_MAX_LEN)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(ntp_state.timezone, timezone, sizeof(ntp_state.timezone) - 1);
    ntp_state.timezone[sizeof(ntp_state.timezone) - 1] = '\0';

    settings_set_string(SETTING_KEY_TIMEZONE, ntp_state.timezone);
    apply_timezone_settings();

    return ESP_OK;
}

esp_err_t ntp_client_get_timezone(char *timezone, size_t max_len)
{
    if (!timezone || max_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(timezone, ntp_state.timezone, max_len - 1);
    timezone[max_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t ntp_client_set_dst_enabled(bool enabled)
{
    ntp_state.dst_enabled = enabled;
    settings_set_bool(SETTING_KEY_DST_ENABLED, enabled);
    return ESP_OK;
}

esp_err_t ntp_client_get_dst_enabled(bool *enabled)
{
    if (!enabled)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *enabled = ntp_state.dst_enabled;
    return ESP_OK;
}

esp_err_t ntp_client_get_local_time_from_utc(time_t utc_time,
                                             struct tm *local_time)
{
    if (!local_time)
    {
        return ESP_ERR_INVALID_ARG;
    }

    time_t adjusted = utc_time + get_total_offset_seconds();
    gmtime_r(&adjusted, local_time);
    return ESP_OK;
}

#else // CONFIG_NTP_CLIENT_ENABLE

esp_err_t ntp_client_init(void) { return ESP_OK; }

esp_err_t ntp_client_deinit(void) { return ESP_OK; }

esp_err_t ntp_client_sync_now(void) { return ESP_OK; }

esp_err_t ntp_client_on_wifi_connected(void) { return ESP_OK; }

esp_err_t ntp_client_get_last_sync(time_t *last_sync)
{
    if (last_sync)
    {
        *last_sync = 0;
    }
    return ESP_OK;
}

esp_err_t ntp_client_set_ntp_server(const char *server)
{
    (void)server;
    return ESP_OK;
}

esp_err_t ntp_client_get_ntp_server(char *server, size_t max_len)
{
    if (server && max_len > 0)
    {
        server[0] = '\0';
    }
    return ESP_OK;
}

esp_err_t ntp_client_set_timezone(const char *timezone)
{
    (void)timezone;
    return ESP_OK;
}

esp_err_t ntp_client_get_timezone(char *timezone, size_t max_len)
{
    if (timezone && max_len > 0)
    {
        timezone[0] = '\0';
    }
    return ESP_OK;
}

esp_err_t ntp_client_set_dst_enabled(bool enabled)
{
    (void)enabled;
    return ESP_OK;
}

esp_err_t ntp_client_get_dst_enabled(bool *enabled)
{
    if (enabled)
    {
        *enabled = false;
    }
    return ESP_OK;
}

esp_err_t ntp_client_get_local_time_from_utc(time_t utc_time,
                                             struct tm *local_time)
{
    (void)utc_time;
    if (local_time)
    {
        memset(local_time, 0, sizeof(*local_time));
    }
    return ESP_OK;
}

#endif // CONFIG_NTP_CLIENT_ENABLE
