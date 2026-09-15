# Zephyr firmware skill

Use this skill when working in `firmware/` or CI steps that build Zephyr apps.

## Goal

Keep firmware changes aligned with a minimal Zephyr workspace application for `xiao_ble/nrf52840/sense`.

## Workflow

1. Confirm the requested board target remains `xiao_ble/nrf52840/sense`.
2. Keep application sources in standard Zephyr layout: `CMakeLists.txt`, `prj.conf`, `src/main.c`.
3. Prefer device-tree aliases such as `DT_ALIAS(led0)` over hard-coded GPIO values.
4. If build steps change, mirror them in `docs/development.md`, `README.md`, and `.github/workflows/firmware-ci.yml`.
5. When validating locally, use `west build -s firmware/blinky -b xiao_ble/nrf52840/sense` from a workspace initialized with this repo's `west.yml`.

## Done when

- The app remains a standard Zephyr application.
- Documentation and CI use the same board target and build path.
- No unrelated peripherals, ML stacks, or extra dependencies were introduced.
