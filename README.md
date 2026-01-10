# ESP32-C6 Watch Firmware

Open-source, modular smartwatch firmware for the Waveshare ESP32-C6 Touch AMOLED 2.06 development board.

![Hardware](https://www.waveshare.com/img/devkit/ESP32-C6-Touch-AMOLED-2.06/ESP32-C6-Touch-AMOLED-2.06-1.jpg)

## 🎯 Features

- ⌚ **Large Digital Clock** - Big, readable time display with date
- 🔋 **Battery Indicator** - Real-time battery percentage and charging status
- 🕐 **RTC Integration** - Accurate timekeeping with PCF85063 RTC chip
- 🎨 **LVGL Graphics** - Smooth, modern UI with LVGL v9
- 🔌 **Modular Architecture** - Easy to add new apps and features

## 🛠️ Hardware Specifications

### Core
- **MCU**: ESP32-C6 (RISC-V, 160MHz)
- **Display**: 2.06" AMOLED Touch Screen (410×502 pixels, CO5300 controller)
- **Touch**: FT3168 capacitive touch controller
- **WiFi**: WiFi 6 (802.11ax)
- **Bluetooth**: BLE 5.0

### Peripherals
- **RTC**: PCF85063 (I2C: 0x51)
- **PMU**: AXP2101 Power Management (I2C: 0x34)
- **IMU**: QMI8658 6-axis accelerometer + gyroscope (I2C: 0x6A/0x6B)
- **Audio**: ES8311 codec, ES7210 ADC, speaker, microphone
- **Battery**: 400mAh LiPo (recommended)

### Pin Configuration
| Function | Pins | Notes |
|----------|------|-------|
| I2C | SDA: GPIO8, SCL: GPIO7 | Shared by RTC, IMU, PMU |
| Display QSPI | CS: GPIO5, CLK: GPIO0, DATA: GPIO1-4 | CO5300 controller |
| Touch | RST: GPIO10, INT: GPIO15 | FT3168 |
| LCD Reset | GPIO11 | - |

## 🚀 Quick Start

### Prerequisites

- ESP-IDF v5.1 or later
- Python 3.8+
- Git

### Building and Flashing

1. **Clone the repository**
   ```bash
   git clone https://github.com/Asion001/esp32_watch.git
   cd esp32_watch
   ```

2. **Configure LVGL fonts** (enable large fonts for big clock)
   ```bash
   idf.py menuconfig
   ```
   Navigate to:
   ```
   Component config → LVGL configuration → Font Usage → Enable built-in fonts
   ```
   Enable:
   - `CONFIG_LV_FONT_MONTSERRAT_48` (for big time display)
   - `CONFIG_LV_FONT_MONTSERRAT_20` (for date)
   - `CONFIG_LV_FONT_MONTSERRAT_14` (for battery)

3. **Build the firmware**
   ```bash
   idf.py build
   ```

4. **Flash to device**
   ```bash
   idf.py flash monitor
   ```

### Initial Setup

On first boot, the RTC will be initialized with a default time (January 10, 2026 12:00:00). To set the correct time, you can:

1. **Via serial console** (future feature)
2. **Via WiFi NTP sync** (future feature - Phase 2)
3. **Modify default time** in `main/apps/watchface/rtc_pcf85063.c`

## 📁 Project Structure

```
esp_watch/
├── main/
│   ├── main.c                      # Application entry point
│   ├── CMakeLists.txt              # Build configuration
│   ├── idf_component.yml           # Component dependencies
│   └── apps/                       # Modular applications
│       └── watchface/              # Digital clock app
│           ├── watchface.c/h       # Main watchface UI
│           ├── rtc_pcf85063.c/h    # RTC driver
│           └── pmu_axp2101.c/h     # Battery/power driver
├── docs/                           # Hardware datasheets
│   └── README.md                   # Documentation index
├── managed_components/             # ESP-IDF managed dependencies
│   ├── lvgl__lvgl/                 # LVGL graphics library
│   └── waveshare__esp32_c6.../     # Board support package
├── CMakeLists.txt                  # Top-level build config
└── README.md                       # This file
```

## 🔧 Architecture

### Modular App System

The firmware uses a modular architecture where each "app" is a self-contained module in `main/apps/`:

```
main/apps/
├── watchface/          # Clock app (current)
├── settings/           # Settings menu (future)
├── stopwatch/          # Stopwatch app (future)
└── ...                 # More apps (future)
```

Each app can have:
- UI components (LVGL screens)
- Sensor/peripheral drivers
- State management
- Event handlers

### Adding New Apps

1. **Create app directory**
   ```bash
   mkdir -p main/apps/my_new_app
   ```

2. **Create app files**
   ```c
   // main/apps/my_new_app/my_new_app.h
   #ifndef MY_NEW_APP_H
   #define MY_NEW_APP_H
   
   #include "lvgl.h"
   
   lv_obj_t* my_new_app_create(lv_obj_t *parent);
   void my_new_app_update(void);
   
   #endif
   ```

3. **Implement app logic**
   ```c
   // main/apps/my_new_app/my_new_app.c
   #include "my_new_app.h"
   
   lv_obj_t* my_new_app_create(lv_obj_t *parent) {
       lv_obj_t *screen = lv_obj_create(parent);
       // Build your UI here
       return screen;
   }
   ```

4. **Include in main.c**
   ```c
   #include "apps/my_new_app/my_new_app.h"
   
   void app_main(void) {
       bsp_display_start();
       bsp_display_lock(0);
       my_new_app_create(lv_screen_active());
       bsp_display_unlock();
   }
   ```

The CMake build system automatically includes all `.c` files in `main/apps/` directories.

### Sensor Access Pattern

All sensors are accessed via I2C using the BSP handle:

```c
#include "bsp/esp-bsp.h"

void my_sensor_init(void) {
    i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
    // Use i2c handle to communicate with sensors
}
```

Current sensor drivers:
- **RTC** (`rtc_pcf85063.c/h`) - Read/write time, validity checking
- **PMU** (`pmu_axp2101.c/h`) - Battery voltage, percentage, charging status

## 📚 API Documentation

### Watchface App

```c
lv_obj_t* watchface_create(lv_obj_t *parent);
void watchface_update(void);
```

### RTC Driver

```c
esp_err_t rtc_init(i2c_master_bus_handle_t i2c_bus);
esp_err_t rtc_read_time(struct tm *time);
esp_err_t rtc_write_time(const struct tm *time);
bool rtc_is_valid(void);
```

### PMU Driver

```c
esp_err_t pmu_init(i2c_master_bus_handle_t i2c_bus);
esp_err_t pmu_get_battery_voltage(uint16_t *voltage_mv);
esp_err_t pmu_get_battery_percent(uint8_t *percent);
esp_err_t pmu_is_charging(bool *is_charging);
esp_err_t pmu_is_vbus_present(bool *vbus_present);
```

## 🗺️ Roadmap

### Phase 2: Settings & WiFi Sync (Q1 2026)
- ⚙️ Settings menu (brightness, sleep timeout)
- 📡 WiFi configuration UI
- 🌐 NTP time synchronization
- 🕐 Time zone configuration
- 💾 Persistent settings storage (NVS)

### Phase 3: Additional Apps (Q2 2026)
- ⏱️ Stopwatch application
- ⏲️ Timer/countdown application
- 🔔 Alarm clock with multiple alarms
- 📊 Sensor dashboard (IMU data, temperature)
- 🎵 Music player controls (adapted from LVGL demo)

### Phase 4: Power Management (Q2 2026)
- 😴 Deep sleep on inactivity
- 🤚 Gesture wake-up (wrist raise detection via IMU)
- 🔋 Battery optimization
- 🌙 Low-power watchface mode
- ⚡ Configurable sleep timeout

### Phase 5: Advanced Features (Q3 2026)
- 🎨 Multiple watchface styles (analog, digital variants)
- 🌤️ Weather display (via WiFi API)
- 📲 Bluetooth notifications (phone pairing)
- 🏃 Fitness tracking (steps, activity)
- 🗓️ Calendar/events integration

## 🔗 Resources

### Hardware Documentation
- [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.06)
- [Datasheets](./docs/README.md) - Local copy of all component datasheets
- [GitHub Demos](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.06)

### Software Documentation
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- New watchface designs
- Additional apps
- Bug fixes
- Documentation improvements
- Hardware optimizations

### Development Guidelines
1. Follow existing code style
2. Add comments for complex logic
3. Test on hardware before submitting
4. Update README for new features

## 📝 License

This project is open source. See individual component licenses:
- ESP-IDF: Apache 2.0
- LVGL: MIT
- Hardware datasheets: Copyright respective manufacturers

## 👤 Author

**Asion001**
- GitHub: [@Asion001](https://github.com/Asion001)

## 🙏 Acknowledgments

- Waveshare for the excellent hardware platform
- Espressif for ESP-IDF and ESP32-C6
- LVGL team for the graphics library
- Open source community

---

**Note**: This is an active development project. Features and APIs may change. Always check the latest commits for updates.

## 📸 Screenshots

_Coming soon - screenshots of the watchface in action!_

---

Built with ❤️ for the maker community
