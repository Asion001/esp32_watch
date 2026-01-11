/**
 * @file rtc_pcf85063.h
 * @brief PCF85063 Real-Time Clock Driver
 *
 * Simple I2C driver for PCF85063 RTC chip
 * I2C Address: 0x51
 */

#ifndef RTC_PCF85063_H
#define RTC_PCF85063_H

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize RTC communication
 *
 * @param i2c_bus I2C master bus handle from BSP
 * @return esp_err_t ESP_OK on success
 */
esp_err_t rtc_init(i2c_master_bus_handle_t i2c_bus);

/**
 * @brief Read current time from RTC
 *
 * @param time Pointer to tm structure to fill
 * @return esp_err_t ESP_OK on success
 */
esp_err_t rtc_read_time(struct tm *time);

/**
 * @brief Write time to RTC
 *
 * @param time Pointer to tm structure with time to set
 * @return esp_err_t ESP_OK on success
 */
esp_err_t rtc_write_time(const struct tm *time);

/**
 * @brief Check if RTC time is valid (initialized)
 *
 * Validates year range (2024-2099) and value sanity
 *
 * @return true if time is valid, false if uninitialized/invalid
 */
bool rtc_is_valid(void);

#ifdef __cplusplus
}
#endif

#endif // RTC_PCF85063_H
