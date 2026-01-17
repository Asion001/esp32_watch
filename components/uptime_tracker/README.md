# Uptime Tracker Component

## Overview

Persistent uptime tracking component for ESP32-C6 Watch. Tracks device uptime across reboots using NVS (Non-Volatile Storage) for battery consumption testing and reliability monitoring.

## Features

- **Persistent Storage**: Uptime data survives reboots and power cycles
- **Boot Counter**: Tracks number of device boots for reliability analysis
- **Dual Uptime Tracking**:
  - Current session uptime (since last boot)
  - Total uptime across all boots
- **Automatic NVS Management**: Handles NVS initialization and storage
- **Human-Readable Formatting**: "Xd Xh Xm" format for easy reading

## Usage

### Initialization

Call once at startup (before creating watchface):

```c
#include "uptime_tracker.h"

esp_err_t ret = uptime_tracker_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize uptime tracker");
}
```

### Periodic Updates

Call from a timer callback (e.g., every second):

```c
void timer_callback(lv_timer_t *timer) {
    uptime_tracker_update();  // Lightweight, safe to call frequently
    
    // Get stats for display
    uptime_stats_t stats;
    if (uptime_tracker_get_stats(&stats) == ESP_OK) {
        // Use stats.current_uptime_sec, stats.total_uptime_sec, stats.boot_count
    }
}
```

### Saving to NVS

Save periodically (e.g., every 60 seconds) to persist data:

```c
static uint32_t save_counter = 0;

void timer_callback(void) {
    save_counter++;
    if (save_counter >= 60) {
        save_counter = 0;
        uptime_tracker_save();  // Writes to NVS
    }
}
```

### Display Formatting

Format uptime for human-readable display:

```c
uptime_stats_t stats;
uptime_tracker_get_stats(&stats);

char buffer[32];
uptime_tracker_format_time(stats.current_uptime_sec, buffer, sizeof(buffer));
// buffer now contains: "2d 5h 23m" or "5h 23m" or "23m"

printf("Uptime: %s\n", buffer);
printf("Total: %llu seconds\n", stats.total_uptime_sec);
printf("Boot count: %lu\n", stats.boot_count);
```

## API Reference

### Functions

- `uptime_tracker_init()` - Initialize component and load stored data
- `uptime_tracker_update()` - Update counters (call frequently)
- `uptime_tracker_save()` - Persist current data to NVS
- `uptime_tracker_get_stats()` - Get current statistics
- `uptime_tracker_format_time()` - Format seconds to "Xd Xh Xm" string
- `uptime_tracker_reset()` - Clear all stored data (use with caution)

### Data Structure

```c
typedef struct {
    uint64_t total_uptime_sec;   // Total uptime across all boots (seconds)
    uint64_t current_uptime_sec; // Current session uptime (seconds)
    uint32_t boot_count;         // Number of times device has booted
    uint32_t last_boot_time;     // Unix timestamp of last boot (reserved)
} uptime_stats_t;
```

## NVS Storage

- **Namespace**: `uptime`
- **Keys**:
  - `total_up` - Total uptime in seconds (uint64_t)
  - `boot_cnt` - Boot counter (uint32_t)
- **Write Frequency**: Recommended every 60 seconds
- **Storage Size**: ~16 bytes

## Battery Testing Use Case

This component is designed for battery consumption testing:

1. **Track runtime**: Monitor how long the watch runs on a charge
2. **Boot reliability**: Detect unexpected resets (high boot count)
3. **Long-term testing**: Compare uptime across multiple charge cycles
4. **Performance analysis**: Correlate features with battery drain

## Example Output

```
Up: 2h 34m                    (Current session)
Total: 15d 7h 12m (Boot #23)  (All time stats)
```

## Integration with Watchface

The watchface automatically displays uptime in the bottom-left corner:

- Line 1: Current session uptime
- Line 2: Total uptime and boot count

## Dependencies

- `nvs_flash` - ESP-IDF NVS storage
- `esp_timer` - High-resolution timer for microsecond precision

## Thread Safety

All functions are thread-safe when called from the LVGL thread. If calling from other threads, add appropriate locking.

## Future Enhancements

- [ ] Battery level correlation (track uptime vs battery drain)
- [ ] Last boot timestamp from RTC
- [ ] Export uptime logs via serial/WiFi
- [ ] Uptime history (last N boots)
