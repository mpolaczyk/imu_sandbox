# Coordinate frames

The simulator uses metres and a right-handed calibrated screen frame:

- `+X`: right across the monitor
- `+Y`: upward
- `+Z`: from the user's nose toward the monitor

Default geometry:

- Nose origin: `(0, 0, 0)`
- Monitor center: `(0, 0, 0.60)`
- Monitor size: `0.531 m × 0.299 m`

Projection rules:

1. Build a head orientation quaternion from yaw, pitch, and roll.
2. Rotate the face-forward vector `(0, 0, 1)`.
3. Intersect that ray with the monitor plane.
4. Project the hit onto monitor `right` and `up` axes.
5. Convert to normalized coordinates where `u` grows rightward and `v` grows downward.

Sign conventions used by the tests and UI:

- Positive yaw moves the cursor to the right.
- Positive pitch moves the cursor upward.
- Roll affects head orientation visualization but does not change the face-forward direction by itself.
