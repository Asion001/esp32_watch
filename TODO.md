# ESP32-C6 Watch Development TODO

**Project**: Open-source modular smartwatch firmware  
**Hardware**: Waveshare ESP32-C6 Touch AMOLED 2.06  
**Last Updated**: January 10, 2026

---

## 🎉 Phase 1: Core Functionality - ✅ COMPLETE

### Hardware Integration

- [x] Display initialization (AMOLED 2.06", 410×502)
- [x] Touch input (FT3168 capacitive)
- [x] RTC integration (PCF85063)
- [x] Battery monitoring (AXP2101 PMU)
- [x] I2C bus management

### Watchface Application

- [x] Digital clock with HH:MM display (Montserrat 48)
- [x] Date display (Day, Month DD format)
- [x] Battery percentage indicator
- [x] Charging status indicator
- [x] Color-coded battery levels (green/yellow/red)
- [x] Automatic 1-second updates
- [x] Voltage display alongside percentage

### Power Management (Basic)

- [x] Sleep manager component architecture
- [x] Light sleep mode on inactivity
- [x] Touch wake-up (GPIO15)
- [x] Display backlight control
- [x] LVGL timer pause/resume
- [x] LVGL rendering control
- [x] Modular configuration system (Kconfig)
- [x] Independent feature testing

### Documentation

- [x] README with quick start guide
- [x] BUILD.md for build instructions
- [x] QUICKSTART.md for developers
- [x] Component API documentation
- [x] Hardware pin configuration
- [x] SLEEP_CONFIG.md for power testing

---

## 🚧 Phase 2: Settings & WiFi Sync - Q1 2026

**Status**: In Progress  
**Priority**: High

### Other

- [x] Integrate build_time.h for compile-time RTC initialization
- [x] Save and show uptime to test battery consumption over time 

### Settings UI

- [x] Create settings app structure (`main/apps/settings/`)
- [x] Main settings menu screen
- [x] Navigation system (back button, menu items)
- [x] Settings storage layer (NVS wrapper)
- [x] Clear data/factory reset option
- [x] Reset uptime option
- [x] Show used flash/RAM statistics

### Display Settings

- [x] Brightness adjustment slider (0-100%)
- [x] Brightness persistence (save to NVS)
- [x] Sleep timeout configuration (5-300 seconds)
- [x] Display timeout setting

### WiFi Configuration

- [x] WiFi manager integration (component complete with 15 API functions)
- [x] SSID scanning and list display (UI screen complete)
- [x] Password input screen (virtual keyboard complete)
- [x] Connection status indicator (UI screen complete)
- [x] WiFi credential storage (secure NVS via settings_storage)
- [x] Auto-reconnect on boot (wifi_manager_auto_connect)

### Time Synchronization

- [ ] NTP client integration
- [ ] Manual NTP server configuration
- [ ] Time zone selection UI (dropdown/list)
- [ ] DST (Daylight Saving Time) support
- [ ] Automatic time sync on WiFi connect
- [ ] RTC auto-update from NTP
- [ ] Last sync timestamp display

### Storage System

- [ ] NVS partition initialization
- [ ] Settings persistence API
- [ ] Factory reset function
- [ ] Settings backup/restore
- [ ] Version migration support

### Testing & Debug

- [x] Save and show build time and version info
- [ ] Settings validation
- [ ] WiFi connection reliability tests
- [ ] NTP sync error handling
- [ ] NVS wear leveling verification

**Estimated Completion**: End of Q1 2026

---

## 📱 Phase 3: Additional Apps - Q2 2026

**Status**: Not Started  
**Priority**: Medium

### Stopwatch Application

- [ ] Create stopwatch app structure
- [ ] Start/Stop/Reset controls
- [ ] Lap time recording (up to 10 laps)
- [ ] Millisecond precision display
- [ ] Lap time list view
- [ ] Session save/load

### Timer/Countdown

- [ ] Timer app structure
- [ ] Time input UI (hours, minutes, seconds)
- [ ] Countdown display
- [ ] Pause/Resume functionality
- [ ] Timer completion alert (vibration + sound)
- [ ] Multiple timer presets
- [ ] Background timer notification

### Alarm Clock

- [ ] Alarm app structure
- [ ] Multiple alarms support (up to 5)
- [ ] Alarm time picker
- [ ] Repeat pattern (daily, weekdays, weekends, custom)
- [ ] Alarm label/name
- [ ] Snooze functionality (5/10/15 min)
- [ ] Alarm sound selection
- [ ] Vibration patterns

### Sensor Dashboard

- [ ] IMU driver (QMI8658 on I2C 0x6A)
- [ ] Accelerometer data display (X, Y, Z axes)
- [ ] Gyroscope data display
- [ ] Step counter algorithm
- [ ] Activity level indicator
- [ ] Temperature sensor reading (if available)
- [ ] Data visualization (graphs)

### Music Player Controls

- [ ] Adapt LVGL music demo
- [ ] Bluetooth audio profile integration
- [ ] Play/Pause/Next/Previous controls
- [ ] Volume control
- [ ] Track info display
- [ ] Album art support (if possible)

### App Launcher

- [ ] App grid/list view
- [ ] App icons
- [ ] App switching animation
- [ ] Recently used apps
- [ ] Swipe gestures for navigation

**Estimated Completion**: Mid Q2 2026

---

## ⚡ Phase 4: Advanced Power Management - Q2 2026

**Status**: Partially Complete  
**Priority**: High (battery life critical)

### Deep Sleep

- [ ] Deep sleep mode implementation
- [ ] Wake stub for faster wake-up
- [ ] State preservation across deep sleep
- [ ] Selective peripheral power-down
- [ ] Deep sleep vs light sleep decision logic
- [ ] Battery level-based sleep strategy

### IMU Gesture Detection

- [ ] QMI8658 motion interrupt configuration
- [ ] Wrist raise detection algorithm
- [ ] Tap detection (single/double tap)
- [ ] Shake detection
- [ ] Orientation detection
- [ ] Gesture-based wake-up from deep sleep
- [ ] False positive filtering

### Battery Optimization

- [ ] Dynamic CPU frequency scaling
- [ ] WiFi power save modes
- [ ] BLE connection interval optimization
- [ ] Display refresh rate adjustment
- [ ] Sensor polling rate optimization
- [ ] Battery profiler/analyzer tool

### Low-Power Watchface

- [ ] Simplified always-on watchface
- [ ] Reduced update frequency (1 min vs 1 sec)
- [ ] Monochrome/grayscale mode
- [ ] Minimal LVGL rendering
- [ ] Quick wake animation

### Power Monitoring

- [ ] Battery life estimator
- [ ] Power consumption statistics
- [ ] Per-app power usage tracking
- [ ] Low battery warnings
- [ ] Battery calibration routine

**Estimated Completion**: End of Q2 2026

---

## 🌟 Phase 5: Advanced Features - Q3 2026

**Status**: Not Started  
**Priority**: Low (nice-to-have)

### Multiple Watchfaces

- [ ] Analog clock watchface
- [ ] Minimal digital watchface
- [ ] Information-dense watchface
- [ ] Sport/fitness watchface
- [ ] Watchface selector UI
- [ ] Custom watchface API
- [ ] Watchface themes (colors, fonts)

### Weather Display

- [ ] Weather API integration (OpenWeatherMap?)
- [ ] Current conditions display
- [ ] 5-day forecast
- [ ] Weather icons
- [ ] Location-based weather
- [ ] Weather alert notifications
- [ ] Background weather updates

### Bluetooth Notifications

- [ ] BLE GATT server setup
- [ ] Notification protocol (ANCS or custom)
- [ ] Incoming call notification
- [ ] SMS/message preview
- [ ] App notification filtering
- [ ] Notification history
- [ ] Quick reply (canned responses)

### Fitness Tracking

- [ ] Step counting algorithm refinement
- [ ] Daily step goal
- [ ] Activity classification (walking/running/idle)
- [ ] Calories burned estimation
- [ ] Distance traveled calculation
- [ ] Weekly/monthly activity summary
- [ ] Activity streak tracking

### Calendar Integration

- [ ] Calendar event storage
- [ ] Event creation UI
- [ ] Event notifications/reminders
- [ ] Sync with phone calendar (BLE)
- [ ] Month/week/day views
- [ ] Recurring events support

**Estimated Completion**: End of Q3 2026

---

## 🛠️ Technical Debt & Improvements

### Code Quality

- [ ] Add unit tests for core components
- [ ] Increase code coverage (target >70%)
- [ ] Static analysis (cppcheck, clang-tidy)
- [ ] Memory leak detection
- [ ] Refactor large functions (>100 lines)
- [ ] Consistent error handling patterns

### Performance

- [ ] Profile LVGL rendering performance
- [ ] Optimize I2C transaction batching
- [ ] Reduce watchface update overhead
- [ ] Implement double buffering if needed
- [ ] Measure and optimize task stack usage

### Hardware Enhancements

- [ ] True display sleep (expose BSP panel_handle)
- [ ] Audio subsystem initialization
- [ ] Speaker/microphone testing
- [ ] ES8311/ES7210 driver integration
- [ ] Vibration motor driver (if present)
- [ ] Light sensor driver (if present)

### Build System

- [ ] CI/CD pipeline (GitHub Actions)
- [ ] Automated testing on commit
- [ ] Release builds with version tags
- [ ] OTA update mechanism
- [ ] Binary size optimization
- [ ] Partition table optimization

### Documentation

- [ ] API reference documentation (Doxygen)
- [ ] Architecture decision records (ADRs)
- [ ] Component interaction diagrams
- [ ] Video tutorials
- [ ] Troubleshooting guide expansion
- [ ] Contributing guidelines

---

## 🐛 Known Issues

### High Priority

- [ ] Display panel sleep requires BSP modification (panel_handle private)
- [ ] Touch interrupt sometimes misses first touch after sleep
- [ ] Battery percentage calculation needs calibration curve
- [ ] RTC drift compensation not implemented

### Medium Priority

- [ ] Long I2C transactions block UI thread
- [ ] LVGL demos consume significant flash space
- [ ] No watchdog timer for hang detection
- [ ] Stack overflow monitoring needed

### Low Priority

- [ ] Compiler warnings with -fanalyzer enabled
- [ ] CMake deprecation warning (version <3.10)
- [ ] Some LVGL symbols not available (font limitations)

---

## 📋 Testing Checklist

### Hardware Testing

- [ ] All I2C devices responding
- [ ] Touch calibration accuracy
- [ ] Display color accuracy
- [ ] Battery charging cycle test
- [ ] Power consumption measurements
- [ ] Temperature stress testing
- [ ] Long-term stability (24h+ runtime)

### Software Testing

- [ ] Boot time optimization
- [ ] Memory leak detection (valgrind equivalent)
- [ ] Task watchdog testing
- [ ] Sleep/wake cycle endurance (100+ cycles)
- [ ] RTC accuracy over 7 days
- [ ] NVS corruption resistance
- [ ] OTA update success/rollback

### User Experience

- [ ] Touch responsiveness
- [ ] Animation smoothness
- [ ] Readability in bright/dim light
- [ ] Battery life in real-world usage
- [ ] Gesture recognition accuracy
- [ ] WiFi connection time

---

## 💡 Future Ideas (Backlog)

### Hardware Additions

- [ ] External flash for watchface storage
- [ ] GPS module integration
- [ ] Heart rate sensor (MAX30102 or similar)
- [ ] SpO2 measurement
- [ ] ECG measurement (research only)

### Software Features

- [ ] Voice assistant integration
- [ ] Calculator app
- [ ] Compass (using IMU magnetometer if available)
- [ ] Flashlight mode (white screen max brightness)
- [ ] QR code generator/scanner
- [ ] Pomodoro timer
- [ ] Habit tracker
- [ ] Meditation timer
- [ ] Remote camera shutter (BLE)
- [ ] Find my phone feature

### Advanced Features

- [ ] Custom scripting language for watchfaces
- [ ] Plugin system for third-party apps
- [ ] Cloud sync for settings/data
- [ ] Multi-device pairing
- [ ] Gesture customization
- [ ] Voice commands
- [ ] Always-on display (AOD) with OLED burn-in protection

---

## 📊 Project Metrics

### Current Status (Phase 1 Complete)

- **Lines of Code**: ~3,500 (estimated)
- **Components**: 6 (main, watchface, rtc, pmu, bsp, sleep_manager)
- **Binary Size**: 671 KB (92% flash available)
- **RAM Usage**: ~144 KB DRAM, 88 bytes LP SRAM
- **Features Implemented**: 20+
- **Documentation Pages**: 6

### Phase 2 Goals

- **Target Binary Size**: <800 KB
- **New Components**: +4 (settings, wifi_manager, ntp_client, nvs_wrapper)
- **Documentation**: +3 pages
- **Test Coverage**: >50%

---

## 🔄 Version History

### v0.1.0 - Current (January 10, 2026)

- Initial release
- Basic watchface with RTC and battery
- Sleep manager (modular, disabled by default)
- Touch wake-up functionality

### v0.2.0 - Planned (Q1 2026)

- Settings menu
- WiFi configuration
- NTP time sync
- Persistent storage

### v0.3.0 - Planned (Q2 2026)

- Stopwatch, Timer, Alarm apps
- Sensor dashboard
- IMU gesture detection

### v1.0.0 - Planned (Q3 2026)

- Feature complete
- Stable API
- Production ready

---

## 🎯 Next Actions (Immediate)

1. **Test sleep manager** - Enable features via menuconfig and verify each works
2. **Measure power consumption** - Validate sleep mode power savings with multimeter
3. **Fix known issues** - Address touch interrupt reliability
4. **Plan Phase 2** - Design settings UI mockups
5. **Setup CI** - Automate builds and basic tests

---

## 📝 Notes

- Keep components modular and independently testable
- Prioritize battery life over features
- Maintain backward compatibility within major versions
- Document all hardware-specific workarounds
- Test on actual hardware before merging to main

---

**Contributing**: See individual phase sections for task details. Pick any unchecked item and submit a PR!

**Questions**: Open an issue on GitHub or check existing documentation.
