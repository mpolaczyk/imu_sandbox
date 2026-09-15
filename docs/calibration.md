# Calibration

The initial simulator implements a practical neutral-pose calibration rather than a full sensor alignment workflow.

## Neutral pose

When **Calibrate neutral pose** is pressed, the current yaw, pitch, and roll slider values are stored as offsets. Subsequent motion is measured relative to that pose:

- effective yaw = current yaw - stored yaw offset
- effective pitch = current pitch - stored pitch offset
- effective roll = current roll - stored roll offset

This matches the expected recenter behavior for a head-driven pointer prototype: the user can adopt a comfortable forward-looking posture, calibrate once, and continue from there.

## Reset

**Reset** restores:

- yaw / pitch / roll to `0°`
- monitor distance to `0.60 m`
- smoothing alpha to `0.22`
- stored calibration offsets to `0°`

## Scope

Mount offsets, IMU drift simulation, and sensor-noise injection are intentionally out of scope for this initial scaffold.
