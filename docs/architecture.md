# Architecture

`imu_sandbox` is a small monorepo with two intentionally separated workstreams:

- `firmware/blinky/` is a Zephyr workspace application for the Seeed Studio XIAO nRF52840 Sense.
- `simulator/` is a web reference implementation of the cursor-projection math using React, Vite, Three.js, React Three Fiber, and Drei.

The simulator is the behavioral reference. Projection, smoothing, and calibration logic live in framework-independent TypeScript modules so the same conventions can later be reimplemented in embedded C without first untangling UI code.

The firmware subtree is deliberately minimal at this stage. It establishes a buildable Zephyr target and repository shape without pulling in IMU, BLE HID, or machine-learning dependencies.
