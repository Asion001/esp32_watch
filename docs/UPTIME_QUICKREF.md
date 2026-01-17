# Uptime Tracker - Quick Reference

## Visual Display

```
╔══════════════════════════════════════╗
║                        85% 3.95V    ║ ← Battery (green/yellow/red)
║                                      ║
║                                      ║
║             14:25                    ║ ← Time (white, 48pt)
║                                      ║
║        Fri, Jan 10                   ║ ← Date (gray, 20pt)
║                                      ║
║                                      ║
║                                      ║
║                                      ║
║                                      ║
║ Up: 2h 34m                           ║ ← Session uptime (gray, 14pt)
║ Total: 15d 7h (Boot #23)             ║ ← Total & boots (dark gray, 14pt)
╚══════════════════════════════════════╝
    2.06" AMOLED (410×502 pixels)
```

## Quick Start

### Initialize (in watchface_create)
```c
#include "uptime_tracker.h"
uptime_tracker_init();
```

### Update (every second in timer callback)
```c
uptime_tracker_update();

uptime_stats_t stats;
uptime_tracker_get_stats(&stats);

char uptime_str[32];
uptime_tracker_format_time(stats.current_uptime_sec, uptime_str, sizeof(uptime_str));
lv_label_set_text_fmt(uptime_label, "Up: %s", uptime_str);
```

### Save (every 60 seconds)
```c
if (save_counter >= 60) {
    save_counter = 0;
    uptime_tracker_save();
}
```

## Data Structure

```c
typedef struct {
    uint64_t total_uptime_sec;   // Total uptime (all boots)
    uint64_t current_uptime_sec; // This session only
    uint32_t boot_count;         // Number of boots
    uint32_t last_boot_time;     // Reserved for future
} uptime_stats_t;
```

## Format Examples

| Uptime (seconds) | Formatted Output |
|------------------|------------------|
| 1800            | "30m"            |
| 5400            | "1h 30m"         |
| 90000           | "1d 1h 0m"       |
| 259200          | "3d 0h 0m"       |

## NVS Storage

- **Namespace**: `uptime`
- **Keys**: 
  - `total_up` (uint64_t) - cumulative uptime
  - `boot_cnt` (uint32_t) - boot counter
- **Size**: 16 bytes total
- **Lifetime**: ~100,000 writes at 60-second intervals = ~69 days continuous use per NVS sector

## Use Cases

### Battery Testing
Track runtime to measure battery life:
```
Boot #1: Up: 8h 23m → Battery died at 8h 23m
Boot #2: Up: 7h 45m → Battery died at 7h 45m
Average: ~8 hours per charge
```

### Stability Testing
Monitor for unexpected resets:
```
Boot #1: Up: 15d 7h → Stable, planned reboot
Boot #45: Up: 23m → Many reboots = problem!
```

### Development
Verify features don't cause resets:
```
Before feature: Boot #12, Up: 3d
After feature: Boot #13, Up: 2d → Good
After bad change: Boot #50, Up: 5m → Crash loop!
```

## API Functions

| Function | Purpose | Frequency |
|----------|---------|-----------|
| `uptime_tracker_init()` | Initialize, load NVS | Once at boot |
| `uptime_tracker_update()` | Update counters | Every second |
| `uptime_tracker_save()` | Persist to NVS | Every 60 sec |
| `uptime_tracker_get_stats()` | Read current data | Every second |
| `uptime_tracker_format_time()` | Format to string | Every second |
| `uptime_tracker_reset()` | Clear all data | Manual only |

## Memory Usage

- **Flash**: ~3 KB (code)
- **RAM**: ~100 bytes (state)
- **NVS**: 16 bytes (storage)
- **Stack**: Minimal (no recursion)

## Integration Checklist

- [x] Add component to project
- [x] Update main CMakeLists.txt
- [x] Initialize in watchface_create()
- [x] Update in timer callback
- [x] Save periodically to NVS
- [x] Display formatted uptime
- [x] Test on hardware
- [ ] Verify NVS persistence
- [ ] Monitor long-term stability

## Troubleshooting

### Uptime resets to 0
- Check NVS initialization
- Verify save is called periodically
- Check for NVS partition corruption

### Boot count too high
- May indicate crash loop
- Check logs for panic/reset reasons
- Monitor system stability

### Display shows garbage
- Check format specifiers (%u not %lu)
- Verify buffer sizes adequate
- Check LVGL font availability

## Future Enhancements

- [ ] Battery drain correlation (uptime vs % drain)
- [ ] Longest uptime record
- [ ] Average uptime per boot
- [ ] Uptime history (last N boots)
- [ ] Export stats via serial/WiFi
