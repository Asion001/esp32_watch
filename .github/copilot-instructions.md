# ESP32-C6 Smartwatch Firmware - Copilot Instructions (Overview)

## Project Snapshot

Modular smartwatch firmware for Waveshare ESP32-C6 Touch AMOLED 2.06 board. Built with ESP-IDF v5.4.0, LVGL v9, and FreeRTOS. Current focus: digital watchface with RTC and battery monitoring.

**Target Hardware**: ESP32-C6 RISC-V @ 160MHz, 2.06" AMOLED (410×502), FT3168 touch, PCF85063 RTC, AXP2101 PMU

## Build & Tooling (Critical)

- **Before any ESP-IDF action**: run `export.bat` (Windows) or `export.sh` (Linux/macOS) from the ESP-IDF SDK to set the environment.
- **Preferred**: VS Code ESP-IDF extension for build/flash/monitor.
- **Allowed**: `idf.py` commands **only after** running export.{sh,bat}.
- **Avoid**: non-ESP-IDF build tools or ad-hoc shell build commands.
- **After changes**: perform a build to confirm compilation.

## Feature Rules (Must Follow)

- Every feature must be menuconfig-toggleable (Kconfig).
- Guard feature code with `#ifdef CONFIG_*`.
- Provide no-op stubs when disabled.
- Test both enabled and disabled builds.
- Settings screen title must start with "App: ".

## Hardware Docs & Fonts

- Re-read relevant datasheet PDFs in [docs/](docs/) when touching hardware-related code.
- Required LVGL fonts in sdkconfig:
  - CONFIG_LV_FONT_MONTSERRAT_48=y
  - CONFIG_LV_FONT_MONTSERRAT_20=y
  - CONFIG_LV_FONT_MONTSERRAT_14=y

## Detailed Rules (Split Instructions)

- Build & tooling details: [.github/instructions/build-and-tools.md](.github/instructions/build-and-tools.md)
- Feature toggle rules: [.github/instructions/feature-flags.md](.github/instructions/feature-flags.md)
- Firmware architecture & I2C patterns: [.github/instructions/firmware-architecture.md](.github/instructions/firmware-architecture.md)
- UI/LVGL rules for apps: [.github/instructions/ui-patterns.md](.github/instructions/ui-patterns.md)
- Embedded C/FreeRTOS best practices: [.github/instructions/embedded-best-practices.md](.github/instructions/embedded-best-practices.md)

## Reference Docs

- Feature guide: [.github/copilot-feature-development.md](.github/copilot-feature-development.md)
- Build guide: [docs/BUILD.md](docs/BUILD.md)
- Quick start: [docs/QUICKSTART.md](docs/QUICKSTART.md)
- Watchface/UI notes: [docs/WATCHFACE_LAYOUT.md](docs/WATCHFACE_LAYOUT.md)
- Settings design: [docs/SETTINGS_IMPLEMENTATION.md](docs/SETTINGS_IMPLEMENTATION.md)
- Hardware specs: [README.md](README.md#-hardware-specifications)
