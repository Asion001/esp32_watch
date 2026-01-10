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

#ifdef __cplusplus
}
#endif

#endif // PMU_AXP2101_H
