# Firmware

The firmware subtree currently contains a minimal Zephyr LED blink sample for the Seeed Studio XIAO nRF52840 Sense.
It intentionally avoids IMU fusion, BLE HID, or TinyML so the repository can establish a clean embedded baseline.

## Target

- Board: `xiao_ble/nrf52840/sense`

## Build from a workspace initialized with this repository's `west.yml`

```bash
west build -s firmware/blinky -b xiao_ble/nrf52840/sense
```
