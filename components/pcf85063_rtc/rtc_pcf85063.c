/**
 * @file rtc_pcf85063.c
 * @brief PCF85063 Real-Time Clock Driver Implementation
 */

#include "rtc_pcf85063.h"
#include "build_time.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "RTC";

// PCF85063 I2C Address and Registers
#define PCF85063_I2C_ADDR 0x51
#define PCF85063_REG_SEC 0x04
#define PCF85063_REG_MIN 0x05
#define PCF85063_REG_HOUR 0x06
#define PCF85063_REG_DAY 0x07
#define PCF85063_REG_WKDAY 0x08
#define PCF85063_REG_MONTH 0x09
#define PCF85063_REG_YEAR 0x0A

static i2c_master_bus_handle_t i2c_handle = NULL;
static i2c_master_dev_handle_t rtc_dev = NULL;

/**
 * @brief Convert BCD (Binary Coded Decimal) to decimal
 */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * @brief Convert decimal to BCD (Binary Coded Decimal)
 */
static uint8_t dec_to_bcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

esp_err_t rtc_init(i2c_master_bus_handle_t i2c_bus)
{
    if (!i2c_bus)
    {
        ESP_LOGE(TAG, "Invalid I2C bus handle");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_handle = i2c_bus;

    // Add PCF85063 device to I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_I2C_ADDR,
        .scl_speed_hz = 400000, // 400kHz I2C
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_handle, &dev_cfg, &rtc_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add RTC device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "RTC PCF85063 initialized");

    // Check if time is valid, if not, set to build time
    if (!rtc_is_valid())
    {
        ESP_LOGW(TAG, "RTC time invalid, setting to build time");
        struct tm build_time;
        if (get_build_time(&build_time) == 0)
        {
            ESP_LOGI(TAG, "Setting RTC to: %04d-%02d-%02d %02d:%02d:%02d",
                     build_time.tm_year + 1900, build_time.tm_mon + 1, build_time.tm_mday,
                     build_time.tm_hour, build_time.tm_min, build_time.tm_sec);
            rtc_write_time(&build_time);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to parse build time, using fallback");
            struct tm default_time = {
                .tm_year = 126, // 2026 - 1900
                .tm_mon = 0,    // January (0-11)
                .tm_mday = 10,  // Day 10
                .tm_hour = 12,  // 12:00
                .tm_min = 0,
                .tm_sec = 0,
            };
            rtc_write_time(&default_time);
        }
    }

    return ESP_OK;
}

esp_err_t rtc_read_time(struct tm *time)
{
    if (!rtc_dev || !time)
    {
        ESP_LOGE(TAG, "RTC not initialized or invalid parameter");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[7];

    // Read time registers (0x04-0x0A: seconds to years)
    esp_err_t ret = i2c_master_transmit_receive(
        rtc_dev,
        (uint8_t[]){PCF85063_REG_SEC}, 1,
        data, 7,
        1000 / portTICK_PERIOD_MS);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read RTC: %s", esp_err_to_name(ret));
        return ret;
    }

    // Convert BCD to decimal and fill tm structure
    time->tm_sec = bcd_to_dec(data[0] & 0x7F); // Mask OS bit
    time->tm_min = bcd_to_dec(data[1] & 0x7F);
    time->tm_hour = bcd_to_dec(data[2] & 0x3F); // Mask AMPM bit
    time->tm_mday = bcd_to_dec(data[3] & 0x3F);
    time->tm_wday = bcd_to_dec(data[4] & 0x07);    // Weekday 0-6
    time->tm_mon = bcd_to_dec(data[5] & 0x1F) - 1; // Month 0-11 in tm
    time->tm_year = bcd_to_dec(data[6]) + 100;     // Years since 1900

    return ESP_OK;
}

esp_err_t rtc_write_time(const struct tm *time)
{
    if (!rtc_dev || !time)
    {
        ESP_LOGE(TAG, "RTC not initialized or invalid parameter");
        return ESP_ERR_INVALID_STATE;
    }

    // Prepare data buffer: register address + 7 time bytes
    uint8_t data[8];
    data[0] = PCF85063_REG_SEC;
    data[1] = dec_to_bcd(time->tm_sec) & 0x7F;     // Seconds (clear OS)
    data[2] = dec_to_bcd(time->tm_min) & 0x7F;     // Minutes
    data[3] = dec_to_bcd(time->tm_hour) & 0x3F;    // Hours (24h format)
    data[4] = dec_to_bcd(time->tm_mday) & 0x3F;    // Day
    data[5] = dec_to_bcd(time->tm_wday) & 0x07;    // Weekday
    data[6] = dec_to_bcd(time->tm_mon + 1) & 0x1F; // Month (1-12)
    data[7] = dec_to_bcd(time->tm_year - 100);     // Year (0-99, offset from 2000)

    esp_err_t ret = i2c_master_transmit(rtc_dev, data, 8, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write RTC: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "RTC time set: %04d-%02d-%02d %02d:%02d:%02d",
             time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
             time->tm_hour, time->tm_min, time->tm_sec);

    return ESP_OK;
}

bool rtc_is_valid(void)
{
    if (!rtc_dev)
    {
        return false;
    }

    struct tm time;
    if (rtc_read_time(&time) != ESP_OK)
    {
        return false;
    }

    // Validate year range (2024-2099)
    int year = time.tm_year + 1900;
    if (year < 2024 || year > 2099)
    {
        ESP_LOGW(TAG, "Invalid year: %d", year);
        return false;
    }

    // Validate month (0-11 in tm)
    if (time.tm_mon < 0 || time.tm_mon > 11)
    {
        ESP_LOGW(TAG, "Invalid month: %d", time.tm_mon);
        return false;
    }

    // Validate day (1-31)
    if (time.tm_mday < 1 || time.tm_mday > 31)
    {
        ESP_LOGW(TAG, "Invalid day: %d", time.tm_mday);
        return false;
    }

    // Validate time
    if (time.tm_hour > 23 || time.tm_min > 59 || time.tm_sec > 59)
    {
        ESP_LOGW(TAG, "Invalid time: %02d:%02d:%02d",
                 time.tm_hour, time.tm_min, time.tm_sec);
        return false;
    }

    return true;
}
