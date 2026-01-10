# ESP32-C6 Touch AMOLED 2.06 Documentation

This directory contains essential datasheets and technical documentation for developing firmware for the ESP32-C6 Touch AMOLED 2.06 smartwatch.

## Hardware Schematics

- **[ESP32-C6-Touch-AMOLED-2.06-Schematic.pdf](ESP32-C6-Touch-AMOLED-2.06-Schematic.pdf)** - Complete board schematic
  - Source: https://files.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.06/ESP32-C6-Touch-AMOLED-2.06-Schematic-V1.0.pdf

## ESP32-C6 MCU Documentation

- **[ESP32-C6-Series-Datasheet.pdf](ESP32-C6-Series-Datasheet.pdf)** - ESP32-C6 datasheet with electrical specifications
  - Source: https://files.waveshare.com/wiki/common/ESP32-C6_Series_Datasheet.pdf

- **[ESP32-C6-Technical-Reference-Manual.pdf](ESP32-C6-Technical-Reference-Manual.pdf)** - Complete technical reference manual
  - Source: https://files.waveshare.com/wiki/common/ESP32-C6_Technical_Reference_Manual.pdf

## Peripheral Component Datasheets

### Power Management
- **[AXP2101-PMU-Datasheet.pdf](AXP2101-PMU-Datasheet.pdf)** - AXP2101 Power Management Unit
  - I2C Address: 0x34
  - Battery voltage monitoring, charging control, power rails
  - Source: https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf

### Real-Time Clock
- **[PCF85063-RTC-Datasheet.pdf](PCF85063-RTC-Datasheet.pdf)** - PCF85063A Real-Time Clock
  - I2C Address: 0x51
  - Time/date registers, alarms, oscillator
  - Source: https://files.waveshare.com/wiki/common/PCF85063A.pdf

### Motion Sensor
- **[QMI8658-IMU-Datasheet.pdf](QMI8658-IMU-Datasheet.pdf)** - QMI8658 6-Axis IMU
  - I2C Address: 0x6A or 0x6B
  - Accelerometer + Gyroscope
  - Source: https://files.waveshare.com/wiki/common/QMI8658C.pdf

### Display
- **[CO5300-Display-Datasheet.pdf](CO5300-Display-Datasheet.pdf)** - CO5300 Display Controller
  - 410×502 AMOLED display
  - QSPI interface, brightness control
  - Source: https://files.waveshare.com/wiki/common/CO5300_Datasheet_V0.00.pdf

## Additional Resources

For more information, code examples, and latest updates, visit:
- **Waveshare Wiki**: https://www.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.06
- **GitHub Demo Repository**: https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.06

## Pin Connections Summary

From the schematic and BSP configuration:

| Peripheral | Pins | Notes |
|------------|------|-------|
| I2C Bus | SDA: GPIO8, SCL: GPIO7 | Shared by RTC, IMU, PMU |
| Display SPI | CS: GPIO5, CLK: GPIO0, DATA0-3: GPIO1-4 | QSPI interface |
| Touch | RST: GPIO10, INT: GPIO15 | FT3168 controller |
| LCD Reset | GPIO11 | Display reset line |
| Audio I2S | SCLK: GPIO20, MCLK: GPIO19, LRCLK: GPIO22, DOUT: GPIO23, DIN: GPIO21 | ES8311 codec |
| Power Amp | GPIO6 | Audio amplifier enable |
| Boot Button | GPIO0 | Shared with display PCLK |

## License

All datasheets are copyright of their respective manufacturers. Downloaded from Waveshare wiki for development reference only.
