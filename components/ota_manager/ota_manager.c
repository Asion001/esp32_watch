/**
 * @file ota_manager.c
 * @brief OTA Update Manager Implementation
 */

#include "ota_manager.h"
#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings_storage.h"
#include <string.h>

static const char *TAG = "ota_manager";

#define OTA_URL_KEY "ota_url"
#define OTA_AUTO_CHECK_KEY "ota_auto"

typedef struct {
  ota_state_t state;
  uint8_t progress;
  ota_callback_t callback;
  void *user_data;
  char update_url[256];
  bool auto_check_enabled;
} ota_manager_t;

static ota_manager_t ota_mgr = {
    .state = OTA_STATE_IDLE,
    .progress = 0,
    .callback = NULL,
    .user_data = NULL,
    .update_url = {0},
    .auto_check_enabled = false
};

// HTTP event handler for OTA progress
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id) {
  case HTTP_EVENT_ON_DATA:
    if (ota_mgr.callback && evt->data_len > 0) {
      // Calculate progress if we know total size
      // This is a simplified version
      ota_mgr.progress = (ota_mgr.progress + 1) % 100;
      ota_mgr.callback(OTA_STATE_DOWNLOADING, ota_mgr.progress, ota_mgr.user_data);
    }
    break;
  default:
    break;
  }
  return ESP_OK;
}

esp_err_t ota_manager_init(void)
{
  ESP_LOGI(TAG, "Initializing OTA manager");

  // Load saved URL from NVS
  char saved_url[256];
  if (settings_get_string(OTA_URL_KEY, CONFIG_OTA_UPDATE_URL, saved_url, sizeof(saved_url)) == ESP_OK) {
    strncpy(ota_mgr.update_url, saved_url, sizeof(ota_mgr.update_url) - 1);
  } else {
    strncpy(ota_mgr.update_url, CONFIG_OTA_UPDATE_URL, sizeof(ota_mgr.update_url) - 1);
  }

  // Load auto-check setting
  bool auto_check = false;
  if (settings_get_bool(OTA_AUTO_CHECK_KEY, CONFIG_OTA_AUTO_CHECK, &auto_check) == ESP_OK) {
    ota_mgr.auto_check_enabled = auto_check;
  }

  ESP_LOGI(TAG, "OTA manager initialized. URL: %s", ota_mgr.update_url);
  return ESP_OK;
}

esp_err_t ota_manager_get_current_version(char *version, size_t max_len)
{
  if (!version) return ESP_ERR_INVALID_ARG;

  const esp_app_desc_t *app_desc = esp_app_get_description();
  strncpy(version, app_desc->version, max_len - 1);
  version[max_len - 1] = '\0';
  return ESP_OK;
}

esp_err_t ota_manager_check_for_update(const char *url, ota_version_info_t *info)
{
  ESP_LOGI(TAG, "Checking for updates...");
  ota_mgr.state = OTA_STATE_CHECKING;

  // Simplified version check - in production, fetch version manifest
  if (info) {
    esp_app_desc_t *app_desc = (esp_app_desc_t *)esp_app_get_description();
    strncpy(info->version, app_desc->version, sizeof(info->version) - 1);
    strncpy(info->url, url ? url : ota_mgr.update_url, sizeof(info->url) - 1);
    info->size = 0; // Would be fetched from server
  }

  ota_mgr.state = OTA_STATE_IDLE;
  return ESP_OK;
}

esp_err_t ota_manager_start_update(const char *url)
{
  if (ota_mgr.state != OTA_STATE_IDLE) {
    ESP_LOGW(TAG, "OTA already in progress");
    return ESP_ERR_INVALID_STATE;
  }

  const char *update_url = url ? url : ota_mgr.update_url;
  ESP_LOGI(TAG, "Starting OTA update from: %s", update_url);

  ota_mgr.state = OTA_STATE_DOWNLOADING;
  ota_mgr.progress = 0;

  if (ota_mgr.callback) {
    ota_mgr.callback(OTA_STATE_DOWNLOADING, 0, ota_mgr.user_data);
  }

  esp_http_client_config_t config = {
      .url = update_url,
      .event_handler = http_event_handler,
      .keep_alive_enable = true,
  };

#ifdef CONFIG_OTA_ENABLE_HTTPS
  config.cert_pem = NULL; // Add your certificate here for HTTPS
  config.skip_cert_common_name_check = true;
#endif

  esp_https_ota_config_t ota_config = {
      .http_config = &config,
  };

  esp_err_t ret = esp_https_ota(&ota_config);

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "OTA update successful! Restarting...");
    ota_mgr.state = OTA_STATE_COMPLETE;
    if (ota_mgr.callback) {
      ota_mgr.callback(OTA_STATE_COMPLETE, 100, ota_mgr.user_data);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
  } else {
    ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    ota_mgr.state = OTA_STATE_FAILED;
    if (ota_mgr.callback) {
      ota_mgr.callback(OTA_STATE_FAILED, 0, ota_mgr.user_data);
    }
  }

  return ret;
}

ota_state_t ota_manager_get_state(void)
{
  return ota_mgr.state;
}

uint8_t ota_manager_get_progress(void)
{
  return ota_mgr.progress;
}

esp_err_t ota_manager_set_update_url(const char *url)
{
  if (!url) return ESP_ERR_INVALID_ARG;

  strncpy(ota_mgr.update_url, url, sizeof(ota_mgr.update_url) - 1);
  return settings_set_string(OTA_URL_KEY, url);
}

esp_err_t ota_manager_get_update_url(char *url, size_t max_len)
{
  if (!url) return ESP_ERR_INVALID_ARG;
  strncpy(url, ota_mgr.update_url, max_len - 1);
  url[max_len - 1] = '\0';
  return ESP_OK;
}

esp_err_t ota_manager_register_callback(ota_callback_t callback, void *user_data)
{
  ota_mgr.callback = callback;
  ota_mgr.user_data = user_data;
  return ESP_OK;
}
