/**
 * @file sleep_manager.c
 * @brief Sleep and Power Management Implementation
 */

#include "sleep_manager.h"

#ifdef CONFIG_SLEEP_MANAGER_ENABLE

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pmu_axp2101.h"
#include "uptime_tracker.h"
#ifdef CONFIG_ENABLE_WIFI
#include "wifi_manager.h"
#endif
#include <string.h>

static const char *TAG = "SleepMgr";

// Debug logging macro
#ifdef CONFIG_SLEEP_MANAGER_DEBUG_LOGS
#define SLEEP_LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#else
#define SLEEP_LOGD(tag, format, ...) ((void)0)
#endif

// Inactivity tracking
static int64_t last_activity_time = 0;
static int64_t last_user_activity_time = 0;
static bool is_sleeping = false;
static bool is_backlight_off = false;

RTC_DATA_ATTR static sleep_manager_sleep_type_t last_sleep_type =
    SLEEP_MANAGER_SLEEP_TYPE_NONE;

// Task handle for sleep monitoring
static TaskHandle_t sleep_task_handle = NULL;

// Store LVGL objects for sleep/wake control
typedef struct
{
  lv_timer_t *timer;
} timer_state_t;

#define MAX_TIMERS 8
static timer_state_t saved_timers[MAX_TIMERS];
static uint8_t saved_timer_count = 0;

#ifdef CONFIG_SLEEP_MANAGER_WIFI_SUSPEND
static void sleep_manager_suspend_wifi(void)
{
#ifdef CONFIG_ENABLE_WIFI
  ESP_LOGI(TAG, "Suspending WiFi for sleep");
  esp_err_t ret = wifi_manager_deinit();
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "WiFi deinit failed: %s", esp_err_to_name(ret));
  }
#endif
}

static void sleep_manager_resume_wifi(void)
{
#ifdef CONFIG_ENABLE_WIFI
  ESP_LOGI(TAG, "Resuming WiFi after wake");
  esp_err_t ret = wifi_manager_init();
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return;
  }

#ifdef CONFIG_WIFI_AUTO_CONNECT
  esp_err_t auto_ret = wifi_manager_auto_connect();
  if (auto_ret != ESP_OK && auto_ret != ESP_ERR_NOT_FOUND)
  {
    ESP_LOGW(TAG, "WiFi auto-connect failed: %s", esp_err_to_name(auto_ret));
  }
#endif
#endif
}
#endif

static bool sleep_manager_lock_display_with_retry(uint32_t timeout_ms,
                                                  uint8_t retries,
                                                  uint32_t delay_ms)
{
  for (uint8_t attempt = 0; attempt <= retries; attempt++)
  {
    if (bsp_display_lock(timeout_ms))
    {
      return true;
    }

    if (delay_ms > 0)
    {
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
  }

  return false;
}

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
static void log_power_state(const char *label)
{
  uint16_t voltage_mv = 0;
  uint8_t percent = 0;
  bool charging = false;
  bool vbus = false;

  esp_err_t ret =
      axp2101_get_battery_data_safe(&voltage_mv, &percent, &charging);
  esp_err_t vbus_ret = axp2101_is_vbus_present(&vbus);

  if (ret == ESP_OK)
  {
    uint16_t volts_int = voltage_mv / 1000;
    uint16_t volts_frac = (voltage_mv % 1000) / 10;
    ESP_LOGI(TAG, "Power %s: %u.%02uV %u%% %s vbus=%d", label, volts_int,
             volts_frac, percent, charging ? "charging" : "discharging",
             vbus);
  }
  else
  {
    ESP_LOGW(TAG, "Power %s: battery read failed (%s)", label,
             esp_err_to_name(ret));
  }

  if (vbus_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Power %s: vbus read failed (%s)", label,
             esp_err_to_name(vbus_ret));
  }
}
#endif

/**
 * @brief Turn off display and backlight (internal helper for sleep)
 */
static esp_err_t display_sleep(void)
{
#ifdef CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL
  if (!is_backlight_off)
  {
#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SCREEN_OFF_ON_USB
    // Don't turn off screen if USB/JTAG is connected (for
    // development/debugging)
    if (sleep_manager_is_usb_connected())
    {
      SLEEP_LOGD(TAG, "USB connected - screen off prevented");
      return ESP_OK;
    }
#endif
    // Turn off backlight
    bsp_display_backlight_off();
    is_backlight_off = true;

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
    log_power_state("backlight_off");
#endif

    // Small delay to allow backlight to fade
    vTaskDelay(pdMS_TO_TICKS(100));

    // Note: We can't call esp_lcd_panel_disp_on_off() directly here
    // because panel_handle is private in BSP. For now, backlight off
    // is sufficient. True display sleep would require BSP modification.

    ESP_LOGI(TAG, "Display sleep (backlight off)");
  }
#else
  ESP_LOGI(TAG, "Display sleep (backlight control disabled)");
#endif
  return ESP_OK;
}

/**
 * @brief Wake up display and backlight (internal helper for wake)
 */
static esp_err_t display_wake(void)
{
#ifdef CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL
  if (is_backlight_off)
  {
    // Turn on backlight
    bsp_display_backlight_on();
    is_backlight_off = false;

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
    log_power_state("backlight_on");
#endif

    ESP_LOGI(TAG, "Display wake (backlight on)");
  }
#else
  ESP_LOGI(TAG, "Display wake (backlight control disabled)");
#endif
  return ESP_OK;
}

/**
 * @brief Pause all LVGL timers to save power during sleep
 */
static void pause_lvgl_timers(void)
{
#ifdef CONFIG_SLEEP_MANAGER_LVGL_TIMER_PAUSE
  saved_timer_count = 0;
  memset(saved_timers, 0, sizeof(saved_timers));

  // Get first timer
  lv_timer_t *timer = lv_timer_get_next(NULL);

  while (timer != NULL && saved_timer_count < MAX_TIMERS)
  {
    // Save timer reference
    saved_timers[saved_timer_count].timer = timer;

    // Pause timer
    lv_timer_pause(timer);
    SLEEP_LOGD(TAG, "Paused timer %d", saved_timer_count);

    saved_timer_count++;
    timer = lv_timer_get_next(timer);
  }

  if (timer != NULL && saved_timer_count >= MAX_TIMERS)
  {
    ESP_LOGW(TAG, "LVGL timer pause limit reached (%d), some timers not paused",
             MAX_TIMERS);
  }
  else if (saved_timer_count == 0)
  {
    ESP_LOGI(TAG, "No LVGL timers found to pause");
  }

  ESP_LOGI(TAG, "Paused %d LVGL timers", saved_timer_count);
#else
  ESP_LOGI(TAG, "LVGL timer pause disabled");
#endif
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

    if (timer)
    {
      // Resume timer
      lv_timer_resume(timer);
      lv_timer_ready(timer); // Force immediate execution
      SLEEP_LOGD(TAG, "Resumed timer %d", i);
    }

    saved_timers[i].timer = NULL;
  }

  ESP_LOGI(TAG, "Resumed %d LVGL timers", saved_timer_count);
  saved_timer_count = 0;
#else
  ESP_LOGI(TAG, "LVGL timer resume disabled");
#endif
}

/**
 * @brief Global event handler for touch events
 * Resets sleep timer and turns on backlight on any screen touch
 */
static void global_touch_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)
  {
    if (is_sleeping)
    {
      return;
    }

    // Reset inactivity timer on touch
    sleep_manager_reset_timer();

    // Turn on backlight if it's off
    if (sleep_manager_is_backlight_off())
    {
      sleep_manager_backlight_on();
    }
  }
}

#ifdef CONFIG_SLEEP_MANAGER_DEEP_SLEEP_ENABLE
static uint32_t sleep_manager_get_user_inactive_time(void)
{
  int64_t now = esp_timer_get_time();
  int64_t elapsed_us = now - last_user_activity_time;
  return (uint32_t)(elapsed_us / 1000); // Convert to milliseconds
}
#endif

/**
 * @brief Sleep check task - monitors inactivity and triggers backlight off and
 * sleep separately
 */
static void sleep_check_task(void *pvParameters)
{
  ESP_LOGI(
      TAG,
      "Sleep check task started (backlight timeout: %ds, sleep timeout: %ds)",
      CONFIG_SLEEP_MANAGER_BACKLIGHT_TIMEOUT_SECONDS,
      CONFIG_SLEEP_TIMEOUT_SECONDS);

  while (1)
  {
    // Check every 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // Check backlight timeout first (independent of sleep)
    if (sleep_manager_should_turn_off_backlight())
    {
      ESP_LOGI(TAG, "Backlight inactivity timeout - turning off backlight");
      sleep_manager_backlight_off();
    }

#ifdef CONFIG_SLEEP_MANAGER_DEEP_SLEEP_ENABLE
    if (!is_sleeping)
    {
      uint32_t user_inactive_ms = sleep_manager_get_user_inactive_time();
      if (user_inactive_ms >= DEEP_SLEEP_TIMEOUT_MS)
      {
        bool allow_deep_sleep = true;

#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SLEEP_ON_USB
        if (sleep_manager_is_usb_connected())
        {
          SLEEP_LOGD(TAG, "USB connected - deep sleep prevented");
          allow_deep_sleep = false;
        }
#endif

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
        if (allow_deep_sleep)
        {
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
          if (gpio_get_level(TOUCH_INT_GPIO) == 0)
          {
            SLEEP_LOGD(TAG, "Touch interrupt active - deep sleep aborted");
            allow_deep_sleep = false;
          }
#endif
          if (allow_deep_sleep && gpio_get_level(BOOT_BUTTON_GPIO) == 0)
          {
            SLEEP_LOGD(TAG, "Button pressed - deep sleep aborted");
            allow_deep_sleep = false;
          }
        }
#endif

        if (allow_deep_sleep)
        {
          is_sleeping = true;
          display_sleep();
          sleep_manager_enter_deep_sleep();
        }
      }
    }
#endif

    // Check sleep timeout (only if not already sleeping)
    if (sleep_manager_should_sleep())
    {
      ESP_LOGI(TAG, "Sleep inactivity timeout - entering sleep mode");

      // Enter sleep (blocks until wake-up)
      esp_err_t ret = sleep_manager_sleep();

      if (ret != ESP_OK)
      {
        ESP_LOGE(TAG, "Sleep failed: %s", esp_err_to_name(ret));
      }
    }
  }
}

/**
 * @brief Initialize sleep manager subsystem
 */
esp_err_t sleep_manager_init(void)
{
  ESP_LOGI(TAG, "Initializing sleep manager (timeout: %d seconds)",
           CONFIG_SLEEP_TIMEOUT_SECONDS);

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
  // Configure wake sources: boot button (GPIO9) and optionally touch (GPIO15)
  // ESP32-C6 uses gpio_wakeup, not ext0/ext1
  // Touch controller pulls INT low when touch detected
  // Boot button connects to ground when pressed

#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
  // NOTE: Do NOT reconfigure GPIO15 - the BSP/touch driver already configured
  // it! We only need to enable wake-up capability on the existing
  // configuration. Reconfiguring it here would break the touch controller's
  // interrupt handling.

  // Enable GPIO wakeup on touch (wake on LOW level - touch detected)
  // This adds wake-up capability without changing the existing GPIO
  // configuration
  esp_err_t ret = gpio_wakeup_enable(TOUCH_INT_GPIO, GPIO_INTR_LOW_LEVEL);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to enable touch GPIO wakeup: %s",
             esp_err_to_name(ret));
    return ret;
  }
#endif

  // Configure boot button GPIO as input with pull-up (button is active-low)
  gpio_config_t button_conf = {.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
                               .mode = GPIO_MODE_INPUT,
                               .pull_up_en = GPIO_PULLUP_ENABLE,
                               .pull_down_en = GPIO_PULLDOWN_DISABLE,
                               .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&button_conf);

  // Enable GPIO wakeup on boot button (wake on LOW level - button pressed)
  ret = gpio_wakeup_enable(BOOT_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to enable button GPIO wakeup: %s",
             esp_err_to_name(ret));
    return ret;
  }

  // Enable sleep wakeup from GPIO
  ret = esp_sleep_enable_gpio_wakeup();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to enable sleep GPIO wakeup: %s",
             esp_err_to_name(ret));
    return ret;
  }

#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
  ESP_LOGI(TAG, "GPIO wake-up enabled: GPIO%d (touch) + GPIO%d (button)",
           TOUCH_INT_GPIO, BOOT_BUTTON_GPIO);
#else
  ESP_LOGI(TAG,
           "GPIO wake-up enabled: GPIO%d (button only - touch disabled for "
           "battery saving)",
           BOOT_BUTTON_GPIO);
#endif
#else
  ESP_LOGI(TAG, "GPIO wake-up disabled");
#endif

  // Keep RTC peripherals powered during sleep (for RTC chip, timers)
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  // Initialize activity timer and state
  last_activity_time = esp_timer_get_time();
  last_user_activity_time = last_activity_time;
  is_sleeping = false;
  is_backlight_off = false;

  // Register global touch event handler on input device for backlight wake-up
  if (sleep_manager_lock_display_with_retry(200, 5, 50))
  {
    lv_display_t *disp = lv_display_get_default();
    if (disp)
    {
      // Get the first input device (touchscreen)
      lv_indev_t *indev = lv_indev_get_next(NULL);
      if (indev)
      {
        lv_indev_add_event_cb(indev, global_touch_event_handler,
                              LV_EVENT_PRESSED, NULL);
        lv_indev_add_event_cb(indev, global_touch_event_handler,
                              LV_EVENT_PRESSING, NULL);
        ESP_LOGI(TAG, "Global touch event handler registered on input device");
      }
      else
      {
        ESP_LOGW(TAG, "No input device found for event handler registration");
      }
    }
    bsp_display_unlock();
  }
  else
  {
    ESP_LOGW(TAG, "Failed to acquire display lock for touch handler registration");
  }

  // Create sleep monitoring task
  BaseType_t task_created = xTaskCreate(sleep_check_task, "sleep_check", 2048,
                                        NULL, 5, &sleep_task_handle);
  if (task_created != pdPASS)
  {
    ESP_LOGE(TAG, "Failed to create sleep monitoring task");
    return ESP_FAIL;
  }

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
  ESP_LOGI(TAG,
           "Sleep manager initialized (wake: touch+button, timeout: %d ms)",
           SLEEP_TIMEOUT_MS);
#else
  ESP_LOGI(TAG, "Sleep manager initialized (wake: button only, timeout: %d ms)",
           SLEEP_TIMEOUT_MS);
#endif
#else
  ESP_LOGI(TAG, "Sleep manager initialized (no wake sources, timeout: %d ms)",
           SLEEP_TIMEOUT_MS);
#endif

  return ESP_OK;
}

bool sleep_manager_get_last_sleep_type(sleep_manager_sleep_type_t *out_type)
{
  if (!out_type)
  {
    return false;
  }

  if (last_sleep_type == SLEEP_MANAGER_SLEEP_TYPE_NONE)
  {
    return false;
  }

  *out_type = last_sleep_type;
  last_sleep_type = SLEEP_MANAGER_SLEEP_TYPE_NONE;
  return true;
}

#ifdef CONFIG_SLEEP_MANAGER_DEEP_SLEEP_ENABLE
static void sleep_manager_enter_deep_sleep(void)
{
  ESP_LOGI(TAG, "Entering deep sleep mode...");

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
  log_power_state("deep_sleep_enter");
#endif

  last_sleep_type = SLEEP_MANAGER_SLEEP_TYPE_DEEP;

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
  ESP_LOGI(TAG,
           "Deep sleep (wake sources: touch GPIO%d, button GPIO%d)",
           TOUCH_INT_GPIO, BOOT_BUTTON_GPIO);
#else
  ESP_LOGI(TAG, "Deep sleep (wake sources: button GPIO%d)", BOOT_BUTTON_GPIO);
#endif
#else
  ESP_LOGI(TAG, "Deep sleep (wake sources: none)");
#endif

  esp_deep_sleep_start();
}
#endif

esp_err_t sleep_manager_sleep(void)
{
  if (is_sleeping)
  {
    ESP_LOGW(TAG, "Already in sleep mode");
    return ESP_OK;
  }

#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SLEEP_ON_USB
  if (sleep_manager_is_usb_connected())
  {
    SLEEP_LOGD(TAG, "USB connected - sleep aborted");
    return ESP_OK;
  }
#endif

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
  if (gpio_get_level(TOUCH_INT_GPIO) == 0)
  {
    SLEEP_LOGD(TAG, "Touch interrupt active - sleep aborted");
    sleep_manager_reset_timer();
    return ESP_OK;
  }
#endif
  if (gpio_get_level(BOOT_BUTTON_GPIO) == 0)
  {
    SLEEP_LOGD(TAG, "Button pressed - sleep aborted");
    sleep_manager_reset_timer();
    return ESP_OK;
  }
#endif

  ESP_LOGI(TAG, "Entering sleep mode...");

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
  log_power_state("sleep_enter");
#endif

  // Save uptime before sleep to prevent data loss
  esp_err_t uptime_ret = uptime_tracker_save();
  if (uptime_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to save uptime before sleep: %s", esp_err_to_name(uptime_ret));
  }
  else
  {
    ESP_LOGI(TAG, "Uptime saved before sleep");
  }

  // Lock LVGL before modifying timers
  if (!sleep_manager_lock_display_with_retry(200, 5, 50))
  {
    ESP_LOGW(TAG, "Failed to acquire display lock - sleep aborted");
    is_sleeping = false;
    return ESP_ERR_TIMEOUT;
  }

  lv_display_t *disp = lv_display_get_default();
  if (!disp)
  {
    ESP_LOGW(TAG, "No LVGL display - sleep aborted");
    bsp_display_unlock();
    is_sleeping = false;
    return ESP_ERR_INVALID_STATE;
  }

  // Pause all LVGL timers
  pause_lvgl_timers();

#ifdef CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL
  // Disable LVGL rendering (optional optimization)
  lv_display_enable_invalidation(disp, false);
  ESP_LOGI(TAG, "LVGL rendering disabled");
#endif

  bsp_display_unlock();

  // Mark as sleeping to avoid touch wake interference during entry
  is_sleeping = true;

  // Turn off display
  display_sleep();

#ifdef CONFIG_SLEEP_MANAGER_WIFI_SUSPEND
  sleep_manager_suspend_wifi();
#endif

  // Enter light sleep - BLOCKS until wake-up event
#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
  ESP_LOGI(TAG,
           "Entering light sleep (wake sources: touch GPIO%d, button GPIO%d)",
           TOUCH_INT_GPIO, BOOT_BUTTON_GPIO);
#else
  ESP_LOGI(TAG, "Entering light sleep (wake sources: button GPIO%d)",
           BOOT_BUTTON_GPIO);
#endif
#else
  ESP_LOGI(TAG, "Entering light sleep (wake sources: none)");
#endif

  last_sleep_type = SLEEP_MANAGER_SLEEP_TYPE_LIGHT;

  int64_t sleep_start = esp_timer_get_time();
  esp_err_t ret = esp_light_sleep_start();
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Light sleep failed: %s", esp_err_to_name(ret));
    sleep_manager_wake();
    return ret;
  }
  int64_t sleep_duration =
      (esp_timer_get_time() - sleep_start) / 1000; // Convert to ms

  // Check wake-up cause
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const char *cause_str = "unknown";

  switch (cause)
  {
  case ESP_SLEEP_WAKEUP_GPIO:
    cause_str = "gpio";
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    cause_str = "timer";
    break;
  case ESP_SLEEP_WAKEUP_UART:
    cause_str = "uart";
    break;
  case ESP_SLEEP_WAKEUP_EXT0:
    cause_str = "ext0";
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    cause_str = "ext1";
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    cause_str = "touchpad";
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    cause_str = "ulp";
    break;
  default:
    cause_str = "unknown";
    break;
  }

  ESP_LOGI(TAG, "Woke from light sleep after %lld ms (cause: %s)",
           sleep_duration, cause_str);

#ifdef CONFIG_SLEEP_MANAGER_GPIO_WAKEUP
  if (cause == ESP_SLEEP_WAKEUP_GPIO)
  {
    uint64_t wake_mask = esp_sleep_get_gpio_wakeup_status();
    if (wake_mask != 0)
    {
      if (wake_mask & (1ULL << BOOT_BUTTON_GPIO))
      {
        ESP_LOGI(TAG, "Wake source: boot button (GPIO%d)",
                 BOOT_BUTTON_GPIO);
      }
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_WAKEUP
      if (wake_mask & (1ULL << TOUCH_INT_GPIO))
      {
        ESP_LOGI(TAG, "Wake source: touch (GPIO%d)", TOUCH_INT_GPIO);
      }
#endif
    }
    else
    {
      ESP_LOGI(TAG, "Wake source: GPIO (unknown pin)");
    }
  }
#endif

  // Wake up display and timers
  sleep_manager_wake();

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
  log_power_state("sleep_exit");
#endif

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

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
  log_power_state("wake_start");
#endif

  // Wake up display
  display_wake();

  // Lock LVGL before modifying timers
  if (!sleep_manager_lock_display_with_retry(200, 5, 50))
  {
    ESP_LOGW(TAG, "Failed to acquire display lock - wake deferred");
    return ESP_ERR_TIMEOUT;
  }

  lv_display_t *disp = lv_display_get_default();
  if (!disp)
  {
    ESP_LOGW(TAG, "No LVGL display - wake aborted");
    bsp_display_unlock();
    return ESP_ERR_INVALID_STATE;
  }

#ifdef CONFIG_SLEEP_MANAGER_LVGL_RENDERING_CONTROL
  // Re-enable LVGL rendering
  lv_display_enable_invalidation(disp, true);
  ESP_LOGI(TAG, "LVGL rendering enabled");
#endif

  // Resume all LVGL timers
  resume_lvgl_timers();

  bsp_display_unlock();

  // Reset light sleep activity timer
  last_activity_time = esp_timer_get_time();

  // Mark as awake
  is_sleeping = false;

#ifdef CONFIG_SLEEP_MANAGER_WIFI_SUSPEND
  sleep_manager_resume_wifi();
#endif

  ESP_LOGI(TAG, "Wake complete");

#ifdef CONFIG_SLEEP_MANAGER_POWER_LOGS
  log_power_state("wake_complete");
#endif

  return ESP_OK;
}

bool sleep_manager_is_usb_connected(void)
{
#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SLEEP_ON_USB
  bool vbus_present = false;
  esp_err_t ret = axp2101_is_vbus_present(&vbus_present);

  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to read VBUS status: %s", esp_err_to_name(ret));
    return false; // Assume not connected on error
  }

  return vbus_present;
#else
  return false;
#endif
}

bool sleep_manager_should_sleep(void)
{
  if (is_sleeping)
  {
    return false; // Already sleeping
  }

#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SLEEP_ON_USB
  // Don't sleep if USB/JTAG is connected (for development/debugging)
  if (sleep_manager_is_usb_connected())
  {
    SLEEP_LOGD(TAG, "USB connected - sleep prevented");
    return false;
  }
#endif

  uint32_t inactive_ms = sleep_manager_get_inactive_time();
  return (inactive_ms >= SLEEP_TIMEOUT_MS);
}

void sleep_manager_reset_timer(void)
{
#ifdef CONFIG_SLEEP_MANAGER_TOUCH_RESET_TIMER
  int64_t now = esp_timer_get_time();
  last_activity_time = now;
  last_user_activity_time = now;
  SLEEP_LOGD(TAG, "Activity timer reset");
#endif
}

uint32_t sleep_manager_get_inactive_time(void)
{
  int64_t now = esp_timer_get_time();
  int64_t elapsed_us = now - last_activity_time;
  return (uint32_t)(elapsed_us / 1000); // Convert to milliseconds
}

bool sleep_manager_should_turn_off_backlight(void)
{
  if (is_backlight_off)
  {
    return false; // Already off
  }

#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SCREEN_OFF_ON_USB
  // Don't turn off backlight if USB/JTAG is connected (for
  // development/debugging)
  if (sleep_manager_is_usb_connected())
  {
    SLEEP_LOGD(TAG, "USB connected - backlight off prevented");
    return false;
  }
#endif

  uint32_t inactive_ms = sleep_manager_get_inactive_time();
  return (inactive_ms >= BACKLIGHT_TIMEOUT_MS);
}

esp_err_t sleep_manager_backlight_off(void)
{
  if (is_backlight_off)
  {
    SLEEP_LOGD(TAG, "Backlight already off");
    return ESP_OK;
  }

#ifdef CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL
#ifdef CONFIG_SLEEP_MANAGER_PREVENT_SCREEN_OFF_ON_USB
  // Don't turn off backlight if USB/JTAG is connected
  if (sleep_manager_is_usb_connected())
  {
    SLEEP_LOGD(TAG, "USB connected - backlight off prevented");
    return ESP_OK;
  }
#endif

  bsp_display_backlight_off();
  is_backlight_off = true;
  ESP_LOGI(TAG, "Backlight turned off");
#else
  ESP_LOGI(TAG, "Backlight control disabled");
#endif

  return ESP_OK;
}

esp_err_t sleep_manager_backlight_on(void)
{
  if (!is_backlight_off)
  {
    SLEEP_LOGD(TAG, "Backlight already on");
    return ESP_OK;
  }

#ifdef CONFIG_SLEEP_MANAGER_BACKLIGHT_CONTROL
  bsp_display_backlight_on();
  is_backlight_off = false;
  ESP_LOGI(TAG, "Backlight turned on");

  // Reset activity timer when turning on backlight
  sleep_manager_reset_timer();
#else
  ESP_LOGI(TAG, "Backlight control disabled");
#endif

  return ESP_OK;
}

bool sleep_manager_is_backlight_off(void) { return is_backlight_off; }

#endif // CONFIG_SLEEP_MANAGER_ENABLE
