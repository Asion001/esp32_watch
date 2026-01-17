/**
 * @file button_handler.c
 * @brief Physical button handling implementation
 */

#include "button_handler.h"
#include "sleep_manager.h"
#include "screen_manager.h"
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ButtonHandler";

/** Button handler state */
static struct
{
    TaskHandle_t task_handle;
    button_handler_config_t config;
    bool running;
} s_button = {
    .task_handle = NULL,
    .running = false};

/**
 * @brief Button monitoring task
 */
static void button_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Button monitor task started (GPIO %d)", s_button.config.gpio_num);

    // Configure button GPIO as input with pull-up
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << s_button.config.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&button_conf);

    uint32_t press_start_ms = 0;
    uint32_t last_release_ms = 0;
    bool was_pressed = false;
    bool long_press_triggered = false;

    while (s_button.running)
    {
        // Button is active-low (pressed = 0)
        bool is_pressed = (gpio_get_level(s_button.config.gpio_num) == 0);

        if (is_pressed && !was_pressed)
        {
            // Button just pressed
            uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

            // Reset sleep inactivity timer on any press
            sleep_manager_reset_timer();

#ifdef CONFIG_SLEEP_MANAGER_ENABLE
            // Turn on backlight if it is off
            if (sleep_manager_is_backlight_off())
            {
                sleep_manager_backlight_on();
            }
#endif

            // Check debounce time
            if (last_release_ms > 0 &&
                (current_ms - last_release_ms) < s_button.config.debounce_ms)
            {
                ESP_LOGD(TAG, "Button press ignored (debounce)");
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            press_start_ms = current_ms;
            was_pressed = true;
            long_press_triggered = false;
            ESP_LOGD(TAG, "Button pressed");
        }
        else if (is_pressed && was_pressed)
        {
            // Button still pressed - check duration
            uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t press_duration = current_ms - press_start_ms;

            if (press_duration >= s_button.config.long_press_ms && !long_press_triggered)
            {
                ESP_LOGI(TAG, "Long press detected - restarting...");
                long_press_triggered = true;
                vTaskDelay(pdMS_TO_TICKS(100)); // Small delay for log to flush
                esp_restart();
            }
        }
        else if (!is_pressed && was_pressed)
        {
            // Button released
            uint32_t current_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t press_duration = current_ms - press_start_ms;
            was_pressed = false;
            last_release_ms = current_ms;

            // Handle short press - navigate back/home
            if (!long_press_triggered && press_duration < s_button.config.short_press_max_ms)
            {
                ESP_LOGI(TAG, "Short press - navigating back");

                // Acquire display lock
                if (!bsp_display_lock(100))
                {
                    ESP_LOGW(TAG, "Failed to acquire display lock");
                }
                else
                {
                    // Check if we can go back via screen manager
                    if (screen_manager_can_go_back())
                    {
                        ESP_LOGI(TAG, "Going back from managed screen");
                        screen_manager_go_back();
                    }
                    else if (s_button.config.tileview && *s_button.config.tileview)
                    {
                        // Check if we're on a different screen than tileview's parent
                        lv_obj_t *active_screen = lv_scr_act();
                        lv_obj_t *tileview_screen = lv_obj_get_parent(*s_button.config.tileview);

                        if (active_screen != tileview_screen)
                        {
                            // We're on some other screen - try screen manager anyway
                            ESP_LOGI(TAG, "On non-tileview screen, attempting go_back");
                            screen_manager_go_back();
                        }
                        else
                        {
                            // We're on tileview - navigate to home tile (0, 0)
                            ESP_LOGI(TAG, "Returning to watchface");
                            lv_tileview_set_tile_by_index(*s_button.config.tileview, 0, 0, LV_ANIM_ON);
                        }
                    }
                    else
                    {
                        ESP_LOGD(TAG, "No navigation target available");
                    }

                    bsp_display_unlock();
                }
            }

            ESP_LOGD(TAG, "Button released (duration: %lu ms)", press_duration);
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
    }

    ESP_LOGI(TAG, "Button monitor task stopped");
    vTaskDelete(NULL);
}

esp_err_t button_handler_init(const button_handler_config_t *config)
{
    if (s_button.running)
    {
        ESP_LOGW(TAG, "Button handler already running");
        return ESP_OK;
    }

    if (!config)
    {
        ESP_LOGE(TAG, "Invalid config (NULL)");
        return ESP_ERR_INVALID_ARG;
    }

    // Store configuration
    s_button.config = *config;
    s_button.running = true;

    // Create button monitor task
    BaseType_t ret = xTaskCreate(
        button_monitor_task,
        "button_mon",
        2048,
        NULL,
        5, // Priority
        &s_button.task_handle);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create button monitor task");
        s_button.running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Button handler initialized");
    return ESP_OK;
}

esp_err_t button_handler_deinit(void)
{
    if (!s_button.running)
    {
        return ESP_OK;
    }

    s_button.running = false;
    // Task will delete itself when it sees running = false

    ESP_LOGI(TAG, "Button handler stopped");
    return ESP_OK;
}

bool button_handler_is_running(void)
{
    return s_button.running;
}
