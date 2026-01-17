---
applyTo: "**/*.{c,h,cpp}"
---

# Embedded C/C++ Best Practices

## Memory & Performance

- Prefer stack allocation for small, fixed-size data.
- Use heap sparingly; check all allocation results.
- Mark read-only data with const to keep it in flash.
- Avoid busy loops; use FreeRTOS delays.
- Batch I2C reads and cache infrequent data.
- Avoid floating-point when not required.

## Error Handling

- Use ESP_ERROR_CHECK for critical calls.
- Log errors and degrade gracefully if peripherals fail.

## Code Style

- Validate pointers before use.
- Use fixed-width types (uint8_t, int32_t).
- Use packed structs for hardware registers.
- Zero-initialize structs before use.

## FreeRTOS Patterns

- UI tasks should have higher priority than background tasks.
- Protect shared state with mutexes.
- Keep queues small (8–16 items).
- Start with 2048–4096 stack bytes; tune with uxTaskGetStackHighWaterMark().
