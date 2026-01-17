# ESP32-C6 Watch TODO

**Last Updated**: January 17, 2026

Short list of what’s next. Implementation later.

---

## Now (Highest Priority)

- [ ] Sleep & power management polish
  - [x] Ensure reliable light sleep entry/exit
  - [x] Fix any wake issues after sleep (touch or display)
  - [x] Confirm LVGL timers/rendering pause/resume paths are safe
  - [ ] Measure power draw for key scenarios (idle, active, charging)
    - [ ] Requires external multimeter/bench supply; use power logs for timing

- [ ] Power optimization steps (from docs)
  - [ ] Measure actual power consumption with multimeter
  - [ ] Adjust sleep timeout and backlight timeout for real usage
  - [ ] Enable full sleep: backlight off + LVGL timer pause + LVGL rendering off
  - [ ] Validate wake sources (touch GPIO15, boot button GPIO9)
  - [x] Add sleep mode indicator UI (countdown before sleep)
  - [ ] Enable deep sleep after extended inactivity
  - [ ] Suspend/resume WiFi around sleep to reduce idle draw
  - [ ] Power down display panel (not just backlight) if BSP allows
  - [ ] Add sensor/rail power gating (IMU, peripherals) via PMU
  - [ ] Disable charging during power measurement sessions

- [x] Settings: show live WiFi connection status
  - [x] Status label updates on connect/disconnect
  - [x] Display IP / RSSI when connected (if available)

---

## Next (Short Term)

- [ ] Deep sleep strategy
  - [x] Decide when to use deep sleep vs light sleep
  - [x] Preserve state across deep sleep

- [ ] OTA updates
  - [x] Settings UI for triggering OTA and checking new versions

- [ ] Battery calibration
  - [ ] Improve percentage curve accuracy

- [ ] Reliability checks
  - [x] Watchdog for hang detection
  - [x] Avoid long I2C blocks on UI thread

- [ ] Update or cleanup docs/\*.md with latest features and instructions

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

## Notes (Recent Work)

- Sleep manager: improved wake logging for GPIO sources (button/touch).
- Button handler: reset sleep timer and wake backlight on button press.
- Sleep manager: avoid touch handler interference during sleep entry.
- Sleep manager: handle display lock failures during sleep/wake transitions.
- Sleep manager: prevent sleep entry when wake GPIO is already active.
- Sleep manager: add backlight helper stubs when disabled.
- Sleep manager: abort sleep/wake if LVGL display is missing.
- Watchface: move RTC/battery I2C reads to background task.
- Sleep manager: handle light sleep start failures cleanly.
- Sleep manager: add optional power diagnostics logging.
- Sleep manager: add display lock retry for sleep/wake.
- App: add task watchdog option (configurable).
- Sleep manager: add deep sleep escalation after extended inactivity.
- App: restore tile state after deep sleep.

---

## Documentation

- [x] Consolidate Markdown docs in docs/
