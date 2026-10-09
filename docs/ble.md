# BLE pose stream

The `firmware/imu_mouse` application keeps its existing BLE HID mouse and adds a
custom GATT pose stream. The stream uses the existing LSM6DSL/LSM6DS3TR-C
acquisition and fusion pipeline; it does not estimate absolute position.

## GATT protocol

All UUIDs use the Bluetooth canonical string form in the simulator. The byte
arrays in `firmware/imu_mouse/src/imu_ble.h` use Bluetooth's little-endian
128-bit UUID representation and correspond to these values.

| GATT item | UUID | Properties |
| --- | --- | --- |
| IMU Pose service | `7e400001-b5a3-f393-e0a9-e50e24dcca9e` | Primary service |
| Pose characteristic | `7e400002-b5a3-f393-e0a9-e50e24dcca9e` | Notify |
| Control/Info characteristic | `7e400003-b5a3-f393-e0a9-e50e24dcca9e` | Read |

The Pose characteristic is a fixed 22-byte frame. Multi-byte fields are
little-endian; offsets are from the start of the value.

| Offset | Size | Type | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8` | Protocol version (`1`) |
| 1 | 1 | `uint8` | Flags: bit 0 orientation valid, bit 1 physical button pressed; other bits reserved |
| 2 | 2 | `uint16` | Wrapping sequence number |
| 4 | 4 | `uint32` | Device uptime in milliseconds |
| 8 | 2 | `int16` | Quaternion `qw`, Q14 (divide by 16384) |
| 10 | 2 | `int16` | Quaternion `qx`, Q14 |
| 12 | 2 | `int16` | Quaternion `qy`, Q14 |
| 14 | 2 | `int16` | Quaternion `qz`, Q14 |
| 16 | 2 | `int16` | Gyro X, 0.01 degree/s per count |
| 18 | 2 | `int16` | Gyro Y, 0.01 degree/s per count |
| 20 | 2 | `int16` | Gyro Z, 0.01 degree/s per count |

The quaternion is the fusion filter's body-to-world orientation. Gyro values
are bias-corrected angular velocity. Firmware clamps fixed-point values to the
`int16` range. The Control/Info value is four bytes: protocol version, sample
rate in Hz (`uint16` little-endian), and mode (`0` fused sensor, `1` synthetic).
The default notification rate is 50 Hz and can be set from 1–100 Hz with
`CONFIG_IMU_BLE_SAMPLE_RATE_HZ`. Set `CONFIG_IMU_BLE_SYNTHETIC_DATA=y` to stream
a synthetic rotation over BLE while leaving the existing mouse path on sensor
fusion.

## Build and flash

In a configured Zephyr/nRF Connect SDK workspace with this repository available
as the application:

```sh
west build -s firmware/imu_mouse -b xiao_ble/nrf52840/sense
west flash
```

The device advertises as **IMU Sandbox**. Its existing HIDS peripheral owns
advertising, connection callbacks, and re-advertising; the custom service UUID
is added to the advertising payload. Only one BLE connection is configured,
so disconnect the mouse from another host before selecting it in the browser.

## Connect from the simulator

Run the simulator from the repository:

```sh
cd simulator
npm ci
npm run dev
```

Open the displayed localhost URL in Chrome or Edge, choose **Connect BLE
device**, and select **IMU Sandbox** in the browser's device chooser. Select
**Live BLE pose** as the input source to drive the scene and cursor projection
from its quaternion. The simulator shows connection status, packet rate, and
the number of missing sequence numbers. Use **Simulated controls** to return to
the existing sliders.

Web Bluetooth requires a supported browser and a secure context: HTTPS or
`localhost`. Other browsers may not expose the Bluetooth API. The browser
prompts for device access and may show the operating system's Bluetooth pairing
dialog. If connection fails, close other apps or hosts connected to the mouse,
keep the device nearby, and retry from the browser. If pairing confirmation is
requested, follow the operating system prompt and confirm on the device as
directed by its existing HID pairing flow.
