/**
 * @file button_handler.h
 * @brief Physical button handling component for ESP32 watch
 *
 * Handles boot button presses for navigation and system control:
 * - Short press: Go back or return to watchface
 * - Long press (3s): Restart device
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "esp_err.h"
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Button handler configuration
     */
    typedef struct
    {
        int gpio_num;                /*!< GPIO number for button (default: 9 for BOOT button) */
        lv_obj_t **tileview;         /*!< Pointer to tileview object for home navigation */
        uint32_t long_press_ms;      /*!< Duration for long press in ms (default: 3000) */
        uint32_t short_press_max_ms; /*!< Max duration for short press in ms (default: 500) */
        uint32_t debounce_ms;        /*!< Debounce time in ms (default: 300) */
    } button_handler_config_t;

/**
 * @brief Default configuration for button handler
 */
#define BUTTON_HANDLER_CONFIG_DEFAULT() \
    {                                   \
        .gpio_num = 9,                  \
        .tileview = NULL,               \
        .long_press_ms = 3000,          \
        .short_press_max_ms = 500,      \
        .debounce_ms = 300}

    /**
     * @brief Initialize and start button handler task
     *
     * Creates a FreeRTOS task that monitors the button and handles:
     * - Short press: screen_manager_go_back() or tileview home
     * - Long press: esp_restart()
     *
     * @param config Button handler configuration
     * @return ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t button_handler_init(const button_handler_config_t *config);

    /**
     * @brief Stop button handler task
     *
     * @return ESP_OK on success, ESP_FAIL if not running
     */
    esp_err_t button_handler_deinit(void);

    /**
     * @brief Check if button handler is running
     *
     * @return true if running, false otherwise
     */
    bool button_handler_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_HANDLER_H
