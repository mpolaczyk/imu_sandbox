# Copilot instructions for `imu_sandbox`

## Repository map

- `firmware/blinky/` is a minimal Zephyr C app for `xiao_ble/nrf52840/sense`.
- `simulator/` is the TypeScript/React reference implementation for cursor projection and visualization.
- `docs/` is the source of truth for architecture, frames, calibration, hardware scope, and local setup.

## Required constraints

- Preserve the MIT license and repository initialization files.
- Do not add TinyML or other ML dependencies unless a task explicitly requires them.
- Keep simulator math in framework-independent TypeScript modules under `simulator/src/math/`.
- Prefer surgical changes that keep firmware and simulator concerns separate.

## Firmware workflow

1. Treat `firmware/blinky/src/main.c` as the minimal Zephyr LED sample.
2. Keep board targeting pinned to `xiao_ble/nrf52840/sense` unless the task says otherwise.
3. When changing firmware behavior, update local build instructions or CI only if the change affects them.

## Simulator workflow

1. Put projection, quaternion, filtering, and calibration math in `simulator/src/math/` or `simulator/src/simulation/`.
2. Keep React components focused on visualization and controls.
3. Add or update Vitest coverage for cursor projection behavior whenever math changes.

## Validation

- For simulator changes, run the narrowest useful combination of `npm run test`, `npm run build`, and `npm run lint` in `simulator/`.
- For firmware changes, prefer `west build -s firmware/blinky -b xiao_ble/nrf52840/sense` when a Zephyr workspace is available.
