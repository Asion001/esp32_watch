# Build Instructions

## Prerequisites

You need ESP-IDF v5.1 or later installed. If you don't have it:

```bash
# Option 1: Using ESP-IDF extension in VS Code
# Install the "Espressif IDF" extension and follow setup wizard

# Option 2: Manual installation
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
. ./export.sh
```

## Building the Firmware

1. **Set up ESP-IDF environment** (if not using VS Code extension):

   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

2. **Navigate to project**:

   ```bash
   cd /home/asion/dev/my/watch/esp_watch
   ```

3. **Build**:

   ```bash
   idf.py build
   ```

4. **Flash to device**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   (Replace `/dev/ttyUSB0` with your actual serial port)

## Configuration Changes

The following configuration has been applied:

- ✅ `CONFIG_LV_FONT_MONTSERRAT_48=y` - Large font for time display
- ✅ `CONFIG_LV_FONT_MONTSERRAT_20=y` - Date display font
- ✅ `CONFIG_LV_FONT_MONTSERRAT_14=y` - Battery indicator font

## Troubleshooting

### "idf.py command not found"

Solution: Source the ESP-IDF environment:

```bash
. $HOME/esp/esp-idf/export.sh
```

### Build fails with font errors

Solution: Clean build and rebuild:

```bash
idf.py fullclean
idf.py build
```

### Serial port permission denied

Solution: Add user to dialout group:

```bash
sudo usermod -a -G dialout $USER
# Log out and log back in
```

## Using ESP-IDF VS Code Extension

If you have the ESP-IDF extension installed in VS Code:

1. Open Command Palette (Ctrl+Shift+P)
2. Run: "ESP-IDF: Build your project"
3. Run: "ESP-IDF: Flash your project"
4. Run: "ESP-IDF: Monitor your device"

## Next Steps After Flashing

1. Watch should display current time (default: 12:00)
2. Battery percentage appears in top-right corner
3. Date displays below time
4. To set correct time, see Phase 2 roadmap (WiFi/NTP sync)
