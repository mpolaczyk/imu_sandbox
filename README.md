# imu_sandbox

`imu_sandbox` is a small monorepo for experimenting with a head-pose cursor controller.
It currently contains a minimal Zephyr blinky application for the Seeed Studio XIAO nRF52840 Sense and a web-based cursor math simulator.

## Repository layout

```text
.github/
  copilot-instructions.md
  skills/
  workflows/
firmware/
  blinky/
simulator/
docs/
west.yml
```

## Contents

- **Firmware:** Zephyr C blinky sample targeting `xiao_ble/nrf52840/sense`, with no ML dependencies.
- **Simulator:** React + TypeScript + Vite + Three.js visual reference for head-origin ray projection, monitor intersection, diagnostics, and cursor smoothing.
- **Copilot guidance:** focused repository instructions and concrete skills for firmware, cursor math, and simulator work.
- **CI:** dedicated workflows for simulator validation and Zephyr firmware builds.

## Quick start

### Simulator

```bash
cd simulator
npm install
npm run dev
```

### Simulator checks

```bash
cd simulator
npm run test
npm run build
npm run lint
```

### Zephyr firmware

```bash
mkdir -p ~/zephyr-workspaces/imu_sandbox
cd ~/zephyr-workspaces/imu_sandbox
git clone git@github.com:mpolaczyk/imu_sandbox.git app
python3 -m venv .venv
source .venv/bin/activate
pip install west
west init -l app
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
west build -s app/firmware/blinky -b xiao_ble/nrf52840/sense
```

## Documentation

- `docs/architecture.md`
- `docs/coordinate-frames.md`
- `docs/calibration.md`
- `docs/hardware.md`
- `docs/development.md`

## Hardware scope

The repository intentionally stops at a Zephyr blinky baseline on hardware and a browser-based math simulator on the web side. TinyML functionality is intentionally excluded from this initial scaffold.
