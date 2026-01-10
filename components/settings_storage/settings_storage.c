/**
 * @file settings_storage.c
 * @brief Generic Settings Storage Implementation
 */

#include "settings_storage.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "SettingsStorage";

static bool initialized = false;

esp_err_t settings_storage_init(void)
{
    if (initialized)
    {
        ESP_LOGD(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing settings storage");

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

    initialized = true;
    ESP_LOGI(TAG, "Settings storage initialized");
    return ESP_OK;
}

esp_err_t settings_get_int(const char *key, int32_t default_value, int32_t *value)
{
    if (!initialized || !key || !value)
    {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        *value = default_value;
        return ret;
    }

    ret = nvs_get_i32(nvs_handle, key, value);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        *value = default_value;
        ESP_LOGD(TAG, "Key '%s' not found, using default: %d", key, (int)default_value);
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Error reading '%s': %s", key, esp_err_to_name(ret));
        *value = default_value;
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t settings_set_int(const char *key, int32_t value)
{
    if (!initialized || !key)
    {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_i32(nvs_handle, key, value);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set '%s': %s", key, esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit '%s': %s", key, esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    ESP_LOGD(TAG, "Set '%s' = %d", key, (int)value);
    return ret;
}

esp_err_t settings_get_uint(const char *key, uint32_t default_value, uint32_t *value)
{
    if (!initialized || !key || !value)
    {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        *value = default_value;
        return ret;
    }

    ret = nvs_get_u32(nvs_handle, key, value);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        *value = default_value;
        ESP_LOGD(TAG, "Key '%s' not found, using default: %u", key, (unsigned int)default_value);
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Error reading '%s': %s", key, esp_err_to_name(ret));
        *value = default_value;
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t settings_set_uint(const char *key, uint32_t value)
{
    if (!initialized || !key)
    {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_u32(nvs_handle, key, value);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set '%s': %s", key, esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit '%s': %s", key, esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    ESP_LOGD(TAG, "Set '%s' = %u", key, (unsigned int)value);
    return ret;
}

esp_err_t settings_get_string(const char *key, const char *default_value, char *value, size_t max_len)
{
    if (!initialized || !key || !value || max_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        if (default_value)
        {
            strncpy(value, default_value, max_len - 1);
            value[max_len - 1] = '\0';
        }
        else
        {
            value[0] = '\0';
        }
        return ret;
    }

    size_t required_size = 0;
    ret = nvs_get_str(nvs_handle, key, NULL, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        if (default_value)
        {
            strncpy(value, default_value, max_len - 1);
            value[max_len - 1] = '\0';
            ESP_LOGD(TAG, "Key '%s' not found, using default: %s", key, default_value);
        }
        else
        {
            value[0] = '\0';
        }
        nvs_close(nvs_handle);
        return ret;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Error reading '%s': %s", key, esp_err_to_name(ret));
        if (default_value)
        {
            strncpy(value, default_value, max_len - 1);
            value[max_len - 1] = '\0';
        }
        else
        {
            value[0] = '\0';
        }
        nvs_close(nvs_handle);
        return ret;
    }

    if (required_size > max_len)
    {
        ESP_LOGW(TAG, "Buffer too small for '%s' (need %u, have %u)", key, (unsigned)required_size, (unsigned)max_len);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_SIZE;
    }

    ret = nvs_get_str(nvs_handle, key, value, &required_size);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t settings_set_string(const char *key, const char *value)
{
    if (!initialized || !key || !value)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(nvs_handle, key, value);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set '%s': %s", key, esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit '%s': %s", key, esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    ESP_LOGD(TAG, "Set '%s' = %s", key, value);
    return ret;
}

esp_err_t settings_get_bool(const char *key, bool default_value, bool *value)
{
    if (!initialized || !key || !value)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t stored_value;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        *value = default_value;
        return ret;
    }

    ret = nvs_get_u8(nvs_handle, key, &stored_value);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        *value = default_value;
        ESP_LOGD(TAG, "Key '%s' not found, using default: %s", key, default_value ? "true" : "false");
    }
    else if (ret == ESP_OK)
    {
        *value = (stored_value != 0);
    }
    else
    {
        ESP_LOGW(TAG, "Error reading '%s': %s", key, esp_err_to_name(ret));
        *value = default_value;
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t settings_set_bool(const char *key, bool value)
{
    if (!initialized || !key)
    {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t stored_value = value ? 1 : 0;
    ret = nvs_set_u8(nvs_handle, key, stored_value);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set '%s': %s", key, esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit '%s': %s", key, esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    ESP_LOGD(TAG, "Set '%s' = %s", key, value ? "true" : "false");
    return ret;
}

esp_err_t settings_erase(const char *key)
{
    if (!initialized || !key)
    {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_erase_key(nvs_handle, key);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to erase '%s': %s", key, esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Erased setting '%s'", key);
    return ret;
}

esp_err_t settings_erase_all(void)
{
    if (!initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(TAG, "Erasing all settings (factory reset)");

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_erase_all(nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase all: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "All settings erased successfully");
    return ret;
}

bool settings_exists(const char *key)
{
    if (!initialized || !key)
    {
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        return false;
    }

    // Try to get the key as a uint8 (smallest type) just to check existence
    uint8_t dummy;
    ret = nvs_get_u8(nvs_handle, key, &dummy);
    nvs_close(nvs_handle);

    return (ret == ESP_OK);
}
