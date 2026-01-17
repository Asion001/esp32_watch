---
applyTo: "main/apps/**"
---

# UI & LVGL Patterns

- Prefer lv_timer_create() for periodic UI updates.
- Timer callbacks run on the LVGL thread and do not require display locking.
- If updating LVGL from tasks or ISRs, lock the display first.
- Keep timer callbacks fast; avoid blocking work.

References:

- docs/WATCHFACE_LAYOUT.md
- docs/SETTINGS_IMPLEMENTATION.md
