# Watchface Layout - Updated with Uptime Display

## Visual Layout

```
┌──────────────────────────────────────┐
│                        100% 4.20V    │ ← Battery (top-right)
│                                      │
│                                      │
│             14:25                    │ ← Time (center, Montserrat 48)
│                                      │
│        Fri, Jan 10                   │ ← Date (below time, Montserrat 20)
│                                      │
│                                      │
│                                      │
│                                      │
│                                      │
│ Up: 2h 34m                           │ ← Current session uptime (bottom-left)
│ Total: 15d 7h (Boot #23)             │ ← Total uptime & boot count
└──────────────────────────────────────┘
```

## Display Elements

### 1. Time Display (Center)
- **Font**: Montserrat 48 (large, bold)
- **Color**: White
- **Format**: HH:MM (24-hour format)
- **Position**: Center of screen, -30px Y offset

### 2. Date Display (Below Time)
- **Font**: Montserrat 20
- **Color**: Gray (#888888)
- **Format**: Day, Month DD (e.g., "Fri, Jan 10")
- **Position**: Center of screen, +30px Y offset

### 3. Battery Indicator (Top-Right)
- **Font**: Montserrat 14
- **Color**: 
  - Green (#00FF00) when > 30%
  - Yellow (#FFFF00) when 15-30%
  - Red (#FF0000) when < 15%
- **Format**: "X% X.XXV" or "⚡ X% X.XXV" (when charging)
- **Position**: Top-right corner, -80px X, 10px Y

### 4. Current Session Uptime (Bottom-Left) **NEW**
- **Font**: Montserrat 14
- **Color**: Gray (#888888)
- **Format**: "Up: Xd Xh Xm" (omits zero values, e.g., "2h 34m")
- **Position**: Bottom-left, 10px X, -30px Y
- **Updates**: Every second

### 5. Total Uptime & Boot Count (Bottom-Left) **NEW**
- **Font**: Montserrat 14
- **Color**: Darker Gray (#666666)
- **Format**: "Total: Xd Xh Xm (Boot #N)"
- **Position**: Bottom-left, 10px X, -10px Y
- **Updates**: Every second
- **Persistence**: Stored in NVS, survives reboots

## Uptime Display Examples

### Short Uptime
```
Up: 34m
Total: 2h 15m (Boot #5)
```

### Medium Uptime
```
Up: 5h 23m
Total: 3d 12h (Boot #12)
```

### Long Uptime
```
Up: 2d 5h 23m
Total: 45d 18h (Boot #3)
```

## Uptime Tracker Features

### Data Persistence
- **Storage**: NVS (Non-Volatile Storage)
- **Namespace**: `uptime`
- **Save Frequency**: Every 60 seconds
- **Survives**: Reboots, power cycles, firmware updates (if NVS partition preserved)

### Boot Counter
- Increments on every device boot
- Useful for:
  - Detecting unexpected resets
  - Reliability testing
  - Monitoring system stability
  - Debugging reset issues

### Use Cases
1. **Battery Testing**: Track runtime between charges
2. **Stability Monitoring**: Low boot count = stable system
3. **Development**: Verify deep sleep doesn't cause data loss
4. **User Feedback**: Show device reliability metrics

## Technical Implementation

### Uptime Calculation
- **Current Session**: `esp_timer_get_time()` since boot
- **Total Uptime**: Stored value + current session
- **Precision**: Microsecond resolution, displayed as minutes

### NVS Storage Keys
- `total_up` (uint64_t): Total uptime in seconds
- `boot_cnt` (uint32_t): Number of boots

### Memory Impact
- **RAM**: ~100 bytes for component state
- **NVS**: 16 bytes persistent storage
- **Code**: ~3KB flash

## Display Refresh Strategy

All elements update every second via LVGL timer:
1. RTC time and date
2. Battery voltage and percentage
3. Current session uptime
4. Total uptime and boot count

Every 60 seconds, uptime data is persisted to NVS.

## Future Enhancements

Potential additions for Phase 3+:
- Uptime history graph
- Average uptime per boot
- Battery drain rate (% per hour)
- Time since last charge
- Longest continuous uptime record
