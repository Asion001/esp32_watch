---
applyTo: "**/*"
---
# Feature Flags & Settings

- Every feature must be menuconfig-toggleable via Kconfig.
- Guard feature code with #ifdef CONFIG_*.
- Provide no-op stubs when a feature is disabled.
- Test both enabled and disabled configurations.
- Settings screen titles must start with "App: ".
