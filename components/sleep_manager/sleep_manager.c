/**
 * @file sleep_manager.c
 * @brief Sleep and Power Management Implementation
 */

#include "sleep_manager.h"

#ifdef CONFIG_SLEEP_MANAGER_ENABLE

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"

static const char *TAG = "SleepMgr";

// Debug logging macro
#ifdef CONFIG_SLEEP_MANAGER_DEBUG_LOGS
#define SLEEP_LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#else
#define SLEEP_LOGD(tag, format, ...) ((void)0)
#endif

// Inactivity tracking
static int64_t last_activity_time = 0;
static bool is_sleeping = false;

// Store LVGL objects for sleep/wake control
typedef struct
{
    lv_timer_t *timer;
} timer_state_t;

#define MAX_TIMERS 8
static timer_state_t saved_timers[MAX_TIMERS];
static uint8_t saved_timer_count = 0;

/**
 * @brief Turn off display and backlight
 */
static esp_err_t display_sleep(void)
{
#ifdef CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL
    // Turn off backlight
    bsp_display_backlight_off();

    // Small delay to allow backlight to fade
    vTaskDelay(pdMS_TO_TICKS(100));

    // Note: We can't call esp_lcd_panel_disp_on_off() directly here
    // because panel_handle is private in BSP. For now, backlight off
    // is sufficient. True display sleep would require BSP modification.

    ESP_LOGI(TAG, "Display sleep (backlight off)");
#else
    ESP_LOGI(TAG, "Display sleep (backlight control disabled)");
#endif
    return ESP_OK;
}

/**
 * @brief Wake up display and backlight
 */
static esp_err_t display_wake(void)
{
#ifdef CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL
    // Turn on backlight
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Display wake (backlight on)");
#else
    ESP_LOGI(TAG, "Display wake (backlight control disabled)");
#endif
    return ESP_OK;
}

/**
#ifdef CONFIG_SLEEP_MANAGER_LVGL_TIMER_PAUSE
    saved_timer_count = 0;

    // Get first timer
    lv_timer_t *timer = lv_timer_get_next(NULL);

    while (timer != NULL && saved_timer_count < MAX_TIMERS) {
        // Save timer reference
        saved_timers[saved_timer_count].timer = timer;

        // Pause timer
        lv_timer_pause(timer);
        SLEEP_LOGD(TAG, "Paused timer %d", saved_timer_count);

        saved_timer_count++;
        timer = lv_timer_get_next(timer);
    }

    ESP_LOGI(TAG, "Paused %d LVGL timers", saved_timer_count);
#else
    ESP_LOGI(TAG, "LVGL timer pause disabled");
#endif
        timer = lv_timer_get_next(timer);
    }

    ESP_LOGI(TAG, "Paused %d LVGL timers", saved_timer_count);
}

/**
 * @brief Resume previously paused LVGL timers
 */
static void resume_lvgl_timers(void)
{
#ifdef CONFIG_SLEEP_MANAGER_LVGL_TIMER_PAUSE
    for (uint8_t i = 0; i < saved_timer_count; i++)
    {
        lv_timer_t *timer = saved_timers[i].timer;

        // Resume timer
        lv_timer_resume(timer);
        lv_timer_ready(timer); // Force immediate execution
        SLEEP_LOGD(TAG, "Resumed timer %d", i);
    }

    ESP_LOGI(TAG, "Resumed %d LVGL timers", saved_timer_count);
    saved_timer_count = 0;
#else
    ESP_LOGI(TAG, "LVGL timer resume disabled");
#endif
}
(timeout : % d seconds) ", CONFIG_SLEEP_TIMEOUT_SECONDS);

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
    // Configure GPIO15 (touch interrupt) as wake source
    // ESP32-C6 uses gpio_wakeup, not ext0/ext1
    // Touch controller pulls INT low when touch detected

    // Configure GPIO as input with pull-up (touch INT is active-low)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TOUCH_INT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
gpio_config(&io_conf);

// Enable GPIO wakeup (wake on LOW level - touch detected)
esp_err_t ret = gpio_wakeup_enable(TOUCH_INT_GPIO, GPIO_INTR_LOW_LEVEL);
if (ret != ESP_OK)
{
    ESP_LOGE(TAG, "Failed to enable GPIO wakeup: %s", esp_err_to_name(ret));
    return ret;
}

// Enable sleep wakeup from GPIO
ret = esp_sleep_enable_gpio_wakeup();
if (ret != ESP_OK)
{
    ESP_LOGE(TAG, "Failed to enable sleep GPIO wakeup: %s", esp_err_to_name(ret));
    return ret;
}

ESP_LOGI(TAG, "GPIO wake-up enabled on GPIO%d", TOUCH_INT_GPIO);
#else
    ESP_LOGI(TAG, "GPIO wake-up disabled");
#endif

// Keep RTC peripherals powered during sleep (for RTC chip, timers)
esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

// Initialize activity timer
last_activity_time = esp_timer_get_time();
is_sleeping = false;

    ESP_LOGI(TAG, "Sleep manager initialized";
    is_sleeping = false;
    
    ESP_LOGI(TAG, "Sleep manager initialized (wake GPIO: %d, timeout: %d ms)",
             TOUCH_INT_GPIO, SLEEP_TIMEOUT_MS);
    
    return ESP_OK;
    }

    esp_err_t sleep_manager_sleep(void)
    {
        if (is_sleeping)
        {
            ESP_LOGW(TAG, "Already in sleep mode");
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Entering sleep mode...");

        // Lock LVGL before modifying timers
        bsp_display_lock(0);

        // Pause all LVGL timers
        pause_lvgl_timers();

#ifdef CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL
        // Disable LVGL rendering (optional optimization)
        lv_display_t *disp = lv_display_get_default();
        if (disp)
        {
            lv_display_enable_invalidation(disp, false);
            ESP_LOGI(TAG, "LVGL rendering disabled");
        }
#endif

        bsp_display_unlock();

        // Turn off display
        display_sleep();

        // Mark as sleeping
        is_sleeping = true;

        // Enter light sleep - BLOCKS until wake-up event
        ESP_LOGI(TAG, "Entering light sleep (wake sources: touch GPIO%d)", TOUCH_INT_GPIO);

        int64_t sleep_start = esp_timer_get_time();
        esp_err_t ret = esp_light_sleep_start();
        int64_t sleep_duration = (esp_timer_get_time() - sleep_start) / 1000; // Convert to ms

        // Check wake-up cause
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        const char *cause_str = (cause == ESP_SLEEP_WAKEUP_EXT0) ? "touch" : (cause == ESP_SLEEP_WAKEUP_TIMER) ? "timer"
                                                                                                               : "unknown";

        ESP_LOGI(TAG, "Woke from light sleep after %lld ms (cause: %s)",
                 sleep_duration, cause_str);

        // Wake up display and timers
        sleep_manager_wake();

        return ret;
    }

    esp_err_t sleep_manager_wake(void)
    {
        if (!is_sleeping)
        {
            ESP_LOGD(TAG, "Not in sleep mode, nothing to wake");
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Waking from sleep mode...");

        // Wake up display
        display_wake();

#ifdef CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL
        // Re-enable LVGL rendering
        lv_display_t *disp = lv_display_get_default();
        if (disp)
        {
            lv_display_enable_invalidation(disp, true);
            ESP_LOGI(TAG, "LVGL rendering enabled");
        }
#endifv_display_t *disp = lv_display_get_default();
        if (disp)
        {
            lv_display_enable_invalidation(disp, true);
        }

        // Resume all LVGL timers
        resume_lvgl_timers();

        bsp_display_unlock();

        // Reset activity timer
        sleep_manager_reset_timer();

        // Mark as awake
        is_sleeping = false;

        ESP_LOGI(TAG, "Wake complete");

        return ESP_OK;
    }

    bool sleep_manager_should_sleep(void)
    {
        if (is_sleeping)
        {
            return false; // Already sleeping
        }

        uint32_t inactive_ms = sleep_manager_get_inactive_time();
        return (inactive_ms >= SLEEP_TIMEOUT_MS);
    }

    void sleep_manager_reset_timer(void)
    {
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER
        last_activity_time = esp_timer_get_time();
        SLEEP_LOGD(TAG, "Activity timer reset");
#endif
    }

    uint32_t sleep_manager_get_inactive_time(void)
    {
        int64_t now = esp_timer_get_time();

#endif                                    // CONFIG_SLEEP_MANAGER_ENABLE   int64_t elapsed_us = now - last_activity_time;
        return (uint32_t)(elapsed_us / 1000); // Convert to milliseconds
    }
