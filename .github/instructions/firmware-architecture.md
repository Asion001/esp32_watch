---
applyTo: "**/*.{c,h,cpp}"
---

# Firmware Architecture & I2C Patterns

## Component Dependencies

- BSP handles display, touch, and I2C init.
- Use bsp_display_start() and bsp_i2c_get_handle() from the BSP.

## Modular App System

- Apps live under main/apps/<app_name>/.
- Implement <app_name>\_create(lv_obj_t \*parent) and call it from app_main().
- Update main/CMakeLists.txt with new app include paths and REQUIRES dependencies.

## LVGL Threading Model

- Lock display before calling LVGL APIs from non-LVGL threads.
- Timers created with lv_timer_create() run in the LVGL thread and do not need locking.

## I2C Access Pattern

- All peripherals share the BSP I2C bus.
- Always obtain the handle via bsp_i2c_get_handle() and pass it to drivers.

### I2C Addresses

- RTC (PCF85063): 0x51
- PMU (AXP2101): 0x34
- IMU (QMI8658): 0x6A/0x6B

## Component Structure

- Custom drivers should be flat: components/<name>/<name>.c and .h (no include/ subdir).
- Use idf_component_register(SRCS ... INCLUDE_DIRS . REQUIRES ...).
