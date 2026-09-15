# Cursor math skill

Use this skill when editing projection, calibration, or smoothing logic.

## Goal

Keep cursor math deterministic, testable, and portable from TypeScript to future embedded C.

## Workflow

1. Treat the calibrated frame as right-handed: +X right across the screen, +Y up, +Z from the nose toward the monitor.
2. Keep monitor intersection math in framework-independent modules under `simulator/src/math/`.
3. Make sign conventions explicit: positive yaw should move right, positive pitch should move up.
4. Preserve the distinction between raw intersection output and smoothed cursor output.
5. Add focused Vitest cases for neutral pose, rightward yaw, upward pitch, and invalid backward-facing rays when math changes.

## Done when

- The math code can be consumed without React.
- Core pose-to-screen behavior is covered by unit tests.
- Coordinate-frame expectations remain documented in `docs/coordinate-frames.md`.
