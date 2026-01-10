# Quick Start Guide

## ✅ What's Been Implemented

Your ESP32-C6 watch firmware is ready with the following features:

### 🎯 Current Features
- ⌚ **Big Digital Clock** - Large, readable HH:MM display using Montserrat 48 font
- 📅 **Date Display** - Shows day of week and date below time
- 🔋 **Battery Indicator** - Real-time battery percentage in top-right corner
  - Green text when >30%
  - Yellow text when 15-30%
  - Red text when <15%
  - Lightning icon (⚡) when charging
- 🕐 **RTC Integration** - Accurate timekeeping with PCF85063 chip
- ✅ **Auto-initialization** - Sets default time on first boot

### 📁 Project Structure
```
esp_watch/
├── README.md              # Complete documentation
├── BUILD.md               # Build instructions
├── docs/                  # Hardware datasheets (PDFs)
│   ├── README.md
│   ├── AXP2101-PMU-Datasheet.pdf
│   ├── PCF85063-RTC-Datasheet.pdf
│   ├── ESP32-C6-Touch-AMOLED-2.06-Schematic.pdf
│   └── ...
└── main/
    ├── main.c             # Entry point
    └── apps/
        └── watchface/     # Clock application
            ├── watchface.c/h       # Main UI
            ├── rtc_pcf85063.c/h    # RTC driver
            └── pmu_axp2101.c/h     # Battery driver
```

## 🚀 How to Build and Flash

### Option 1: Using ESP-IDF VS Code Extension (Recommended)

1. **Install ESP-IDF Extension** in VS Code
2. **Open Command Palette** (Ctrl+Shift+P)
3. **Build**: `ESP-IDF: Build your project`
4. **Flash**: `ESP-IDF: Flash your project`
5. **Monitor**: `ESP-IDF: Monitor your device`

### Option 2: Command Line

```bash
# 1. Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# 2. Build firmware
cd /home/asion/dev/my/watch/esp_watch
idf.py build

# 3. Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

## 📝 Configuration

The firmware is pre-configured with:
- ✅ Montserrat 48 font (big time display)
- ✅ Montserrat 20 font (date display)
- ✅ Montserrat 14 font (battery indicator)
- ✅ LVGL demos enabled (for reference)

No additional configuration needed!

## 🎨 What You'll See

When the watch boots:

```
╔══════════════════════╗
║          100% ⚡      ║  <- Battery indicator (top-right)
║                      ║
║                      ║
║       12:00          ║  <- Big time display (center)
║                      ║
║   Fri, Jan 10        ║  <- Date display (below time)
║                      ║
╚══════════════════════╝
```

## 🔧 Setting the Correct Time

The RTC initializes with: **January 10, 2026 at 12:00:00**

To change the time:

### Method 1: Modify Default Time (Current)
Edit `main/apps/watchface/rtc_pcf85063.c`:

```c
struct tm default_time = {
    .tm_year = 126,  // 2026 - 1900
    .tm_mon = 0,     // January (0-11)
    .tm_mday = 10,   // Day 10
    .tm_hour = 12,   // Hour (24h format)
    .tm_min = 0,     // Minutes
    .tm_sec = 0,     // Seconds
};
```

Then rebuild and reflash.

### Method 2: WiFi NTP Sync (Coming in Phase 2)
- Settings app to configure WiFi
- Automatic time sync via NTP
- Time zone configuration

## 🗺️ Roadmap

### Phase 2 - Settings & WiFi (Next)
- WiFi connection manager
- NTP time synchronization
- Brightness control
- Time zone settings

### Phase 3 - More Apps
- Stopwatch
- Timer/countdown
- Alarm clock
- Sensor dashboard

### Phase 4 - Power Management
- Deep sleep mode
- Gesture wake-up
- Battery optimization

### Phase 5 - Advanced Features
- Multiple watchface styles
- Weather display
- Notifications
- Fitness tracking

## 🧩 Adding Your Own App

1. Create new app folder:
```bash
mkdir -p main/apps/my_app
```

2. Create app files:
```c
// main/apps/my_app/my_app.h
#ifndef MY_APP_H
#define MY_APP_H
#include "lvgl.h"
lv_obj_t* my_app_create(lv_obj_t *parent);
#endif

// main/apps/my_app/my_app.c
#include "my_app.h"
lv_obj_t* my_app_create(lv_obj_t *parent) {
    lv_obj_t *screen = lv_obj_create(parent);
    // Your UI code here
    return screen;
}
```

3. Include in main.c:
```c
#include "apps/my_app/my_app.h"
// Use in app_main()
```

4. Build automatically includes it!

## 📚 Documentation

- **[README.md](README.md)** - Complete project documentation
- **[BUILD.md](BUILD.md)** - Detailed build instructions
- **[docs/README.md](docs/README.md)** - Hardware datasheets index

## 🔗 Resources

- **GitHub**: https://github.com/Asion001/esp32_watch
- **Waveshare Wiki**: https://www.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.06
- **ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf/
- **LVGL Docs**: https://docs.lvgl.io/

## ❓ Troubleshooting

### Build fails
```bash
idf.py fullclean
idf.py build
```

### Serial port not found
Check: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`

### Permission denied
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Font not found error
Already configured! Font is enabled in sdkconfig.

## 🎉 You're Ready!

Everything is set up and committed to GitHub. Just build, flash, and enjoy your open-source smartwatch!

---

**Questions?** Open an issue on GitHub: https://github.com/Asion001/esp32_watch/issues
