/**
 * @file sleep_manager.h
 * @brief Sleep and Power Management for ESP32-C6 Watch
 *
 * Handles display sleep, light sleep mode, and wake-up events.
 * Wake sources: boot button (GPIO9) always enabled, touch (GPIO15) optional.
 *
 * Features can be enabled/disabled via menuconfig:
 * Component config → Sleep Manager Configuration
 */

#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include "esp_err.h"
#include "lvgl.h"
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Boot button GPIO (hardware button wake source) */
#define BOOT_BUTTON_GPIO (9)

// Only compile if sleep manager is enabled
#ifdef CONFIG_SLEEP_MANAGER_ENABLE

/** Inactivity timeout before turning off backlight (milliseconds) */
#define BACKLIGHT_TIMEOUT_MS (CONFIG_SLEEP_MANAGER_BACKLIGHT_TIMEOUT_SECONDS * 1000)

/** Inactivity timeout before entering sleep (milliseconds) */
#define SLEEP_TIMEOUT_MS (CONFIG_SLEEP_TIMEOUT_SECONDS * 1000)

/** Touch interrupt GPIO (FT3168 INT pin) */
#define TOUCH_INT_GPIO (15)

    /**
     * @brief Initialize sleep manager
     *
     * Sets up wake-up sources and prepares sleep subsystem
     *
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t sleep_manager_init(void);

    /**
     * @brief Enter display sleep and light sleep mode
     *
     * - Pauses LVGL timers
     * - Turns off display and backlight
     * - Enters ESP32-C6 light sleep
     * - Blocks until wake-up event (touch or timeout)
     *
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t sleep_manager_sleep(void);

    /**
     * @brief Wake from sleep mode
     *
     * - Wakes display and backlight
     * - Resumes LVGL timers
     * - Forces immediate UI update
     *
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t sleep_manager_wake(void);

    /**
     * @brief Check if system should enter sleep mode
     *
     * Call this periodically to check if inactivity timeout expired
     *
     * @return true if sleep should be entered, false otherwise
     */
    bool sleep_manager_should_sleep(void);

    /**
     * @brief Reset inactivity timer
     *
     * Call this on touch events or other user activity
     */
    void sleep_manager_reset_timer(void);

    /**
     * @brief Get current inactivity time in milliseconds
     *
     * @return Milliseconds since last activity
     */
    uint32_t sleep_manager_get_inactive_time(void);

    /**
     * @brief Check if USB/JTAG is connected
     *
     * Uses AXP2101 PMU to detect VBUS presence
     *
     * @return true if USB power is detected, false otherwise
     */
    bool sleep_manager_is_usb_connected(void);

    /**
     * @brief Check if backlight should turn off
     *
     * Call this periodically to check if backlight timeout expired
     *
     * @return true if backlight should turn off, false otherwise
     */
    bool sleep_manager_should_turn_off_backlight(void);

    /**
     * @brief Turn off backlight (independent of sleep)
     *
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t sleep_manager_backlight_off(void);

    /**
     * @brief Turn on backlight (independent of sleep)
     *
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t sleep_manager_backlight_on(void);

    /**
     * @brief Check if backlight is currently off
     *
     * @return true if backlight is off, false if on
     */
    bool sleep_manager_is_backlight_off(void);

#else // !CONFIG_SLEEP_MANAGER_ENABLE

// Stub functions when sleep manager is disabled
static inline esp_err_t sleep_manager_init(void) { return ESP_OK; }
static inline esp_err_t sleep_manager_sleep(void) { return ESP_OK; }
static inline esp_err_t sleep_manager_wake(void) { return ESP_OK; }
static inline bool sleep_manager_should_sleep(void) { return false; }
static inline void sleep_manager_reset_timer(void) {}
static inline uint32_t sleep_manager_get_inactive_time(void) { return 0; }
static inline bool sleep_manager_is_usb_connected(void) { return false; }

#endif // CONFIG_SLEEP_MANAGER_ENABLE

#ifdef __cplusplus
}
#endif

#endif // SLEEP_MANAGER_H
