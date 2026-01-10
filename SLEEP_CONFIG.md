# Sleep Manager Configuration Guide

## Overview

Sleep manager features are now **modular and disabled by default**. Each feature can be enabled independently via `idf.py menuconfig` for incremental testing.

## Quick Start

### 1. Access Configuration Menu

```bash
idf.py menuconfig
```

Navigate to: **Component config → Sleep Manager Configuration**

### 2. Configuration Options

| Option                              | Description                          | Default | Dependencies          |
| ----------------------------------- | ------------------------------------ | ------- | --------------------- |
| **Enable Sleep Manager**            | Master switch for all sleep features | **OFF** | None                  |
| Sleep timeout (seconds)             | Inactivity period before sleep       | 10s     | Sleep Manager enabled |
| Control display backlight           | Turn off backlight during sleep      | ON      | Sleep Manager enabled |
| Pause LVGL timers during sleep      | Stop watchface updates in sleep      | ON      | Sleep Manager enabled |
| Disable LVGL rendering during sleep | Stop frame rendering                 | ON      | Sleep Manager enabled |
| Enable GPIO wake-up                 | Wake on touch (GPIO15)               | ON      | Sleep Manager enabled |
| Reset timer on touch events         | Touch resets inactivity timer        | ON      | Sleep Manager enabled |
| Enable debug logging                | Detailed sleep manager logs          | OFF     | Sleep Manager enabled |

## Testing Scenarios

### Scenario 1: Completely Disabled (Default)

**Purpose**: Baseline testing - no sleep functionality

```
[✓] All features: OFF
```

**Expected behavior**:

- Watchface runs continuously
- No sleep-related logs
- Battery drains at normal rate
- No touch event processing for sleep

**Build command**:

```bash
idf.py build flash monitor
```

---

### Scenario 2: Basic Sleep Only

**Purpose**: Test sleep entry without display/LVGL control

```
[✓] Enable Sleep Manager: ON
[✗] Control display backlight: OFF
[✗] Pause LVGL timers during sleep: OFF
[✗] Disable LVGL rendering during sleep: OFF
[✓] Enable GPIO wake-up: ON
[✓] Reset timer on touch events: ON
    Sleep timeout: 10 seconds
```

**Expected behavior**:

- Device enters light sleep after 10s inactivity
- Display stays on (backlight not controlled)
- Watchface continues updating (timers not paused)
- Touch wakes device
- Log: "Entering light sleep..." → "Woke from light sleep"

**Test steps**:

1. Flash and monitor
2. Wait 10 seconds without touching
3. Observe log message "Entering sleep mode..."
4. Touch screen to wake
5. Verify "Woke from light sleep" log

---

### Scenario 3: Display Backlight Only

**Purpose**: Test backlight control without LVGL changes

```
[✓] Enable Sleep Manager: ON
[✓] Control display backlight: ON
[✗] Pause LVGL timers during sleep: OFF
[✗] Disable LVGL rendering during sleep: OFF
[✓] Enable GPIO wake-up: ON
[✓] Reset timer on touch events: ON
```

**Expected behavior**:

- Display turns off after 10s
- Watchface continues updating in background
- Touch wakes and restores backlight
- Log: "Display sleep (backlight off)" → "Display wake (backlight on)"

---

### Scenario 4: LVGL Timer Pause

**Purpose**: Test stopping watchface updates during sleep

```
[✓] Enable Sleep Manager: ON
[✓] Control display backlight: ON
[✓] Pause LVGL timers during sleep: ON
[✗] Disable LVGL rendering during sleep: OFF
[✓] Enable GPIO wake-up: ON
[✓] Reset timer on touch events: ON
```

**Expected behavior**:

- Display off + watchface stops updating
- Time "freezes" during sleep
- On wake, time jumps to current time
- Log: "Paused X LVGL timers" → "Resumed X LVGL timers"

---

### Scenario 5: Full LVGL Control

**Purpose**: Test complete LVGL sleep (timers + rendering)

```
[✓] Enable Sleep Manager: ON
[✓] Control display backlight: ON
[✓] Pause LVGL timers during sleep: ON
[✓] Disable LVGL rendering during sleep: ON
[✓] Enable GPIO wake-up: ON
[✓] Reset timer on touch events: ON
```

**Expected behavior**:

- Maximum power savings
- Display off, no updates, no rendering
- Log: "LVGL rendering disabled" → "LVGL rendering enabled"

---

### Scenario 6: No Wake-Up (Infinite Sleep)

**Purpose**: Test sleep without wake mechanism (requires serial or reset to wake)

```
[✓] Enable Sleep Manager: ON
[✓] Control display backlight: ON
[✓] Pause LVGL timers during sleep: ON
[✗] Enable GPIO wake-up: OFF
[✓] Reset timer on touch events: ON
```

**Expected behavior**:

- Device enters sleep after 10s
- Touch does NOT wake device
- Must press reset button or disconnect USB to wake
- Log: "GPIO wake-up disabled"

**⚠️ Warning**: Device will appear frozen - this is for testing only!

---

### Scenario 7: No Touch Reset (Auto-Sleep)

**Purpose**: Test automatic sleep regardless of touch activity

```
[✓] Enable Sleep Manager: ON
[✓] Control display backlight: ON
[✓] Pause LVGL timers during sleep: ON
[✗] Reset timer on touch events: OFF
[✓] Enable GPIO wake-up: ON
```

**Expected behavior**:

- Device sleeps after 10s even if you keep touching
- Touch still wakes from sleep, but doesn't prevent it
- Useful for testing wake-up without timer reset

---

### Scenario 8: Full Featured (Production)

**Purpose**: Full sleep functionality with all features

```
[✓] Enable Sleep Manager: ON
[✓] Control display backlight: ON
[✓] Pause LVGL timers during sleep: ON
[✓] Disable LVGL rendering during sleep: ON
[✓] Enable GPIO wake-up: ON
[✓] Reset timer on touch events: ON
[✗] Enable debug logging: OFF
    Sleep timeout: 10 seconds
```

**Expected behavior**:

- Complete sleep/wake cycle
- Optimal power savings
- Normal user experience

---

### Scenario 9: Debug Mode

**Purpose**: Maximum logging for troubleshooting

```
[✓] Enable Sleep Manager: ON
[✓] All sub-features: ON
[✓] Enable debug logging: ON
```

**Expected behavior**:

- Same as Scenario 8
- Additional debug logs:
  - "Activity timer reset" on each touch
  - "Paused timer 0", "Paused timer 1", etc.
  - "Resumed timer 0", etc.

---

## Modifying Configuration

### Via menuconfig (Recommended)

```bash
idf.py menuconfig
# Navigate to Component config → Sleep Manager Configuration
# Use spacebar to toggle options
# Press 'S' to save, then 'Q' to quit
idf.py build flash monitor
```

### Via sdkconfig File (Advanced)

Edit `sdkconfig` manually:

```
CONFIG_SLEEP_MANAGER_ENABLE=y
CONFIG_SLEEP_TIMEOUT_SECONDS=10
CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL=y
CONFIG_SLEEP_MANAGER_LVGL_TIMER_PAUSE=y
CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL=y
CONFIG_SLEEP_MANAGER_GPIO_WAKEUP=y
CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER=y
CONFIG_SLEEP_MANAGER_DEBUG_LOGS=n
```

Then rebuild:

```bash
idf.py build flash monitor
```

---

## Troubleshooting

### Issue: Sleep never triggers

**Check**:

- Is `CONFIG_SLEEP_MANAGER_ENABLE=y`?
- Is `CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER=y`? (If yes, touch resets timer)
- Look for log: "Sleep manager disabled in configuration"

### Issue: Can't wake from sleep

**Check**:

- Is `CONFIG_SLEEP_MANAGER_GPIO_WAKEUP=y`?
- Look for log: "GPIO wake-up disabled"
- Try pressing reset button

### Issue: Display stays on during sleep

**Check**:

- Is `CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL=y`?
- Look for log: "Display sleep (backlight control disabled)"

### Issue: Too many logs

**Solution**:

- Set `CONFIG_SLEEP_MANAGER_DEBUG_LOGS=n`

### Issue: Sleep timeout too short/long

**Solution**:

```bash
idf.py menuconfig
# Sleep Manager Configuration → Sleep timeout (seconds)
# Set to desired value (5-300 seconds)
```

---

## Log Messages Reference

| Log Message                                         | Feature           | Meaning                                      |
| --------------------------------------------------- | ----------------- | -------------------------------------------- |
| "Sleep manager disabled in configuration"           | Master switch OFF | Sleep functionality completely disabled      |
| "Sleep check task started"                          | Master switch ON  | Sleep monitoring active                      |
| "Sleep manager disabled - task exiting"             | Master switch OFF | Sleep task exits immediately                 |
| "Initializing sleep manager (timeout: X seconds)"   | Init              | Sleep manager starting with X second timeout |
| "GPIO wake-up enabled on GPIO15"                    | GPIO wakeup ON    | Touch will wake device                       |
| "GPIO wake-up disabled"                             | GPIO wakeup OFF   | No wake-up source configured                 |
| "Touch event callbacks registered"                  | Touch reset ON    | Touch will reset timer                       |
| "Activity timer reset"                              | Debug + Touch     | Timer was reset by touch event               |
| "Inactivity timeout - entering sleep mode"          | Sleep entry       | Device is entering sleep                     |
| "Display sleep (backlight off)"                     | Backlight ON      | Backlight turned off                         |
| "Display sleep (backlight control disabled)"        | Backlight OFF     | Backlight left on                            |
| "Paused X LVGL timers"                              | LVGL timers ON    | Watchface timers stopped                     |
| "LVGL timer pause disabled"                         | LVGL timers OFF   | Timers continue running                      |
| "LVGL rendering disabled"                           | LVGL rendering ON | Frame rendering stopped                      |
| "Entering light sleep (wake sources: touch GPIO15)" | Sleep             | CPU entering light sleep                     |
| "Woke from light sleep after X ms (cause: touch)"   | Wake              | Device woke due to touch                     |
| "Display wake (backlight on)"                       | Backlight ON      | Backlight restored                           |
| "LVGL rendering enabled"                            | LVGL rendering ON | Rendering resumed                            |
| "Resumed X LVGL timers"                             | LVGL timers ON    | Watchface timers restarted                   |
| "Wake complete"                                     | Wake              | Full wake sequence done                      |

---

## Power Consumption Estimates

| Configuration           | Estimated Current | Use Case                 |
| ----------------------- | ----------------- | ------------------------ |
| Sleep disabled          | 50-150mA          | Development, testing     |
| Basic sleep only        | 40-100mA          | Testing sleep entry/exit |
| Backlight off           | 10-30mA           | Partial power savings    |
| Backlight + LVGL timers | 5-15mA            | Good power savings       |
| Full sleep (all ON)     | 1-5mA             | Production mode          |

_Note: Actual values depend on other factors (WiFi, BLE, sensors, etc.)_

---

## Build Commands Quick Reference

```bash
# Clean build (if config changes don't apply)
idf.py fullclean
idf.py build

# Flash and monitor
idf.py flash monitor

# Monitor only (after flash)
idf.py monitor

# Exit monitor
Ctrl+]

# One-liner for config → test cycle
idf.py menuconfig && idf.py build flash monitor
```

---

## Next Steps After Testing

1. **Measure actual power consumption** with multimeter
2. **Adjust sleep timeout** based on use case (10s may be too short/long)
3. **Test different wake sources** (future: IMU gesture, button)
4. **Add sleep mode indicator UI** (show countdown before sleep)
5. **Implement deep sleep** for extended battery life (Phase 4)

---

**Default Configuration**: All features **OFF** for safe initial testing.

Enable features incrementally as you verify each works correctly!
