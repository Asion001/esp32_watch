/**
 * @file pmu_axp2101.h
 * @brief AXP2101 Power Management Unit Driver
 *
 * Simple I2C driver for AXP2101 PMU chip
 * I2C Address: 0x34
 */

#ifndef PMU_AXP2101_H
#define PMU_AXP2101_H

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize PMU communication
     *
     * @param i2c_bus I2C master bus handle from BSP
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t axp2101_init(i2c_master_bus_handle_t i2c_bus);

    /**
     * @brief Read battery voltage in millivolts
     *
     * @param voltage_mv Pointer to store voltage value
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t axp2101_get_battery_voltage(uint16_t *voltage_mv);

    /**
     * @brief Get estimated battery percentage (0-100%)
     *
     * Based on voltage curve:
     * 4.2V = 100%, 3.7V = 50%, 3.3V = 0%
     *
     * @param percent Pointer to store percentage value
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t axp2101_get_battery_percent(uint8_t *percent);

    /**
     * @brief Check if battery is currently charging
     *
     * @param is_charging Pointer to store charging status
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t axp2101_is_charging(bool *is_charging);

    /**
     * @brief Check if USB power is connected
     *
     * @param vbus_present Pointer to store VBUS status
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t axp2101_is_vbus_present(bool *vbus_present);

    /**
     * @brief Safely get all battery data with retry logic and sanity checks
     *
     * This is the recommended function for UI updates. It includes:
     * - Automatic retry on I2C errors (up to 3 attempts)
     * - Sanity checks on all values
     * - Graceful fallback to safe defaults on failure
     *
     * Note: AXP2101 does not have battery current measurement capability.
     * For current monitoring, add external INA219/INA226 sensor.
     *
     * @param voltage_mv Pointer to store voltage (or NULL to skip)
     * @param percent Pointer to store percentage (or NULL to skip)
     * @param is_charging Pointer to store charging status (or NULL to skip)
     * @return esp_err_t ESP_OK if at least one value was read successfully
     */
    esp_err_t axp2101_get_battery_data_safe(uint16_t *voltage_mv, uint8_t *percent,
                                            bool *is_charging);

    /**
     * @brief Enable or disable battery charging
     *
     * Controls the AXP2101 Cell Battery charge enable bit (register 0x18, bit 1).
     * Use this to disable charging for accurate power consumption testing.
     *
     * @param enable true to enable charging, false to disable
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t axp2101_set_charging_enabled(bool enable);

#ifdef __cplusplus
}
#endif

#endif // PMU_AXP2101_H
