/**
 * @file wifi_manager.c
 * @brief WiFi Manager Component Implementation
 */

#include "wifi_manager.h"
#include "settings_storage.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi_manager";

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_SCAN_DONE_BIT BIT2

// Maximum connection retry attempts
#define MAX_RETRY_ATTEMPTS 3

// WiFi manager state
static struct
{
  bool initialized;
  wifi_state_t state;
  EventGroupHandle_t event_group;
  esp_netif_t *sta_netif;
  int retry_count;
  wifi_manager_callback_t callback;
  void *callback_user_data;
  wifi_ap_info_t scan_results[20]; // Max 20 APs
  uint16_t scan_count;
} wifi_mgr = {0};

/**
 * @brief Update WiFi state and trigger callback
 */
static void wifi_manager_set_state(wifi_state_t new_state)
{
  if (wifi_mgr.state != new_state)
  {
    wifi_mgr.state = new_state;
    ESP_LOGI(TAG, "State changed to: %d", new_state);

    if (wifi_mgr.callback)
    {
      wifi_mgr.callback(new_state, wifi_mgr.callback_user_data);
    }
  }
}

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "WiFi station started");
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "Disconnected from AP");
      wifi_manager_set_state(WIFI_STATE_DISCONNECTED);

      if (wifi_mgr.retry_count < MAX_RETRY_ATTEMPTS)
      {
        wifi_mgr.retry_count++;
        ESP_LOGI(TAG, "Retry %d/%d", wifi_mgr.retry_count, MAX_RETRY_ATTEMPTS);
        esp_wifi_connect();
        wifi_manager_set_state(WIFI_STATE_CONNECTING);
      }
      else
      {
        ESP_LOGE(TAG, "Connection failed after %d attempts",
                 MAX_RETRY_ATTEMPTS);
        wifi_manager_set_state(WIFI_STATE_FAILED);
        xEventGroupSetBits(wifi_mgr.event_group, WIFI_FAIL_BIT);
      }
      break;

    case WIFI_EVENT_SCAN_DONE:
      // Fetch scan results - use static buffer to avoid stack overflow
      static wifi_ap_record_t ap_records[20];
      uint16_t ap_num = 20;

      // First get the number of APs found
      esp_wifi_scan_get_ap_num(&ap_num);
      if (ap_num > 20)
      {
        ap_num = 20; // Limit to our buffer size
      }
      wifi_mgr.scan_count = ap_num;

      // Then get the records if any were found
      if (ap_num > 0)
      {
        esp_wifi_scan_get_ap_records(&wifi_mgr.scan_count, ap_records);

        // Convert to our format
        for (int i = 0; i < wifi_mgr.scan_count; i++)
        {
          size_t len = strnlen((char *)ap_records[i].ssid, 32);
          memcpy(wifi_mgr.scan_results[i].ssid, (char *)ap_records[i].ssid, len);
          wifi_mgr.scan_results[i].ssid[len < 32 ? len : 32] = '\0';
          wifi_mgr.scan_results[i].rssi = ap_records[i].rssi;
          wifi_mgr.scan_results[i].authmode = ap_records[i].authmode;
          wifi_mgr.scan_results[i].channel = ap_records[i].primary;
        }
      }

      // Minimal logging to avoid stack overflow
      ESP_LOGI(TAG, "Scan done: %d APs", wifi_mgr.scan_count);

      // Signal that scan results are ready
      xEventGroupSetBits(wifi_mgr.event_group, WIFI_SCAN_DONE_BIT);

      wifi_manager_set_state(WIFI_STATE_DISCONNECTED);
      break;

    default:
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    wifi_mgr.retry_count = 0;
    wifi_manager_set_state(WIFI_STATE_CONNECTED);
    xEventGroupSetBits(wifi_mgr.event_group, WIFI_CONNECTED_BIT);
  }
}

esp_err_t wifi_manager_init(void)
{
  if (wifi_mgr.initialized)
  {
    ESP_LOGW(TAG, "Already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing WiFi manager");

  // Create event group
  wifi_mgr.event_group = xEventGroupCreate();
  if (!wifi_mgr.event_group)
  {
    ESP_LOGE(TAG, "Failed to create event group");
    return ESP_FAIL;
  }

  // Initialize TCP/IP network interface (ignore if already initialized)
  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
  {
    ESP_LOGE(TAG, "Failed to init netif: %s", esp_err_to_name(ret));
    vEventGroupDelete(wifi_mgr.event_group);
    return ret;
  }

  // Create default event loop if not already created
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
  {
    ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
    vEventGroupDelete(wifi_mgr.event_group);
    return ret;
  }

  // Create default WiFi station
  wifi_mgr.sta_netif = esp_netif_create_default_wifi_sta();

  // Initialize WiFi with default config
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Register event handlers
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  // Set WiFi mode to station
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  // Set WiFi storage to RAM (credentials managed via NVS)
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  // Enable WiFi power save mode for battery life
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));

  // Start WiFi
  ESP_ERROR_CHECK(esp_wifi_start());

  // Set WiFi country code (required for scanning to work properly)
  wifi_country_t country = {
      .cc = "US",                        // United States
      .schan = 1,                        // Start channel
      .nchan = 11,                       // Number of channels (1-11)
      .policy = WIFI_COUNTRY_POLICY_AUTO // Auto channel selection
  };
  ESP_ERROR_CHECK(esp_wifi_set_country(&country));
  ESP_LOGI(TAG, "WiFi country set to US (channels 1-11)");

  wifi_mgr.initialized = true;
  wifi_mgr.state = WIFI_STATE_DISCONNECTED;
  wifi_mgr.retry_count = 0;
  wifi_mgr.scan_count = 0;

  ESP_LOGI(TAG, "WiFi manager initialized");
  return ESP_OK;
}

esp_err_t wifi_manager_deinit(void)
{
  if (!wifi_mgr.initialized)
  {
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Deinitializing WiFi manager");

  // Disconnect if connected
  wifi_manager_disconnect();

  // Stop WiFi
  esp_err_t ret = esp_wifi_stop();
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "WiFi stop failed: %s", esp_err_to_name(ret));
  }

  // Unregister event handlers
  esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler);
  esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &wifi_event_handler);

  // Deinit WiFi (only if stop succeeded)
  ret = esp_wifi_deinit();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "WiFi deinit failed: %s", esp_err_to_name(ret));
  }

  // Destroy netif
  if (wifi_mgr.sta_netif)
  {
    esp_netif_destroy(wifi_mgr.sta_netif);
    wifi_mgr.sta_netif = NULL;
  }

  // Delete event group
  if (wifi_mgr.event_group)
  {
    vEventGroupDelete(wifi_mgr.event_group);
    wifi_mgr.event_group = NULL;
  }

  wifi_mgr.initialized = false;
  wifi_mgr.callback = NULL;

  ESP_LOGI(TAG, "WiFi manager deinitialized");
  return ESP_OK;
}

esp_err_t wifi_manager_scan_start(void)
{
  if (!wifi_mgr.initialized)
  {
    ESP_LOGE(TAG, "Not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Starting WiFi scan");

  // Disconnect from AP before scanning if currently connected
  if (wifi_mgr.state == WIFI_STATE_CONNECTED)
  {
    ESP_LOGI(TAG, "Disconnecting before scan");
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500)); // Wait longer for complete disconnect
  }

  // Clear previous scan results
  wifi_mgr.scan_count = 0;
  memset(wifi_mgr.scan_results, 0, sizeof(wifi_mgr.scan_results));
  xEventGroupClearBits(wifi_mgr.event_group, WIFI_SCAN_DONE_BIT);

  wifi_manager_set_state(WIFI_STATE_SCANNING);

  // Start scan with proper configuration
  wifi_scan_config_t scan_config = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0, // Scan all channels (1-11 for US)
      .show_hidden = false,
      .scan_type = WIFI_SCAN_TYPE_ACTIVE,
      .scan_time = {
          .active = {
              .min = 120, // Min time per channel (ms)
              .max = 300  // Max time per channel (ms)
          }}};

  esp_err_t ret = esp_wifi_scan_start(&scan_config, false); // Non-blocking
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(ret));
    wifi_manager_set_state(WIFI_STATE_DISCONNECTED);
    return ret;
  }

  return ESP_OK;
}

esp_err_t wifi_manager_wait_for_scan(uint32_t timeout_ms)
{
  if (!wifi_mgr.initialized)
  {
    ESP_LOGE(TAG, "Not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  EventBits_t bits = xEventGroupWaitBits(
      wifi_mgr.event_group, WIFI_SCAN_DONE_BIT, pdTRUE, pdFALSE,
      pdMS_TO_TICKS(timeout_ms));

  if (bits & WIFI_SCAN_DONE_BIT)
  {
    return ESP_OK;
  }

  return ESP_ERR_TIMEOUT;
}

esp_err_t wifi_manager_get_scan_results(wifi_ap_info_t *ap_list,
                                        uint16_t *ap_count)
{
  if (!wifi_mgr.initialized || !ap_list || !ap_count)
  {
    ESP_LOGE(TAG, "Invalid arguments: init=%d, ap_list=%p, ap_count=%p",
             wifi_mgr.initialized, ap_list, ap_count);
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Get scan results: stored count=%d, requested=%d",
           wifi_mgr.scan_count, *ap_count);

  uint16_t count =
      (*ap_count < wifi_mgr.scan_count) ? *ap_count : wifi_mgr.scan_count;

  for (uint16_t i = 0; i < count; i++)
  {
    memcpy(&ap_list[i], &wifi_mgr.scan_results[i], sizeof(wifi_ap_info_t));
  }

  *ap_count = count;
  ESP_LOGI(TAG, "Returning %d scan results", count);

  return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password,
                               bool save_credentials)
{
  if (!wifi_mgr.initialized || !ssid)
  {
    ESP_LOGE(TAG, "Invalid arguments");
    return ESP_ERR_INVALID_ARG;
  }

  // Validate SSID length
  size_t ssid_len = strnlen(ssid, 32);
  if (ssid_len == 0 || ssid_len > 32)
  {
    ESP_LOGE(TAG, "Invalid SSID length: %d", ssid_len);
    return ESP_ERR_INVALID_ARG;
  }

  // Validate password length (if provided)
  if (password)
  {
    size_t pass_len = strnlen(password, 64);
    if (pass_len > 0 && (pass_len < 8 || pass_len > 64))
    {
      ESP_LOGE(TAG, "Invalid password length: %d (must be 8-64 chars)",
               pass_len);
      return ESP_ERR_INVALID_ARG;
    }
  }

  ESP_LOGI(TAG, "Connecting to '%s'", ssid);

  // Configure WiFi
  wifi_config_t wifi_config = {0};
  ssid_len = strnlen(ssid, sizeof(wifi_config.sta.ssid) -
                               1); // Reuse existing ssid_len
  memcpy(wifi_config.sta.ssid, ssid, ssid_len);
  wifi_config.sta.ssid[ssid_len] = '\0';
  if (password && strnlen(password, 1) > 0)
  {
    size_t pass_len = strnlen(password, sizeof(wifi_config.sta.password) - 1);
    memcpy(wifi_config.sta.password, password, pass_len);
    wifi_config.sta.password[pass_len] = '\0';
  }
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;

  // Save credentials if requested
  if (save_credentials)
  {
    ESP_LOGI(TAG, "Saving credentials to NVS");
    settings_set_string(SETTING_KEY_WIFI_SSID, ssid);
    if (password)
    {
      settings_set_string(SETTING_KEY_WIFI_PASSWORD, password);
    }
    else
    {
      settings_set_string(SETTING_KEY_WIFI_PASSWORD, "");
    }
  }

  // Set WiFi configuration
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  // Clear event bits
  xEventGroupClearBits(wifi_mgr.event_group,
                       WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

  // Reset retry counter
  wifi_mgr.retry_count = 0;

  // Start connection
  wifi_manager_set_state(WIFI_STATE_CONNECTING);
  esp_err_t ret = esp_wifi_connect();

  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Connect failed: %s", esp_err_to_name(ret));
    wifi_manager_set_state(WIFI_STATE_FAILED);
    return ret;
  }

  // Clear password from memory for security
  if (password)
  {
    memset((void *)password, 0, strlen(password));
  }
  memset(&wifi_config, 0, sizeof(wifi_config));

  return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
  if (!wifi_mgr.initialized)
  {
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Disconnecting");

  esp_err_t ret = esp_wifi_disconnect();
  if (ret == ESP_OK)
  {
    wifi_manager_set_state(WIFI_STATE_DISCONNECTED);
  }

  return ret;
}

wifi_state_t wifi_manager_get_state(void) { return wifi_mgr.state; }

bool wifi_manager_is_connected(void)
{
  return wifi_mgr.state == WIFI_STATE_CONNECTED;
}

esp_err_t wifi_manager_get_connected_ssid(char *ssid, size_t max_len)
{
  if (!wifi_mgr.initialized || !ssid || max_len < 33)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!wifi_manager_is_connected())
  {
    return ESP_ERR_WIFI_NOT_CONNECT;
  }

  wifi_config_t wifi_config;
  esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  if (ret == ESP_OK)
  {
    size_t len = strnlen((char *)wifi_config.sta.ssid, max_len - 1);
    memcpy(ssid, (char *)wifi_config.sta.ssid, len);
    ssid[len] = '\0';
  }

  return ret;
}

esp_err_t wifi_manager_get_rssi(int8_t *rssi)
{
  if (!wifi_mgr.initialized || !rssi)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!wifi_manager_is_connected())
  {
    return ESP_ERR_WIFI_NOT_CONNECT;
  }

  wifi_ap_record_t ap_info;
  esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
  if (ret == ESP_OK)
  {
    *rssi = ap_info.rssi;
  }

  return ret;
}

esp_err_t wifi_manager_get_ip_address(char *ip_str, size_t max_len)
{
  if (!wifi_mgr.initialized || !ip_str || max_len < 16)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!wifi_manager_is_connected())
  {
    return ESP_ERR_WIFI_NOT_CONNECT;
  }

  esp_netif_ip_info_t ip_info;
  esp_err_t ret = esp_netif_get_ip_info(wifi_mgr.sta_netif, &ip_info);
  if (ret == ESP_OK)
  {
    snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
  }

  return ret;
}

esp_err_t wifi_manager_register_callback(wifi_manager_callback_t callback,
                                         void *user_data)
{
  wifi_mgr.callback = callback;
  wifi_mgr.callback_user_data = user_data;
  ESP_LOGI(TAG, "Callback registered");
  return ESP_OK;
}

esp_err_t wifi_manager_clear_credentials(void)
{
  ESP_LOGI(TAG, "Clearing saved credentials");
  settings_erase(SETTING_KEY_WIFI_SSID);
  settings_erase(SETTING_KEY_WIFI_PASSWORD);
  return ESP_OK;
}

bool wifi_manager_has_credentials(void)
{
  return settings_exists(SETTING_KEY_WIFI_SSID);
}

esp_err_t wifi_manager_auto_connect(void)
{
  if (!wifi_mgr.initialized)
  {
    ESP_LOGE(TAG, "Not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (!wifi_manager_has_credentials())
  {
    ESP_LOGI(TAG, "No saved credentials");
    return ESP_ERR_NOT_FOUND;
  }

  char ssid[33] = {0};
  char password[65] = {0};

  // Load credentials from NVS
  esp_err_t ret =
      settings_get_string(SETTING_KEY_WIFI_SSID, "", ssid, sizeof(ssid));
  if (ret != ESP_OK || strlen(ssid) == 0)
  {
    ESP_LOGE(TAG, "Failed to load SSID");
    return ESP_FAIL;
  }

  settings_get_string(SETTING_KEY_WIFI_PASSWORD, "", password,
                      sizeof(password));

  ESP_LOGI(TAG, "Auto-connecting to saved network");
  return wifi_manager_connect(ssid, password, false); // Don't re-save
}
