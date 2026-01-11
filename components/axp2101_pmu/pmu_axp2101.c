/**
 * @file pmu_axp2101.c
 * @brief AXP2101 Power Management Unit Driver Implementation
 */

#include "pmu_axp2101.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "PMU";

// AXP2101 I2C Address
#define AXP2101_I2C_ADDR 0x34

// AXP2101 Register Addresses (from datasheet page 24)
#define AXP2101_REG_STATUS 0x00     // Status register
#define AXP2101_REG_CHG_STATUS 0x01 // Charge status register
#define AXP2101_REG_ADC_ENABLE 0x30 // ADC enable control
#define AXP2101_REG_VBAT_H 0x34     // Battery voltage high byte (14-bit ADC)
#define AXP2101_REG_VBAT_L 0x35     // Battery voltage low byte
#define AXP2101_REG_FUEL_GAUGE                                                 \
  0xA4                          // Battery percentage from fuel gauge (0-100%)
#define AXP2101_REG_VBUS_H 0x38 // VBUS voltage high byte
#define AXP2101_REG_VBUS_L 0x39 // VBUS voltage low byte
#define AXP2101_REG_VBUS_STATUS 0x00 // VBUS status (bit 5)

// Voltage calculation constants
#define VBAT_MIN_MV 3300     // 0% battery
#define VBAT_NOMINAL_MV 3700 // ~50% battery
#define VBAT_MAX_MV 4200     // 100% battery

// Sanity check constants
#define VBAT_ABSOLUTE_MIN_MV 2500 // Below this is invalid/hardware error
#define VBAT_ABSOLUTE_MAX_MV 4500 // Above this is invalid/hardware error
#define IBAT_MAX_MA 3000          // Maximum realistic current (+/- 3A)
#define I2C_RETRY_COUNT 3         // Number of retries for I2C operations

static i2c_master_bus_handle_t i2c_handle = NULL;
static i2c_master_dev_handle_t pmu_dev = NULL;

esp_err_t axp2101_init(i2c_master_bus_handle_t i2c_bus) {
  if (!i2c_bus) {
    ESP_LOGE(TAG, "Invalid I2C bus handle");
    return ESP_ERR_INVALID_ARG;
  }

  i2c_handle = i2c_bus;

  // Add AXP2101 device to I2C bus
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = AXP2101_I2C_ADDR,
      .scl_speed_hz = 400000, // 400kHz I2C
  };

  esp_err_t ret = i2c_master_bus_add_device(i2c_handle, &dev_cfg, &pmu_dev);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add PMU device: %s", esp_err_to_name(ret));
    return ret;
  }

  // Enable ADC for battery voltage and current measurements
  // Bit 7: VBAT ADC, Bit 6: IBAT charge, Bit 5: IBAT discharge
  uint8_t adc_enable = 0xE0; // Enable VBAT, IBAT charge, IBAT discharge ADCs
  ret = i2c_master_transmit(pmu_dev,
                            (uint8_t[]){AXP2101_REG_ADC_ENABLE, adc_enable}, 2,
                            1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to enable ADCs: %s (continuing anyway)",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "ADC enabled: 0x%02X", adc_enable);
  }

  // Small delay to let ADC stabilize
  vTaskDelay(pdMS_TO_TICKS(50));

  ESP_LOGI(TAG, "PMU AXP2101 initialized");
  return ESP_OK;
}

esp_err_t axp2101_get_battery_voltage(uint16_t *voltage_mv) {
  if (!pmu_dev || !voltage_mv) {
    ESP_LOGE(TAG, "PMU not initialized or invalid parameter");
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t data[2];

  // Read battery voltage registers (14-bit ADC value)
  esp_err_t ret =
      i2c_master_transmit_receive(pmu_dev, (uint8_t[]){AXP2101_REG_VBAT_H}, 1,
                                  data, 2, 1000 / portTICK_PERIOD_MS);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read battery voltage: %s", esp_err_to_name(ret));
    return ret;
  }

  // AXP2101 VBAT ADC: Simple 16-bit value, 1mV per LSB
  *voltage_mv = ((uint16_t)data[0] << 8) | data[1];

  return ESP_OK;
}

esp_err_t axp2101_get_battery_percent(uint8_t *percent) {
  if (!percent) {
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t voltage_mv;
  esp_err_t ret = axp2101_get_battery_voltage(&voltage_mv);
  if (ret != ESP_OK) {
    return ret;
  }

  // Simple linear mapping with two slopes
  // 3.3V (0%) -> 3.7V (50%) -> 4.2V (100%)
  int calculated_percent;

  if (voltage_mv <= VBAT_MIN_MV) {
    calculated_percent = 0;
  } else if (voltage_mv >= VBAT_MAX_MV) {
    calculated_percent = 100;
  } else if (voltage_mv < VBAT_NOMINAL_MV) {
    // 3.3V to 3.7V range (0% to 50%)
    calculated_percent =
        (voltage_mv - VBAT_MIN_MV) * 50 / (VBAT_NOMINAL_MV - VBAT_MIN_MV);
  } else {
    // 3.7V to 4.2V range (50% to 100%)
    calculated_percent = 50 + (voltage_mv - VBAT_NOMINAL_MV) * 50 /
                                  (VBAT_MAX_MV - VBAT_NOMINAL_MV);
  }

  // Clamp to 0-100 range
  *percent = (calculated_percent < 0)     ? 0
             : (calculated_percent > 100) ? 100
                                          : calculated_percent;

  return ESP_OK;
}

esp_err_t axp2101_is_charging(bool *is_charging) {
  if (!pmu_dev || !is_charging) {
    ESP_LOGE(TAG, "PMU not initialized or invalid parameter");
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t status;

  // Read charge status register
  esp_err_t ret =
      i2c_master_transmit_receive(pmu_dev, (uint8_t[]){AXP2101_REG_CHG_STATUS},
                                  1, &status, 1, 1000 / portTICK_PERIOD_MS);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read charge status: %s", esp_err_to_name(ret));
    return ret;
  }

  // Check charging bit (bit 6 in status register)
  // NOTE: Logic appears to be inverted - bit is 0 when charging
  *is_charging = (status & 0x40) == 0;

  return ESP_OK;
}

esp_err_t axp2101_is_vbus_present(bool *vbus_present) {
  if (!pmu_dev || !vbus_present) {
    ESP_LOGE(TAG, "PMU not initialized or invalid parameter");
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t status;

  // Read status register
  esp_err_t ret =
      i2c_master_transmit_receive(pmu_dev, (uint8_t[]){AXP2101_REG_STATUS}, 1,
                                  &status, 1, 1000 / portTICK_PERIOD_MS);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read VBUS status: %s", esp_err_to_name(ret));
    return ret;
  }

  // Check VBUS present bit (bit 5 in status register)
  *vbus_present = (status & 0x20) != 0;

  return ESP_OK;
}

esp_err_t axp2101_get_battery_data_safe(uint16_t *voltage_mv, uint8_t *percent,
                                        bool *is_charging) {
  if (!pmu_dev) {
    ESP_LOGE(TAG, "PMU not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  bool success = false;
  esp_err_t ret;

  // Read voltage with retry
  if (voltage_mv) {
    uint16_t temp_voltage = 0;
    for (int retry = 0; retry < I2C_RETRY_COUNT; retry++) {
      ret = axp2101_get_battery_voltage(&temp_voltage);
      if (ret == ESP_OK) {
        // Sanity check
        if (temp_voltage >= VBAT_ABSOLUTE_MIN_MV &&
            temp_voltage <= VBAT_ABSOLUTE_MAX_MV) {
          *voltage_mv = temp_voltage;
          success = true;
          break;
        } else {
          ESP_LOGW(TAG, "Voltage out of range: %u mV (attempt %d)",
                   temp_voltage, retry + 1);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(10)); // Small delay before retry
    }

    if (!success) {
      ESP_LOGW(TAG,
               "Failed to read valid voltage after %d attempts, using default",
               I2C_RETRY_COUNT);
      *voltage_mv = VBAT_NOMINAL_MV; // Safe default
    }
  }

  // Read percentage with retry
  if (percent) {
    bool percent_success = false;
    uint8_t temp_percent = 0;
    for (int retry = 0; retry < I2C_RETRY_COUNT; retry++) {
      ret = axp2101_get_battery_percent(&temp_percent);
      if (ret == ESP_OK && temp_percent <= 100) {
        *percent = temp_percent;
        percent_success = true;
        success = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!percent_success) {
      ESP_LOGW(TAG, "Failed to read valid percentage, using default");
      *percent = 50; // Safe default
    }
  }

  // Read charging status with retry
  if (is_charging) {
    bool charging_success = false;
    bool temp_charging = false;
    for (int retry = 0; retry < I2C_RETRY_COUNT; retry++) {
      ret = axp2101_is_charging(&temp_charging);
      if (ret == ESP_OK) {
        *is_charging = temp_charging;
        charging_success = true;
        success = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!charging_success) {
      ESP_LOGW(TAG, "Failed to read charging status, using default");
      *is_charging = false; // Safe default
    }
  }

  return success ? ESP_OK : ESP_FAIL;
}
