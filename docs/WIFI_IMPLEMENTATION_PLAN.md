# WiFi Configuration Implementation Guide

## Overview

This document outlines the plan for implementing Phase 2 WiFi Configuration features.

## Architecture

### Component: wifi_manager

Location: `components/wifi_manager/`

**Purpose**: Manages WiFi connectivity, scanning, and credential storage

**Key Features**:
- WiFi initialization and configuration
- Access point scanning
- Connection management
- Secure credential storage (NVS)
- Auto-reconnect functionality
- Connection state monitoring

### UI Screens

#### 1. WiFi Settings Screen
Location: `main/apps/settings/screens/wifi_settings.c/h`

**Features**:
- "Scan WiFi" button
- List of saved networks
- Connection status display
- "Forget Network" option
- Current network info (SSID, signal strength, IP)

#### 2. WiFi Scan Screen
Location: `main/apps/settings/screens/wifi_scan.c/h`

**Features**:
- List of available networks
- Signal strength indicators
- Security type icons (open/WPA/WPA2)
- Sorting by signal strength
- Refresh button
- Select network to connect

#### 3. WiFi Password Screen
Location: `main/apps/settings/screens/wifi_password.c/h`

**Features**:
- LVGL keyboard widget for password input
- Show/hide password toggle
- "Connect" and "Cancel" buttons
- Save credentials checkbox
- Input validation

## Implementation Plan

### Phase 1: WiFi Manager Component ⏳

**Files to Create**:
- `components/wifi_manager/wifi_manager.h` - API header ✅ Created
- `components/wifi_manager/wifi_manager.c` - Implementation
- `components/wifi_manager/CMakeLists.txt` - Build config
- `components/wifi_manager/Kconfig` - Configuration options

**Key Functions**:
```c
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_scan_start(void);
esp_err_t wifi_manager_get_scan_results(...);
esp_err_t wifi_manager_connect(ssid, password, save);
esp_err_t wifi_manager_disconnect(void);
wifi_state_t wifi_manager_get_state(void);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_auto_connect(void);
```

**Dependencies**:
- ESP-IDF WiFi API (`esp_wifi.h`)
- ESP-IDF Event Loop (`esp_event.h`)
- ESP-IDF NetIF (`esp_netif.h`)
- settings_storage component (for credentials)

**Implementation Tasks**:
1. WiFi initialization in STA mode
2. Event handler for WiFi events (connect, disconnect, scan done)
3. Scan functionality with result caching
4. Connection state machine
5. Credential storage/retrieval via settings_storage
6. Auto-reconnect logic
7. Signal strength monitoring

### Phase 2: WiFi Settings Screen ⏳

**Features**:
- Main WiFi settings entry point
- Display connection status
- "Scan for Networks" button
- List saved networks (if any)
- Forget network functionality
- Network info display (when connected)

**UI Elements**:
- Status label (Connected/Disconnected/Connecting)
- SSID label (when connected)
- Signal strength icon/label
- IP address display
- Scan button
- Disconnect button (when connected)

### Phase 3: WiFi Scan Results Screen ⏳

**Features**:
- LVGL list widget for AP results
- Signal strength bars/icons
- Security indicators (🔒 for secured, 🔓 for open)
- RSSI value display
- Tap to select network
- Refresh button
- Back button

**Sorting**:
- By signal strength (strongest first)
- Filter out duplicate SSIDs (keep strongest)

**UI Elements**:
- List with AP items
- Each item shows: SSID, RSSI bars, security icon
- Loading indicator during scan
- "No networks found" message

### Phase 4: WiFi Password Input Screen ⏳

**Features**:
- LVGL keyboard widget (`lv_keyboard_create()`)
- Password text area
- Show/hide password button
- Save credentials checkbox
- Connect button
- Cancel button

**Validation**:
- Minimum password length (8 characters for WPA/WPA2)
- Maximum length check
- Empty password allowed for open networks

**UI Layout**:
```
┌──────────────────────────────────────┐
│ Connect to: MyNetwork                │
│                                       │
│ Password:                             │
│ [******************]  [👁]           │
│                                       │
│ [x] Save credentials                  │
│                                       │
│ [QWERTYUIOP]                         │
│ [ASDFGHJKL]                          │
│ [ZXCVBNM]                            │
│ [123] [Space] [⌫]                   │
│                                       │
│ [Connect]  [Cancel]                   │
└──────────────────────────────────────┘
```

### Phase 5: Integration & Auto-Connect ⏳

**Auto-Connect Flow**:
1. On boot, check if credentials exist: `wifi_manager_has_credentials()`
2. If yes, attempt auto-connect: `wifi_manager_auto_connect()`
3. Display connection status on watchface (optional)
4. Handle connection failures gracefully

**Watchface Integration** (Optional):
- WiFi icon in status bar
- Show when connected
- Update signal strength periodically

## Technical Considerations

### Security

**Credential Storage**:
- Use encrypted NVS partition (if available)
- Store using settings_storage: `SETTING_KEY_WIFI_SSID`, `SETTING_KEY_WIFI_PASSWORD`
- Never log passwords
- Clear text input after connection attempt

### Memory Management

**WiFi Scan Results**:
- Limit to 20 APs max (`WIFI_MANAGER_MAX_AP_NUM`)
- Free scan results after processing
- Use stack allocation where possible

**Keyboard Widget**:
- LVGL keyboard can be memory-intensive
- Create on-demand, destroy after use
- Monitor heap usage

### Error Handling

**Connection Failures**:
- Wrong password: Show error, allow retry
- Network not found: Show error, return to scan
- Timeout: Show error, offer retry
- No DHCP: Show error with details

**Scan Failures**:
- WiFi not initialized: Show error
- Scan timeout: Show error, offer retry
- No networks found: Show friendly message

### Thread Safety

**WiFi Events**:
- WiFi events arrive in event loop task
- Use event groups or semaphores for synchronization
- Update UI only from LVGL task (use `bsp_display_lock()`)
- Consider using LVGL timers for polling state

### Power Management

**WiFi Power Save**:
- Enable WiFi power save mode: `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)`
- Disconnect when not needed
- Provide manual disconnect option

## Testing Requirements

### Unit Testing
- WiFi manager state transitions
- Credential storage/retrieval
- Scan result parsing
- SSID validation

### Integration Testing
- Connect to open network
- Connect to WPA2 network
- Connect with wrong password (should fail gracefully)
- Auto-reconnect after reboot
- Forget network functionality
- Multiple scan cycles

### Hardware Testing
- Test on actual ESP32-C6 device
- Test with various WiFi routers
- Test signal strength indicators
- Test keyboard usability
- Test in weak signal areas
- Test rapid connect/disconnect cycles

## TODO Checklist

### WiFi Manager Component
- [ ] Implement wifi_manager.c
- [ ] Add CMakeLists.txt
- [ ] Add Kconfig options
- [ ] Integrate with settings_storage
- [ ] Test initialization
- [ ] Test scanning
- [ ] Test connection
- [ ] Test credential storage
- [ ] Test auto-reconnect

### WiFi Settings Screen
- [ ] Create wifi_settings.h/c
- [ ] Design UI layout
- [ ] Add to settings menu
- [ ] Implement status display
- [ ] Implement scan button
- [ ] Implement forget network
- [ ] Test navigation

### WiFi Scan Screen
- [ ] Create wifi_scan.h/c
- [ ] Design list UI
- [ ] Implement AP list display
- [ ] Add signal strength indicators
- [ ] Add security icons
- [ ] Implement tap to select
- [ ] Test sorting
- [ ] Test refresh

### WiFi Password Screen
- [ ] Create wifi_password.h/c
- [ ] Integrate LVGL keyboard
- [ ] Implement password input
- [ ] Add show/hide toggle
- [ ] Add save credentials checkbox
- [ ] Implement connect logic
- [ ] Test validation
- [ ] Test with various networks

### Integration
- [ ] Initialize WiFi manager in main.c
- [ ] Add auto-connect on boot
- [ ] Update TODO.md
- [ ] Add documentation
- [ ] Test full flow
- [ ] Test error cases
- [ ] Optimize memory usage
- [ ] Verify security

## Estimated Effort

- **WiFi Manager Component**: 4-6 hours
- **WiFi Settings Screen**: 2-3 hours
- **WiFi Scan Screen**: 2-3 hours  
- **WiFi Password Screen**: 3-4 hours
- **Integration & Testing**: 3-4 hours
- **Total**: ~14-20 hours

## Dependencies

### ESP-IDF Components
- `esp_wifi` - WiFi driver
- `esp_event` - Event handling
- `esp_netif` - Network interface
- `nvs_flash` - Credential storage
- `lwip` - TCP/IP stack

### Project Components
- `settings_storage` - Credential storage
- `screen_manager` - Screen navigation (if available)
- `lvgl` - UI framework

## Next Steps

1. Complete wifi_manager.c implementation
2. Add CMakeLists.txt and Kconfig
3. Create wifi_settings screen
4. Create wifi_scan screen
5. Create wifi_password screen
6. Integrate with main app
7. Test thoroughly
8. Update documentation

## References

- ESP-IDF WiFi API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html
- LVGL Keyboard: https://docs.lvgl.io/master/widgets/keyboard.html
- LVGL List: https://docs.lvgl.io/master/widgets/list.html
- ESP-IDF Event Loop: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_event.html

---

**Status**: Planning Complete  
**Next**: Implementation of wifi_manager component  
**Priority**: High (required for NTP time sync)
