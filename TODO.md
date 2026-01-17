# ESP32-C6 Watch TODO

**Last Updated**: January 17, 2026

Short list of what’s next. Implementation later.

---

## Now (Highest Priority)

- [ ] Sleep & power management polish
  - [ ] Ensure reliable light sleep entry/exit
  - [ ] Fix any wake issues after sleep (touch or display)
  - [ ] Confirm LVGL timers/rendering pause/resume paths are safe
  - [ ] Measure power draw for key scenarios (idle, active, charging)

- [ ] Settings: show live WiFi connection status
  - [ ] Status label updates on connect/disconnect
  - [ ] Display IP / RSSI when connected (if available)

---

## Next (Short Term)

- [ ] Deep sleep strategy
  - [ ] Decide when to use deep sleep vs light sleep
  - [ ] Preserve state across deep sleep

- [ ] Battery calibration
  - [ ] Improve percentage curve accuracy

- [ ] Reliability checks
  - [ ] Watchdog for hang detection
  - [ ] Avoid long I2C blocks on UI thread

---

## Later (Backlog)

### Refactor Tasks

- [ ] Extract common settings UI components (lists, toggles, sliders)
- [ ] Create a shared settings data model + persistence helpers
- [ ] Build a screen navigation stack (push/pop, back, transitions)
- [ ] Centralize event routing for settings screens
- [ ] Add a background task framework (periodic jobs, NTP sync)
- [ ] Standardize error/status banners across screens

### New Features

- [ ] IMU gesture wake-up
- [ ] WiFi/NTP enhancements (error handling + retry policies)
- [ ] Weather display via WiFi API
- [ ] Oled burn-in mitigation strategies
- [ ] Additional apps (stopwatch, timer, alarms)
- [ ] Possible usage of low-power cpu core in esp32-c6
- [ ] Settings for time and date size on watchface
- [ ] Multiple watchfaces
- [ ] User documentation for settings and features

---

## Documentation

- [x] Consolidate Markdown docs in docs/
