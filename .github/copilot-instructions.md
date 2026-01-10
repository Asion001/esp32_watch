# ESP32-C6 Smartwatch Firmware - AI Coding Instructions

## Project Overview

Modular smartwatch firmware for Waveshare ESP32-C6 Touch AMOLED 2.06 board. Built with ESP-IDF v5.4.0, LVGL v9, and FreeRTOS. Current focus: digital watchface with RTC and battery monitoring.

**Target Hardware**: ESP32-C6 RISC-V @ 160MHz, 2.06" AMOLED (410×502), FT3168 touch, PCF85063 RTC, AXP2101 PMU

**Prerequisites**: ESP-IDF v5.4.0 minimum (project uses latest I2C master driver API)

## Feature Development Guidelines

**ALL new features MUST follow these rules:**

1. **Configurable via menuconfig**: Every feature needs a `Kconfig` file with enable/disable toggle
2. **Conditional compilation**: Use `#ifdef CONFIG_*` guards around feature code
3. **Stub implementations**: Provide no-op functions when feature is disabled
4. **Always build-test**: Run `idf.py build` after ANY change to verify compilation
5. **Test both states**: Build with feature enabled AND disabled

**📖 Full Guidelines**: See [.github/copilot-feature-development.md](.github/copilot-feature-development.md) for complete feature development process.

**🔧 Quick Commands**: See `Makefile` for common tasks:

- `make build` - Standard build
- `make check` - Test all configurations
- `make test-all` - Build with all features
- `make test-minimal` - Build with minimal features

## Build System & Commands

**CRITICAL**: This is an ESP-IDF project. Always use ESP-IDF tools, never raw shell commands for builds.

### Essential Commands

- **Build**: Use VS Code ESP-IDF extension "Build your project" OR `idf.py build`
- **Flash**: Use extension "Flash your project" OR `idf.py -p /dev/ttyUSB0 flash`
- **Monitor**: Use extension "Monitor your device" OR `idf.py monitor`
- **Clean**: `idf.py fullclean` (when SDK config changes)
- **Config**: `idf.py menuconfig` (for LVGL fonts, board settings)

### Required LVGL Fonts (in sdkconfig)

```
CONFIG_LV_FONT_MONTSERRAT_48=y  # Big time display
CONFIG_LV_FONT_MONTSERRAT_20=y  # Date display
CONFIG_LV_FONT_MONTSERRAT_14=y  # Battery indicator
```

## Architecture

### Component Dependencies

```
main (app entry)
├── managed_components/
│   ├── waveshare__esp32_c6_touch_amoled_2_06 (BSP - handles display/touch/I2C init)
│   └── lvgl__lvgl (v9.2.0 graphics)
└── components/
    ├── pcf85063_rtc (RTC driver)
    └── axp2101_pmu (battery/power driver)
```

**Key Pattern**: BSP provides `bsp_display_start()` and `bsp_i2c_get_handle()`. Always use BSP's I2C handle for sensors.

### Modular App System

Location: `main/apps/<app_name>/`

- CMake auto-discovers all `.c` files in `apps/` subdirs
- Each app is self-contained (UI + drivers + state)
- Current app: `watchface/` (digital clock)

**Adding New Apps**:

1. Create `main/apps/my_app/` directory
2. Implement `my_app_create(lv_obj_t *parent)` function
3. Include in `main/main.c` and call in `app_main()`
4. No CMake changes needed (auto-discovered)

### LVGL Threading Model

```c
bsp_display_lock(0);      // MUST lock before any LVGL API calls
lv_label_set_text(...);   // Safe to call LVGL functions
bsp_display_unlock();     // Unlock when done
```

**Critical**: Never call LVGL APIs without holding display lock. Timers created via `lv_timer_create()` run in LVGL thread and don't need locking.

## I2C Sensor Access Pattern

All peripherals share I2C bus (SDA: GPIO8, SCL: GPIO7). BSP handles bus initialization.

```c
#include "bsp/esp-bsp.h"

// In your init function:
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
rtc_init(i2c);    // Pass handle to component drivers
axp2101_init(i2c);
```

**I2C Addresses**:

- RTC (PCF85063): `0x51`
- PMU (AXP2101): `0x34`
- IMU (QMI8658): `0x6A/0x6B` (not yet implemented)

**Future IMU Driver**: When implementing QMI8658, follow same pattern as RTC/PMU - create `components/qmi8658_imu/` with flat structure, accept I2C handle in init, provide gesture detection APIs for wrist-raise wake-up (Phase 4).

## Component Drivers

### RTC Driver (`components/pcf85063_rtc/`)

```c
esp_err_t rtc_init(i2c_master_bus_handle_t i2c_bus);
esp_err_t rtc_read_time(struct tm *time);           // Standard C time struct
esp_err_t rtc_write_time(const struct tm *time);
bool rtc_is_valid(void);  // Checks year in range 2024-2099
```

**Auto-init**: First boot sets Jan 10, 2026 12:00:00. Modify default in `rtc_pcf85063.c` if needed.

### PMU Driver (`components/axp2101_pmu/`)

```c
esp_err_t axp2101_init(i2c_master_bus_handle_t i2c_bus);
esp_err_t axp2101_get_battery_voltage(uint16_t *voltage_mv);
esp_err_t axp2101_get_battery_percent(uint8_t *percent);  // 0-100%
esp_err_t axp2101_is_charging(bool *is_charging);
esp_err_t axp2101_is_vbus_present(bool *vbus_present);
```

**Voltage curve**: 4.2V=100%, 3.7V=50%, 3.3V=0% (linear interpolation)

## UI Patterns

### Watchface Update Pattern

```c
static lv_timer_t *update_timer;

static void watchface_timer_cb(lv_timer_t *timer) {
    // Read sensors
    rtc_read_time(&time);
    axp2101_get_battery_percent(&percent);

    // Update UI (already in LVGL thread, no locking needed)
    lv_label_set_text_fmt(label, "%02d:%02d", time.tm_hour, time.tm_min);
}

lv_obj_t *watchface_create(lv_obj_t *parent) {
    // Create UI elements
    time_label = lv_label_create(parent);

    // Create 1-second update timer (runs in LVGL thread)
    update_timer = lv_timer_create(watchface_timer_cb, 1000, NULL);
}
```

### Color-Coded Battery Display

```c
if (percent > 30) {
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);  // Green
} else if (percent > 15) {
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFF00), 0);  // Yellow
} else {
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), 0);  // Red
}

// Charging indicator
lv_label_set_text_fmt(label, LV_SYMBOL_CHARGE " %d%%", percent);
```

## File Locations & Conventions

### Component Structure

- Custom drivers: `components/<name>/<name>.c/h` (flat, no `include/` subdir)
- CMake: `idf_component_register(SRCS "file.c" INCLUDE_DIRS "." REQUIRES driver)`
- Apps: `main/apps/<app>/` with descriptive filenames (e.g., `watchface.c`, not `app.c`)

### Naming Conventions

- Component API prefix: `<component>_` (e.g., `rtc_init`, `axp2101_get_battery_percent`)
- Static TAG: `static const char *TAG = "ComponentName";` for ESP_LOG
- Header guards: `<FILENAME>_H` uppercase

## Development Roadmap Context

**Current Phase (Phase 1)**: Basic watchface with time/date/battery ✅

**Next Priorities**:

- Phase 2: Settings menu, WiFi/NTP sync, NVS storage
- Phase 3: Additional apps (stopwatch, timer, alarms)
- Phase 4: Deep sleep, IMU wake-up, power optimization
- Phase 5: Multiple watchfaces, BT notifications, fitness

## Embedded C/C++ Best Practices for MCU

### Memory Management

- **Stack allocation first**: Prefer stack variables for small, fixed-size data
- **Heap use carefully**: Use `malloc`/`free` sparingly, prefer static allocation for predictable memory usage
- **Check allocations**: Always check return values from heap allocations
- **IRAM placement**: Use `IRAM_ATTR` for ISR/high-frequency functions (already configured via `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM`)

### Performance & Power

- **Avoid busy loops**: Use FreeRTOS delays (`vTaskDelay`) instead of `while(1)` loops
- **Minimize I2C transactions**: Batch sensor reads when possible, cache infrequently-changing data
- **Use const**: Mark read-only data with `const` to keep it in flash, save RAM
- **Volatile for hardware**: Use `volatile` for hardware register access and shared ISR variables

### Error Handling

- **Check ESP_ERROR_CHECK**: Use for critical operations, log errors otherwise
- **Graceful degradation**: If sensor init fails, continue with reduced functionality
- **Example pattern**:
  ```c
  esp_err_t ret = rtc_init(i2c);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "RTC init failed: %s", esp_err_to_name(ret));
      // Continue without RTC, display "--:--" instead
  }
  ```

### Code Style

- **NULL checks**: Always validate pointers before dereferencing
- **Fixed-width types**: Use `uint8_t`, `int32_t`, not `int`, `long`
- **Packed structs**: Use `__attribute__((packed))` for hardware register maps
- **Initialization**: Zero-initialize structs: `struct tm time = {0};`

### FreeRTOS Patterns

- **Task priorities**: UI tasks higher priority than background tasks
- **Mutex protection**: Use mutexes for shared state (BSP provides display mutex)
- **Queue depth**: Keep queues small (8-16 items max) to save RAM
- **Stack size**: Start with 2048-4096 bytes, tune via `uxTaskGetStackHighWaterMark()`

## Common Pitfalls

1. **Don't** call `idf.py` commands from terminal commands - use ESP-IDF extension
2. **Don't** forget to lock display before LVGL calls in non-timer code
3. **Don't** use `components/<name>/include/` - components use flat structure
4. **Don't** hardcode I2C bus init - always get from `bsp_i2c_get_handle()`
5. **Don't** forget to enable required Montserrat fonts in menuconfig
6. **Don't** use floating-point math unnecessarily (slower on RISC-V without FPU)
7. **Don't** block in timer callbacks - keep them fast (<10ms)

## Key Files Reference

- Entry point: [main/main.c](main/main.c)
- Watchface app: [main/apps/watchface/watchface.c](main/apps/watchface/watchface.c)
- RTC driver: [components/pcf85063_rtc/rtc_pcf85063.c](components/pcf85063_rtc/rtc_pcf85063.c)
- PMU driver: [components/axp2101_pmu/pmu_axp2101.c](components/axp2101_pmu/pmu_axp2101.c)
- Dependencies: [main/idf_component.yml](main/idf_component.yml)
- SDK config: [sdkconfig.defaults](sdkconfig.defaults)

## Documentation

- Hardware specs/pins: [README.md](README.md#-hardware-specifications)
- Build guide: [BUILD.md](BUILD.md)
- Quick reference: [QUICKSTART.md](QUICKSTART.md)
- Datasheets: [docs/](docs/) (PDFs for all chips)
