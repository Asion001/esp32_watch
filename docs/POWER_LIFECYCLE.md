# Power Lifecycle and Module States

This document describes how the firmware manages power across system states and how each major module behaves. It is based on current implementation in the sleep manager, WiFi manager, and PMU drivers.

## Overview

The system uses a lightweight **sleep manager** to transition between normal operation, display/backlight idle, light sleep, and optional deep sleep. Power-related behavior is controlled by menuconfig (Kconfig) options and is guarded with `CONFIG_*` flags.

Key components involved:

- **Sleep manager**: Activity timeouts, display sleep, light sleep, deep sleep, wake sources.
- **Backlight/display**: Backlight is toggled via BSP; panel power-down is not currently performed.
- **WiFi manager**: Explicit start/stop and power-save mode when active.
- **PMU (AXP2101)**: Battery/charging status and USB VBUS detection.
- **RTC (PCF85063)**: External RTC for timekeeping; RTC peripherals are kept powered during sleep.

## System Power States

| State                        | Entry Trigger                                                                 | CPU         | Display/Backlight          | LVGL Timers       | LVGL Rendering      | WiFi                | Wake Sources                          |
| ---------------------------- | ----------------------------------------------------------------------------- | ----------- | -------------------------- | ----------------- | ------------------- | ------------------- | ------------------------------------- |
| **Boot**                     | Reset / power-on                                                              | Active      | On                         | Running           | Enabled             | Off by default      | N/A                                   |
| **Active**                   | Normal usage                                                                  | Active      | On                         | Running           | Enabled             | As requested by app | Touch, button                         |
| **Idle (backlight timeout)** | Inactivity for `CONFIG_SLEEP_MANAGER_BACKLIGHT_TIMEOUT_SECONDS`               | Active      | Backlight off (if enabled) | Running           | Enabled             | As requested by app | Touch, button                         |
| **Light Sleep**              | Inactivity for `CONFIG_SLEEP_TIMEOUT_SECONDS`                                 | Light sleep | Backlight off (if enabled) | Paused (optional) | Disabled (optional) | No automatic change | Button GPIO9; Touch GPIO15 (optional) |
| **Deep Sleep**               | Inactivity for `CONFIG_SLEEP_MANAGER_DEEP_SLEEP_TIMEOUT_SECONDS` (if enabled) | Deep sleep  | Off                        | Stopped           | Disabled            | Off                 | Button GPIO9; Touch GPIO15 (optional) |

Notes:

- **Sleep manager is disabled by default** (`CONFIG_SLEEP_MANAGER_ENABLE=n`).
- **Deep sleep is optional** and disabled by default.
- **USB/JTAG connected** can block sleep and/or backlight-off depending on config.

## Sleep Manager Controls (menuconfig)

Menu: **Component config → App: Sleep Manager**

| Config                                            | Purpose                         | Default |
| ------------------------------------------------- | ------------------------------- | ------- |
| `CONFIG_SLEEP_MANAGER_ENABLE`                     | Master sleep manager switch     | `n`     |
| `CONFIG_SLEEP_MANAGER_BACKLIGHT_TIMEOUT_SECONDS`  | Backlight timeout               | `10`    |
| `CONFIG_SLEEP_TIMEOUT_SECONDS`                    | Light sleep timeout             | `30`    |
| `CONFIG_SLEEP_MANAGER_DEEP_SLEEP_ENABLE`          | Enable deep sleep               | `n`     |
| `CONFIG_SLEEP_MANAGER_DEEP_SLEEP_TIMEOUT_SECONDS` | Deep sleep timeout              | `300`   |
| `CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL`          | Backlight control               | `y`     |
| `CONFIG_SLEEP_MANAGER_LVGL_TIMER_PAUSE`           | Pause LVGL timers in sleep      | `y`     |
| `CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL`     | Disable LVGL rendering in sleep | `y`     |
| `CONFIG_SLEEP_MANAGER_GPIO_WAKEUP`                | Enable GPIO wake                | `y`     |
| `CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP`               | Touch wake (GPIO15)             | `y`     |
| `CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER`          | Reset inactivity timer on touch | `y`     |
| `CONFIG_SLEEP_MANAGER_PREVENT_SLEEP_ON_USB`       | Block sleep on USB VBUS         | `y`     |
| `CONFIG_SLEEP_MANAGER_PREVENT_SCREEN_OFF_ON_USB`  | Block backlight off on USB      | `n`     |
| `CONFIG_SLEEP_MANAGER_DEBUG_LOGS`                 | Debug logs                      | `n`     |
| `CONFIG_SLEEP_MANAGER_POWER_LOGS`                 | Battery/power logs              | `n`     |

## Module Power States

### Backlight / Display

- **On**: Normal operation, backlight enabled.
- **Off**: Backlight disabled (screen content still present but not visible).
- **Notes**:
  - Backlight is controlled with `bsp_display_backlight_on/off()`.
  - The LCD panel is **not** explicitly powered down; `esp_lcd_panel_disp_on_off()` is not called.
  - Backlight-off can be **blocked when USB VBUS is present** if `CONFIG_SLEEP_MANAGER_PREVENT_SCREEN_OFF_ON_USB=y`.

### CPU / SoC Power

- **Active**: Normal FreeRTOS scheduling.
- **Light Sleep**: `esp_light_sleep_start()`; blocks until wake event.
- **Deep Sleep**: `esp_deep_sleep_start()` (optional).
- **RTC Peripherals**: Sleep manager keeps `ESP_PD_DOMAIN_RTC_PERIPH` **ON** during sleep to support RTC/timers.

### LVGL

- **Timers running**: Normal updates.
- **Timers paused**: Optional; controlled by `CONFIG_SLEEP_MANAGER_LVGL_TIMER_PAUSE`.
- **Rendering disabled**: Optional; controlled by `CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL`.

### WiFi

- **Off**: `wifi_manager_deinit()` stops WiFi and frees resources.
- **On (station mode)**: `wifi_manager_init()` starts WiFi with `WIFI_PS_MIN_MODEM` power-save mode.
- **States**: `DISCONNECTED`, `CONNECTING`, `CONNECTED`, `FAILED`, `SCANNING`.
- **Notes**:
  - Sleep manager does **not** automatically disable WiFi when sleeping.
  - Scans are active and consume more power; they temporarily disconnect if connected.

### RTC (PCF85063)

- External RTC used for timekeeping.
- RTC peripherals are kept powered during sleep via `ESP_PD_DOMAIN_RTC_PERIPH`.
- Time is preserved across light sleep and deep sleep.

### PMU (AXP2101)

- Provides battery voltage/percentage and charging status.
- Detects USB VBUS for **sleep/backlight blocking** and power logs.
- Charging can be enabled/disabled via `axp2101_set_charging_enabled()` (useful for power measurement).

### Touch (FT3168)

- Touch interrupt GPIO (`GPIO15`) is an optional wake source.
- Touch input can reset the inactivity timer when enabled (`CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER=y`).

### Boot Button

- Boot button (`GPIO9`) is always a wake source when GPIO wake is enabled.

## Power Lifecycle Flow

1. **Boot** → System initializes core services and UI.
2. **Active** → User interaction keeps inactivity timer reset.
3. **Backlight timeout** → Backlight off (optional) while CPU remains active.
4. **Light sleep** → LVGL timers optionally paused; rendering optionally disabled; CPU in light sleep.
5. **Wake** → Touch or button wakes CPU; backlight restored; LVGL timers resume.
6. **Deep sleep (optional)** → After extended inactivity, CPU enters deep sleep.

## Known Limitations / Current Behavior

- No automatic WiFi suspend/resume on sleep transitions.
- Display panel is not fully powered down (backlight-only sleep).
- Sensor power gating (IMU, etc.) is not implemented yet; relies on PMU rails.

## Related Docs

- Sleep manager configuration: [docs/SLEEP_CONFIG.md](SLEEP_CONFIG.md)
- PMU register map: [docs/AXP2101_REGISTER_MAP.md](AXP2101_REGISTER_MAP.md)
