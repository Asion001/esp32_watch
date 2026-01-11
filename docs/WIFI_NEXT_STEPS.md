# WiFi Implementation - Next Steps

## Status: Ready for Implementation

The WiFi Configuration feature has been fully planned (see `WIFI_IMPLEMENTATION_PLAN.md`) and is ready for implementation. However, this is a substantial feature requiring 14-20 hours of focused development work.

## Why WiFi Wasn't Implemented in This Session

WiFi implementation requires:
- ~500+ lines of component code (wifi_manager.c)
- ESP-IDF WiFi API integration (complex event handling)
- 3 complete UI screens with LVGL widgets
- Extensive testing on hardware
- Security considerations (credential encryption)
- Error handling for many edge cases

This is beyond the scope of a single development session and should be tackled as a focused, dedicated implementation effort.

## Recommended Approach

### Option 1: Incremental Implementation (Recommended)

Break WiFi implementation into 5 separate PRs/commits:

**PR 1: WiFi Manager Component Core** (4-6 hours)
- Create `components/wifi_manager/wifi_manager.c/h`
- Implement initialization and basic state management
- Add event handling for WiFi events
- Test basic WiFi init/deinit

**PR 2: Scanning Functionality** (2-3 hours)
- Implement `wifi_manager_scan_start()`
- Add scan result caching and parsing
- Implement `wifi_manager_get_scan_results()`
- Test scanning on hardware

**PR 3: Connection Management** (3-4 hours)
- Implement `wifi_manager_connect()`
- Add connection state machine
- Implement credential storage via settings_storage
- Add `wifi_manager_auto_connect()`
- Test connection to various networks

**PR 4: WiFi UI Screens** (5-7 hours)
- Create wifi_settings screen (status, scan button)
- Create wifi_scan screen (AP list with LVGL)
- Create wifi_password screen (LVGL keyboard)
- Wire up navigation
- Test full user flow

**PR 5: Integration & Polish** (2-3 hours)
- Add auto-connect on boot
- Add WiFi icon to watchface (optional)
- Error handling and user feedback
- Final testing and documentation

### Option 2: All-at-Once Implementation (Advanced)

If you're experienced with ESP-IDF WiFi APIs and LVGL:
- Use the detailed plan in `WIFI_IMPLEMENTATION_PLAN.md`
- Implement all phases in one go
- Allocate 2-3 days for development
- Test thoroughly on hardware
- Document any deviations from plan

### Option 3: Use Existing ESP-IDF Examples

ESP-IDF provides WiFi provisioning examples that can be adapted:
- `esp-idf/examples/wifi/getting_started/station/`
- `esp-idf/examples/provisioning/wifi_prov_mgr/`
- Adapt these for the smartwatch use case
- Integrate with existing settings_storage

## What's Already Done

✅ **Planning Complete**:
- API specification (14 functions defined)
- Architecture designed
- Security considerations documented
- Memory management strategy
- Testing requirements
- UI screen mockups

✅ **Dependencies Ready**:
- settings_storage component (for credentials)
- screen_manager (for navigation)
- LVGL (for UI)

✅ **Documentation**:
- Complete implementation guide
- Code examples
- Best practices

## What's Needed

### 1. WiFi Manager Component

**File**: `components/wifi_manager/wifi_manager.c` (~400-500 lines)

**Key Functions to Implement**:
```c
esp_err_t wifi_manager_init(void) {
    // 1. Initialize ESP-IDF netif
    // 2. Create default WiFi station
    // 3. Initialize WiFi with default config
    // 4. Register event handlers
    // 5. Start WiFi in station mode
    // 6. Load saved credentials if available
}

esp_err_t wifi_manager_scan_start(void) {
    // 1. Check WiFi initialized
    // 2. Configure scan parameters
    // 3. Start async scan
    // 4. Return immediately (results via event)
}

esp_err_t wifi_manager_connect(const char *ssid, 
                                 const char *password, 
                                 bool save) {
    // 1. Validate inputs
    // 2. Set WiFi config (SSID, password, auth mode)
    // 3. Call esp_wifi_connect()
    // 4. If save==true, store to settings_storage
    // 5. Set state to CONNECTING
    // 6. Wait for connection (or timeout)
}

static void wifi_event_handler(void *arg, 
                                 esp_event_base_t event_base,
                                 int32_t event_id, 
                                 void *event_data) {
    // Handle:
    // - WIFI_EVENT_STA_START
    // - WIFI_EVENT_STA_CONNECTED
    // - WIFI_EVENT_STA_DISCONNECTED
    // - WIFI_EVENT_SCAN_DONE
    // - IP_EVENT_STA_GOT_IP
    // Update state, call user callbacks
}
```

**ESP-IDF APIs to Use**:
- `esp_netif_init()`
- `esp_event_loop_create_default()`
- `esp_netif_create_default_wifi_sta()`
- `esp_wifi_init()`
- `esp_wifi_set_mode(WIFI_MODE_STA)`
- `esp_wifi_set_config()`
- `esp_wifi_start()`
- `esp_wifi_scan_start()`
- `esp_wifi_scan_get_ap_records()`
- `esp_wifi_connect()`
- `esp_wifi_disconnect()`
- `esp_event_handler_register()`

### 2. WiFi Settings Screen

**File**: `main/apps/settings/screens/wifi_settings.c` (~200 lines)

**UI Elements**:
- Status label (Connected/Disconnected/Connecting...)
- Current SSID label (when connected)
- Signal strength indicator
- IP address display
- "Scan for Networks" button
- "Disconnect" button (when connected)
- "Forget Network" button (when connected)
- Back button

**Pseudo-code**:
```c
lv_obj_t *wifi_settings_create(lv_obj_t *parent) {
    // Create screen container
    // Add title "WiFi"
    // Add back button
    
    // Create status section
    status_label = lv_label_create(...);
    lv_label_set_text(status_label, "Not Connected");
    
    // Create connection info (hidden initially)
    ssid_label = lv_label_create(...);
    signal_label = lv_label_create(...);
    ip_label = lv_label_create(...);
    
    // Create action buttons
    scan_btn = lv_btn_create(...);
    lv_obj_add_event_cb(scan_btn, scan_button_cb, LV_EVENT_CLICKED, NULL);
    
    disconnect_btn = lv_btn_create(...);
    // Initially hidden, show when connected
    
    // Update UI based on current WiFi state
    update_wifi_ui();
    
    return screen;
}

static void scan_button_cb(lv_event_t *e) {
    // Show loading indicator
    // Call wifi_manager_scan_start()
    // Navigate to wifi_scan screen
    screen_manager_switch(wifi_scan_screen);
}
```

### 3. WiFi Scan Results Screen

**File**: `main/apps/settings/screens/wifi_scan.c` (~250 lines)

**UI Elements**:
- LVGL list widget (`lv_list_create()`)
- Each AP as list item with:
  - SSID text
  - Signal strength bars/icon
  - Security indicator (🔒 or 🔓)
- Refresh button
- Back button
- Loading indicator during scan

**Pseudo-code**:
```c
lv_obj_t *wifi_scan_create(lv_obj_t *parent) {
    // Create screen
    // Add title "Select Network"
    // Add back button
    
    // Create list widget
    ap_list = lv_list_create(parent);
    lv_obj_set_size(ap_list, LV_PCT(90), LV_PCT(75));
    
    // Add refresh button
    refresh_btn = lv_btn_create(...);
    lv_obj_add_event_cb(refresh_btn, refresh_cb, LV_EVENT_CLICKED, NULL);
    
    // Populate list with scan results
    populate_ap_list();
    
    return screen;
}

static void populate_ap_list(void) {
    wifi_ap_record_simple_t ap_list[20];
    uint16_t num_aps = 0;
    
    wifi_manager_get_scan_results(ap_list, 20, &num_aps);
    
    // Sort by RSSI
    qsort(ap_list, num_aps, sizeof(wifi_ap_record_simple_t), compare_rssi);
    
    // Add each AP to list
    for (int i = 0; i < num_aps; i++) {
        lv_obj_t *item = lv_list_add_btn(ap_list_widget, 
                                          get_security_icon(ap_list[i]),
                                          ap_list[i].ssid);
        lv_obj_add_event_cb(item, ap_select_cb, LV_EVENT_CLICKED, &ap_list[i]);
    }
}

static void ap_select_cb(lv_event_t *e) {
    wifi_ap_record_simple_t *ap = lv_event_get_user_data(e);
    
    // Store selected SSID globally
    selected_ssid = ap->ssid;
    
    if (ap->is_open) {
        // Connect directly (no password needed)
        wifi_manager_connect(ap->ssid, NULL, true);
    } else {
        // Navigate to password screen
        screen_manager_switch(wifi_password_screen);
    }
}
```

### 4. WiFi Password Input Screen

**File**: `main/apps/settings/screens/wifi_password.c` (~300 lines)

**UI Elements**:
- SSID label showing selected network
- Password text area (with show/hide)
- LVGL keyboard widget
- "Save credentials" checkbox
- "Connect" button
- "Cancel" button

**Pseudo-code**:
```c
lv_obj_t *wifi_password_create(lv_obj_t *parent) {
    // Create screen
    // Add title with SSID
    ssid_label = lv_label_create(...);
    lv_label_set_text_fmt(ssid_label, "Connect to: %s", selected_ssid);
    
    // Create password text area
    password_ta = lv_textarea_create(parent);
    lv_textarea_set_password_mode(password_ta, true);
    lv_textarea_set_max_length(password_ta, 64);
    
    // Create LVGL keyboard
    keyboard = lv_keyboard_create(parent);
    lv_keyboard_set_textarea(keyboard, password_ta);
    
    // Add show/hide password button
    show_btn = lv_btn_create(...);
    lv_obj_add_event_cb(show_btn, toggle_password_visibility, ...);
    
    // Add save checkbox
    save_cb = lv_checkbox_create(parent);
    lv_checkbox_set_text(save_cb, "Save credentials");
    lv_obj_add_state(save_cb, LV_STATE_CHECKED); // Checked by default
    
    // Add connect button
    connect_btn = lv_btn_create(...);
    lv_obj_add_event_cb(connect_btn, connect_button_cb, LV_EVENT_CLICKED, NULL);
    
    // Add cancel button
    cancel_btn = lv_btn_create(...);
    lv_obj_add_event_cb(cancel_btn, cancel_button_cb, LV_EVENT_CLICKED, NULL);
    
    return screen;
}

static void connect_button_cb(lv_event_t *e) {
    const char *password = lv_textarea_get_text(password_ta);
    bool save = lv_obj_has_state(save_cb, LV_STATE_CHECKED);
    
    // Validate password length
    if (strlen(password) < 8) {
        show_error_msgbox("Password must be at least 8 characters");
        return;
    }
    
    // Show connecting message
    show_loading_msgbox("Connecting...");
    
    // Attempt connection
    esp_err_t ret = wifi_manager_connect(selected_ssid, password, save);
    
    if (ret == ESP_OK) {
        // Clear password from memory
        memset((void*)password, 0, strlen(password));
        
        // Navigate back to settings
        screen_manager_switch(settings_screen);
    } else {
        hide_loading_msgbox();
        show_error_msgbox("Connection failed. Check password and try again.");
    }
}
```

## Testing Checklist

Before marking WiFi as complete, test:

- [ ] WiFi scan shows nearby networks
- [ ] Can connect to open network
- [ ] Can connect to WPA2 network with correct password
- [ ] Wrong password shows error and allows retry
- [ ] Credentials save to NVS when checkbox checked
- [ ] Auto-reconnect works after reboot
- [ ] Disconnect button works
- [ ] Forget network clears credentials
- [ ] Signal strength displays accurately
- [ ] IP address shows when connected
- [ ] Connection survives sleep/wake cycles
- [ ] Handle no networks found gracefully
- [ ] Handle scan timeout gracefully
- [ ] Handle connection timeout (wrong password)
- [ ] Handle DHCP failure
- [ ] Memory doesn't leak after multiple connect/disconnect cycles

## Additional Considerations

### Security

- Use `esp_wifi_set_storage(WIFI_STORAGE_RAM)` to avoid storing passwords in flash
- Store credentials only via settings_storage (which uses encrypted NVS if configured)
- Clear password buffers after use: `memset(password, 0, len)`
- Never log passwords
- Validate SSID and password lengths

### Power Management

- Enable WiFi power save: `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)`
- Disconnect WiFi when not needed
- Provide option to disable auto-reconnect

### Error Messages

Provide helpful, specific error messages:
- "Wrong password" (not just "connection failed")
- "Network not found"
- "No IP address (DHCP timeout)"
- "WiFi scan failed"
- "Already connected"

### Memory

- LVGL keyboard is memory-intensive (~50KB heap)
- Create keyboard only when needed
- Destroy after use to free memory
- Monitor heap with `esp_get_free_heap_size()`

## Resources

- [ESP-IDF WiFi Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html)
- [ESP-IDF Event Loop](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_event.html)
- [LVGL Keyboard Widget](https://docs.lvgl.io/master/widgets/keyboard.html)
- [LVGL List Widget](https://docs.lvgl.io/master/widgets/list.html)
- Existing planning doc: `WIFI_IMPLEMENTATION_PLAN.md`

## Summary

WiFi implementation is a **substantial feature** that requires:
- Deep ESP-IDF knowledge
- LVGL UI development
- Hardware testing
- Security considerations

It's recommended to tackle this as a **dedicated, focused effort** rather than part of a mixed development session. The planning phase is complete; implementation can begin when ready.

---

**Next After WiFi**: Once WiFi is working, implement NTP Time Synchronization (Phase 2, next section in TODO.md).
