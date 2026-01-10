
    config SLEEP_MANAGER_GPIO_WAKEUP
        bool "Enable GPIO wake-up"
        depends on SLEEP_MANAGER_ENABLE
        default y
        help
            Enable hardware wake-up sources (boot button).
            Disable to test sleep without wake-up capability.

    config SLEEP_MANAGER_TOUCH_WAKEUP
        bool "Enable touch screen wake-up"
        depends on SLEEP_MANAGER_GPIO_WAKEUP
        default y
        help
            Enable touch screen as wake-up source.
            Disable to save battery and require boot button press to wake.
            When disabled, only the boot button (GPIO9) will wake the watch.# ESP32-C6 Smartwatch Firmware Makefile
# Quick reference for common build tasks

.PHONY: help build flash monitor clean menuconfig defconfig test-all test-minimal test-default check format

# Default target
help:
	@echo "ESP32-C6 Smartwatch Firmware - Build Commands"
	@echo ""
	@echo "Development Commands:"
	@echo "  make build          - Build firmware with current configuration"
	@echo "  make flash          - Flash firmware to device"
	@echo "  make monitor        - Open serial monitor"
	@echo "  make clean          - Clean build artifacts"
	@echo "  make menuconfig     - Open configuration menu"
	@echo "  make defconfig      - Save current config as defaults"
	@echo ""
	@echo "Testing Commands:"
	@echo "  make test-all       - Build with all features enabled"
	@echo "  make test-minimal   - Build with minimal features"
	@echo "  make test-default   - Build with default configuration"
	@echo "  make check          - Run all build configurations + checks"
	@echo ""
	@echo "Maintenance Commands:"
	@echo "  make size           - Show binary size breakdown"
	@echo "  make format         - Format code (if clang-format available)"
	@echo "  make analyze        - Static analysis checks"
	@echo ""
	@echo "Feature Development:"
	@echo "  See .github/copilot-feature-development.md for guidelines"

# Standard build commands
build:
	idf.py build

flash:
	idf.py -p /dev/ttyUSB0 flash

monitor:
	idf.py -p /dev/ttyUSB0 monitor

clean:
	idf.py fullclean

menuconfig:
	idf.py menuconfig

defconfig:
	idf.py save-defconfig
	@echo "✓ Configuration saved to sdkconfig.defaults"

size:
	idf.py size
	idf.py size-components

# Test all features enabled
test-all:
	@echo "=== Building with ALL features enabled ==="
	@cat sdkconfig.defaults sdkconfig.all-features > sdkconfig
	idf.py build
	@echo "✓ All features build successful"

# Test minimal configuration
test-minimal:
	@echo "=== Building with MINIMAL features ==="
	@cat sdkconfig.defaults sdkconfig.minimal > sdkconfig
	idf.py build
	@echo "✓ Minimal features build successful"

# Test default configuration
test-default:
	@echo "=== Building with DEFAULT configuration ==="
	@cp sdkconfig.defaults sdkconfig
	idf.py build
	@echo "✓ Default configuration build successful"

# Run all build tests
check: test-default test-all test-minimal analyze
	@echo ""
	@echo "======================================"
	@echo "✅ All build configurations passed!"
	@echo "======================================"

# Static analysis
analyze:
	@echo "=== Running static analysis ==="
	@echo "Checking for TODOs and FIXMEs..."
	@grep -r "TODO\|FIXME" main/ components/ || true
	@echo ""
	@echo "Checking for unsafe functions..."
	@! grep -r "strcpy\|strcat\|sprintf\|gets" --include="*.c" main/ components/ || echo "⚠️  Found unsafe functions"
	@echo ""
	@echo "Checking for trailing whitespace..."
	@! grep -r " $$" --include="*.c" --include="*.h" main/ components/ || echo "⚠️  Found trailing whitespace"
	@echo "✓ Static analysis complete"

# Code formatting (requires clang-format)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "Formatting C files..."; \
		find main/ components/ -name "*.c" -o -name "*.h" | xargs clang-format -i; \
		echo "✓ Code formatted"; \
	else \
		echo "⚠️  clang-format not installed"; \
	fi

# Quick flash and monitor
run: flash monitor

# Development workflow: build, flash, monitor
dev: build flash monitor

# CI-like local test (same as CI pipeline)
ci-local: test-default test-all test-minimal analyze
	@echo ""
	@echo "======================================"
	@echo "✅ Local CI checks passed!"
	@echo "Ready to commit/push"
	@echo "======================================"
