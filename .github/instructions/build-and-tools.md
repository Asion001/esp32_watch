---
applyTo: "**/*"
---

# Build & Tooling (ESP-IDF)

- Run export.bat (Windows) or export.sh (Linux/macOS) from the ESP-IDF SDK before any build, flash, or idf.py usage.
- Prefer the VS Code ESP-IDF extension for build/flash/monitor.
- idf.py commands are allowed only after the export script has been run.
- Avoid non-ESP-IDF build tools or ad-hoc shell build commands.
- After any change, perform a build to verify compilation.
- Use ESP-IDF clean/fullclean when sdkconfig or toolchain settings change.
- Makefile shortcuts are acceptable only after running export.{bat,sh}.
