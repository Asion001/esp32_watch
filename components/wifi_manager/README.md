# WiFi Manager Component

## Overview

The WiFi Manager component provides a simple API for managing WiFi connectivity on the ESP32-C6 smartwatch. It handles scanning, connection management, credential storage, and auto-reconnect functionality.

## Features

- **WiFi Scanning**: Scan for available networks with signal strength and security info
- **Connection Management**: Connect/disconnect with automatic retry logic
- **Credential Storage**: Securely save WiFi credentials in NVS for auto-reconnect
- **Auto-Reconnect**: Automatically connect to saved network on boot
- **State Callbacks**: Get notified of WiFi state changes
- **Power Management**: Built-in WiFi power saving mode for battery life

## API Functions

### Initialization

```c
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_deinit(void);
```

Initialize/deinitialize the WiFi manager. Must call `wifi_manager_init()` before any other functions.

### Scanning

```c
esp_err_t wifi_manager_scan_start(void);
esp_err_t wifi_manager_get_scan_results(wifi_ap_info_t *ap_list, uint16_t *ap_count);
```

Start a WiFi scan (asynchronous) and retrieve results. Scan typically completes in 1-3 seconds.

### Connection

```c
esp_err_t wifi_manager_connect(const char *ssid, const char *password, bool save_credentials);
esp_err_t wifi_manager_disconnect(void);
esp_err_t wifi_manager_auto_connect(void);
```

Connect to a WiFi network with optional credential saving. Use `auto_connect()` on boot to reconnect to saved network.

### Status

```c
wifi_state_t wifi_manager_get_state(void);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_connected_ssid(char *ssid, size_t max_len);
esp_err_t wifi_manager_get_rssi(int8_t *rssi);
esp_err_t wifi_manager_get_ip_address(char *ip_str, size_t max_len);
```

Query current WiFi connection status, SSID, signal strength, and IP address.

### Callbacks

```c
esp_err_t wifi_manager_register_callback(wifi_manager_callback_t callback, void *user_data);
```

Register a callback to be notified of WiFi state changes.

### Credentials

```c
esp_err_t wifi_manager_clear_credentials(void);
bool wifi_manager_has_credentials(void);
```

Manage saved WiFi credentials in NVS.

## Usage Example

```c
#include "wifi_manager.h"

// Callback for WiFi state changes
void wifi_callback(wifi_state_t state, void *user_data) {
    switch (state) {
        case WIFI_STATE_CONNECTED:
            ESP_LOGI("APP", "WiFi connected!");
            break;
        case WIFI_STATE_DISCONNECTED:
            ESP_LOGI("APP", "WiFi disconnected");
            break;
        case WIFI_STATE_FAILED:
            ESP_LOGI("APP", "WiFi connection failed");
            break;
        default:
            break;
    }
}

void app_main(void) {
    // Initialize WiFi manager
    wifi_manager_init();
    
    // Register callback
    wifi_manager_register_callback(wifi_callback, NULL);
    
    // Try auto-connect first
    if (wifi_manager_has_credentials()) {
        wifi_manager_auto_connect();
    } else {
        // Or scan and connect manually
        wifi_manager_scan_start();
        
        // Wait for scan to complete...
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // Get scan results
        wifi_ap_info_t ap_list[20];
        uint16_t ap_count = 20;
        wifi_manager_get_scan_results(ap_list, &ap_count);
        
        // Connect to first network
        if (ap_count > 0) {
            wifi_manager_connect(ap_list[0].ssid, "password", true);
        }
    }
}
```

## WiFi States

- `WIFI_STATE_DISCONNECTED` - Not connected
- `WIFI_STATE_CONNECTING` - Connection in progress
- `WIFI_STATE_CONNECTED` - Successfully connected
- `WIFI_STATE_FAILED` - Connection failed after retries
- `WIFI_STATE_SCANNING` - Scan in progress

## Security Considerations

- WiFi storage set to `WIFI_STORAGE_RAM` (credentials not saved to WiFi flash)
- Credentials stored via encrypted NVS using `settings_storage` component
- Password buffers cleared from memory after use
- Input validation for SSID (max 32 chars) and password (8-64 chars)

## Power Management

- WiFi power save mode enabled (`WIFI_PS_MIN_MODEM`)
- Automatically reduces power consumption when WiFi is idle
- Disconnect when not needed to save battery

## Dependencies

- `esp_wifi` - ESP-IDF WiFi driver
- `esp_netif` - Network interface
- `esp_event` - Event handling
- `nvs_flash` - NVS storage
- `settings_storage` - Credential persistence

## Memory Usage

- Component: ~500 bytes RAM
- Scan results cache: ~1.2 KB (20 APs × 60 bytes)
- Event group: ~24 bytes
- Total: ~1.7 KB RAM

## Thread Safety

WiFi manager uses ESP-IDF's WiFi driver which is thread-safe. However, scan results are cached in a static buffer, so avoid concurrent scans.

## Limitations

- Maximum 20 scan results cached
- Maximum 3 connection retry attempts
- Supports only WPA2-PSK and open networks (no WEP, enterprise)
- Station mode only (no AP mode)
