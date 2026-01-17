# Configuration Files Reference

This project uses multiple ESP-IDF configuration files for different build scenarios.

## Configuration Files

### `sdkconfig.defaults`

**Purpose**: Default configuration for development and production builds  
**Usage**: Base configuration used by `idf.py build`  
**Contains**:

- Essential hardware settings
- LVGL configuration
- Core feature defaults
- Board-specific settings

**When to modify**:

- Setting default feature states
- Changing hardware parameters
- Updating board configuration
- Running `idf.py save-defconfig`

### `sdkconfig.all-features`

**Purpose**: CI testing with maximum features enabled  
**Usage**: Used by CI "build-all-features" job and `make test-all`  
**Contains**: All optional features set to `=y`

**When to modify**: When adding a new optional feature, add its enable flag here

Example:

```ini
CONFIG_MY_NEW_FEATURE_ENABLE=y
CONFIG_MY_NEW_FEATURE_OPTION=100
```

### `sdkconfig.minimal`

**Purpose**: CI testing with minimal/no optional features  
**Usage**: Used by CI "build-minimal" job and `make test-minimal`  
**Contains**: Optional features set to `=n`

**When to modify**: When adding a new optional feature, add its disable flag here

Example:

```ini
CONFIG_MY_NEW_FEATURE_ENABLE=n
```

### `sdkconfig` (generated)

**Purpose**: Active build configuration  
**Usage**: Generated during build, do NOT commit to git  
**Contains**: Complete merged configuration from defaults + menuconfig changes

**Note**: This file is in `.gitignore`

### `sdkconfig.old` (generated)

**Purpose**: Backup of previous configuration  
**Usage**: Automatic backup created by ESP-IDF when config changes  
**Note**: This file is in `.gitignore`

## Configuration Flow

```
Development Build:
sdkconfig.defaults → [idf.py build] → sdkconfig → build/

CI All Features:
sdkconfig.defaults + sdkconfig.all-features → sdkconfig → build/

CI Minimal:
sdkconfig.defaults + sdkconfig.minimal → sdkconfig → build/
```

## Adding a New Feature

When adding a new configurable feature:

1. **Create Kconfig** in component directory
2. **Test with menuconfig**

   ```bash
   idf.py menuconfig  # Enable feature
   idf.py build       # Test enabled

   idf.py menuconfig  # Disable feature
   idf.py build       # Test disabled
   ```

3. **Update default config**

   ```bash
   idf.py save-defconfig  # Saves to sdkconfig.defaults
   ```

4. **Update CI configs**

   - Add `CONFIG_MY_FEATURE_ENABLE=y` to `sdkconfig.all-features`
   - Add `CONFIG_MY_FEATURE_ENABLE=n` to `sdkconfig.minimal`

5. **Test CI locally**
   ```bash
   make test-all      # Uses sdkconfig.all-features
   make test-minimal  # Uses sdkconfig.minimal
   make check         # Tests all configs
   ```

## Configuration Inheritance

The project uses layered configuration:

```
Layer 1 (Base):         sdkconfig.defaults
                        ↓
Layer 2 (Overrides):    sdkconfig.all-features OR sdkconfig.minimal
                        ↓
Layer 3 (Generated):    sdkconfig
```

Later layers override earlier layers for the same config option.

## Best Practices

1. **Keep defaults minimal** - Only essential settings in `sdkconfig.defaults`
2. **Document changes** - Add comments in config files explaining non-obvious settings
3. **Test both states** - Always verify feature works when enabled AND disabled
4. **Use Kconfig dependencies** - Let Kconfig hide irrelevant options with `depends on`
5. **Version control** - Commit `sdkconfig.defaults`, `sdkconfig.all-features`, `sdkconfig.minimal`

## Troubleshooting

**Config mismatch errors:**

```bash
idf.py fullclean  # Clean everything
idf.py build      # Rebuild from scratch
```

**Testing specific config:**

```bash
# Temporarily use a specific config
cat sdkconfig.defaults sdkconfig.all-features > sdkconfig
idf.py build

# Restore default
cp sdkconfig.defaults sdkconfig
```

**See effective config:**

```bash
idf.py menuconfig  # Shows current active config
# Or check build/config/sdkconfig.h
```
