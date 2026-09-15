# Hardware scope

Current hardware scope is intentionally narrow:

- Board: Seeed Studio XIAO nRF52840 Sense
- Firmware target: `xiao_ble/nrf52840/sense`
- Firmware behavior: plain Zephyr LED blink sample

Not included in this scaffold:

- TinyML or other ML libraries
- sensor fusion
- BLE HID mouse support
- wearable mechanical design files

This keeps the repository ready for later IMU and HID work while preserving a minimal, buildable embedded baseline.
