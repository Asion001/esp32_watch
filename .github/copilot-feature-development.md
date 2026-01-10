# Feature Development Guidelines

## Overview

All new features in the ESP32-C6 Smartwatch firmware must be **configurable and toggleable** through `idf.py menuconfig`. This ensures flexibility, easier testing, and the ability to disable features for power optimization or memory constraints.

## Step-by-Step Feature Development Process

### 1. Create Feature Component

Create a new component in the `components/` directory:

```bash
mkdir -p components/my_feature
cd components/my_feature
touch my_feature.c my_feature.h CMakeLists.txt Kconfig
```

### 2. Add Kconfig Configuration

Create `components/my_feature/Kconfig` with feature toggles and **"App:" prefix**:

```kconfig
menu "App: My Feature"

config MY_FEATURE_ENABLE
    bool "Enable My Feature"
    default y
    help
        Enable or disable My Feature functionality.
        When disabled, the feature code is not compiled.

config MY_FEATURE_OPTION_1
    int "Feature Option 1"
    depends on MY_FEATURE_ENABLE
    default 100
    range 10 1000
    help
        Configure a numeric option for the feature.

config MY_FEATURE_DEBUG_LOGS
    bool "Enable Debug Logs"
    depends on MY_FEATURE_ENABLE
    default n
    help
        Enable verbose debug logging for My Feature.

endmenu
```

**Important**: Always use **"App:"** prefix in the menu name. This groups all application features together in menuconfig under Component config section, making them easy to find.

**Key Points:**

- Main enable/disable toggle with `bool` type
- Use `depends on` for sub-options
- Provide `default` values
- Use `range` for bounded numeric values
- Add descriptive `help` text

### 3. Implement Feature with Conditional Compilation

In `components/my_feature/my_feature.c`:

```c
#include "my_feature.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "MyFeature";

#ifdef CONFIG_MY_FEATURE_ENABLE

esp_err_t my_feature_init(void)
{
    ESP_LOGI(TAG, "Initializing My Feature");

#ifdef CONFIG_MY_FEATURE_DEBUG_LOGS
    ESP_LOGD(TAG, "Debug mode enabled");
#endif

    // Feature initialization code

    return ESP_OK;
}

void my_feature_task(void)
{
    // Feature implementation
}

#else // CONFIG_MY_FEATURE_ENABLE not defined

// Provide stub implementations when feature is disabled
esp_err_t my_feature_init(void)
{
    return ESP_OK; // No-op
}

void my_feature_task(void)
{
    // No-op
}

#endif // CONFIG_MY_FEATURE_ENABLE
```

In `components/my_feature/my_feature.h`:

```c
#ifndef MY_FEATURE_H
#define MY_FEATURE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize My Feature
 * @return ESP_OK on success
 */
esp_err_t my_feature_init(void);

/**
 * @brief Run feature task
 */
void my_feature_task(void);

#ifdef __cplusplus
}
#endif

#endif // MY_FEATURE_H
```

### 4. Configure CMakeLists.txt

In `components/my_feature/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "my_feature.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer  # Add dependencies here
)
```

### 5. Integrate into Main Application

In `main/main.c` or relevant app file:

```c
#include "my_feature.h"

void app_main(void)
{
    // ... other initialization ...

#ifdef CONFIG_MY_FEATURE_ENABLE
    esp_err_t ret = my_feature_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize My Feature: %s", esp_err_to_name(ret));
    }
#endif

    // ... rest of application ...
}
```

### 6. Configure Feature via Menuconfig

```bash
idf.py menuconfig
```

Navigate to your feature's menu and configure options. Settings are saved to `sdkconfig`.

### 7. Build and Test

**CRITICAL**: Always build after making changes:

```bash
idf.py build
```

This checks for:

- Compilation errors
- Configuration conflicts
- Missing dependencies
- Syntax errors

If successful, flash and test:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

### 8. Test Both Enabled and Disabled States

**Important**: Test your feature in both configurations:

```bash
# Test with feature enabled
idf.py menuconfig  # Enable feature
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# Test with feature disabled
idf.py menuconfig  # Disable feature
idf.py build        # Must still compile!
idf.py -p /dev/ttyUSB0 flash monitor
```

### 9. Update Configuration Files

After implementing your feature, update the appropriate config files:

**For default configuration:**

```bash
# Configure features in menuconfig
idf.py menuconfig

# Save current config as defaults
idf.py save-defconfig

# This updates sdkconfig.defaults with minimal changes
```

**For CI test configurations:**

- `sdkconfig.all-features` - Add your feature with `CONFIG_MY_FEATURE_ENABLE=y`
- `sdkconfig.minimal` - Add your feature with `CONFIG_MY_FEATURE_ENABLE=n`

These files ensure CI tests your feature in both enabled and disabled states.

## Best Practices

### Configuration Naming

- Use descriptive prefixes: `CONFIG_<COMPONENT>_<FEATURE>_<OPTION>`
- Example: `CONFIG_SLEEP_MANAGER_ENABLE`, `CONFIG_SLEEP_MANAGER_TIMEOUT_MS`

### Feature Design

1. **Always provide stub implementations** when feature is disabled
2. **Check config in header guards** for conditional API exposure
3. **Use `depends on` in Kconfig** to hide sub-options when feature is disabled
4. **Provide sensible defaults** that work out of the box

### Conditional Compilation Patterns

```c
// Pattern 1: Function-level conditionals
#ifdef CONFIG_MY_FEATURE_ENABLE
void feature_function(void) {
    // Implementation
}
#else
void feature_function(void) {
    // Stub/no-op
}
#endif

// Pattern 2: Code block conditionals
void some_function(void) {
#ifdef CONFIG_MY_FEATURE_ENABLE
    feature_specific_code();
#endif
    common_code();
}

// Pattern 3: Compile-time configuration values
#define BUFFER_SIZE CONFIG_MY_FEATURE_BUFFER_SIZE
```

### Error Handling

Always handle initialization failures gracefully:

```c
esp_err_t ret = my_feature_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Feature init failed: %s", esp_err_to_name(ret));
    // Continue with reduced functionality, don't crash
}
```

## Verification Checklist

Before committing new features:

- [ ] Feature compiles with option **enabled**
- [ ] Feature compiles with option **disabled**
- [ ] Kconfig has descriptive help text
- [ ] Default values are sensible
- [ ] Dependencies are declared in CMakeLists.txt
- [ ] Debug logs use `CONFIG_*_DEBUG_LOGS` guards
- [ ] `idf.py build` completes successfully
- [ ] Tested on actual hardware
- [ ] `sdkconfig.defaults` updated if needed
- [ ] CI/CD pipeline passes

## Examples in Codebase

See existing components for reference:

- **Sleep Manager**: `components/sleep_manager/` - Comprehensive sleep feature with multiple options
- **Uptime Tracker**: `components/uptime_tracker/` - Simple enableable component
- **RTC Driver**: `components/pcf85063_rtc/` - Hardware driver with I2C dependency

## Common Pitfalls

### ❌ Don't: Hard-code feature availability

```c
// BAD - always compiled
my_feature_init();
```

### ✅ Do: Use conditional compilation

```c
// GOOD - conditionally compiled
#ifdef CONFIG_MY_FEATURE_ENABLE
my_feature_init();
#endif
```

### ❌ Don't: Forget stub implementations

```c
// BAD - linker errors when disabled
#ifdef CONFIG_MY_FEATURE_ENABLE
void my_function(void) { /* impl */ }
#endif
```

### ✅ Do: Provide stubs for disabled features

```c
// GOOD - always compiles
#ifdef CONFIG_MY_FEATURE_ENABLE
void my_function(void) { /* impl */ }
#else
void my_function(void) { /* no-op */ }
#endif
```

### ❌ Don't: Use raw config values without defaults

```c
// BAD - undefined if config missing
int timeout = CONFIG_MY_TIMEOUT;
```

### ✅ Do: Provide fallback defaults

```c
// GOOD - safe fallback
#ifndef CONFIG_MY_TIMEOUT
#define CONFIG_MY_TIMEOUT 5000
#endif
int timeout = CONFIG_MY_TIMEOUT;
```

## CI/CD Integration

All features are automatically tested in the CI/CD pipeline with:

- All features enabled (maximum functionality)
- All features disabled (minimal build)
- Mixed configurations (common use cases)

See `.github/workflows/build.yml` for details.
