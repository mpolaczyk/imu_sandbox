# Simulator math

Data flow from a BLE pose frame to the head-coupled 3D view and the cursor
projection. Module names refer to `simulator/src/`.

```mermaid
flowchart TD
    BLE["BLE pose frame (20 B)<br/>ble/ble-client.ts<br/>quaternion Q14, accel 1 mg, uptime ms"]
    BLE --> PARSE["parsePoseFrame<br/>normalise quaternion q<br/>accel mg to m/s²"]

    PARSE --> Q["q: body-to-world sensor orientation"]
    PARSE --> A["a_body: specific force (gravity included)"]
    PARSE --> T["uptime ms to dt (reject dt ≤ 0 or > 0.1 s, else 20 ms)"]

    Q --> REL["relativeImuOrientation (math/orientation.ts)<br/>q_rel = q_ref⁻¹ · q<br/>q_scene = S · q_rel · S⁻¹<br/>S = sensor-to-scene axis remap"]
    REF(["Calibrate / Reset<br/>q_ref = current q"]) -.-> REL

    subgraph TRACK ["PositionTracker (math/positionTracker.ts)"]
        direction TB
        W["a_world = q · a_body"]
        G["Warm-up: average a_world at rest to get g_world"]
        L["a_lin = a_world − g_world"]
        Z{"|a_lin| < deadband for N samples?"}
        MOVE["v += a_lin·dt<br/>v *= (1 − damping·dt)<br/>p += v·dt"]
        STILL["v = 0<br/>p *= (1 − dt / τ), pull to origin"]
        CLAMP["clamp each axis to ±maxOffset"]
        W --> G --> L --> Z
        Z -- no --> MOVE --> CLAMP
        Z -- yes --> STILL --> CLAMP
    end
    Q --> W
    A --> W
    T --> MOVE
    T --> STILL
    REF -.->|"reset"| G

    CLAMP --> TR["relativeImuTranslation<br/>d_scene = S · (q_ref⁻¹ · p_world)"]
    REF -.-> TR

    REL --> FWD["forwardVector<br/>f = q_scene · (0, 0, −1)"]
    FWD --> HIT["intersectNoseRayWithScreen (math/headPointer.ts)<br/>t = n·(c − o) / (n·f); hit = o + t·f<br/>u = x/width + 0.5, v = 0.5 − y/height"]
    HIT --> EMA["ema smoothing (math/filters.ts)"]
    EMA --> CURSOR["Cursor preview + 3D scene markers"]

    REL --> EYE["eyePosition (math/headCamera.ts)<br/>eye = q_scene · EYE_OFFSET + NECK_PIVOT + d_scene"]
    TR --> EYE
    EYE --> PROJ["offAxisProjection<br/>l,r,b,t = (screen edges − eye.xy) · near / depth<br/>camera fixed orientation, at eye"]
    PROJ --> VIEW["Head-coupled view (ui/HeadCameraView.tsx)<br/>screen acts as a window into the scene"]
```

## Notes

- **Rotation** drives the cursor ray and the eye's arc around the neck pivot.
  It also rotates acceleration into the world frame so gravity can be removed.
- **Translation** is dead-reckoned from acceleration only. The deadband,
  zero-velocity update, damping, re-centring, and clamp keep drift bounded, so
  it only reflects short head movements.
- **Off-axis projection:** the virtual camera never rotates. The frustum is
  skewed so the screen rectangle stays the image plane, which produces the
  look-around parallax effect.
