import { Vector3 } from 'three'
import type { PositionTrackerOptions } from './math/positionTracker'

/*
 * Geometry (metres). Origin = centre of the monitor's bottom edge; +X right, +Y up,
 * +Z towards the user (the room is behind the screen, at -Z).
 *
 * SIDE VIEW (head looks left, towards the screen)
 *
 *     +Y
 *      ^   top edge
 *      |  +
 *      |  |                                       EYE_OFFSET (pivot -> eye, rotated by head)
 *      |  |                               O <-----------------.
 *    H |  |   monitor                    eye                  |
 *      |  |                                                   |
 *      |  +   bottom edge = origin                            x NECK_PIVOT_POSITION
 *      +----------------------------------------------> +Z   |
 *                                                       SENSOR_OFFSET: eye -> IMU
 *
 *   pivot = NECK_PIVOT_POSITION                     (fixed point, from the origin)
 *   eye   = pivot + R * EYE_OFFSET                  (R = head orientation)
 *   imu   = eye   + R * SENSOR_OFFSET
 *
 * FRONT VIEW (monitor as seen from the user)
 *
 *          |<------ MONITOR_WIDTH_M ------>|
 *          +-------------------------------+ --
 *          |                               |  ^
 *          |            monitor            |  | MONITOR_HEIGHT_M
 *          |                               |  v
 *          +---------------+---------------+ -- 
 *                          |
 *                          |origin (x = 0, y = 0, z = 0)
 */

/** Physical monitor, in metres. */
export const MONITOR_WIDTH_M = 0.32
export const MONITOR_HEIGHT_M = 0.18
/** Shifts the monitor and the room relative to the body (+X right, +Y up, +Z towards the user). Tunes the look without changing measurements. */
export const SCENE_ORIGIN_OFFSET = new Vector3(0, 0.05, 0)

/** Neck pivot position relative to the monitor origin. Fixes the neutral eye distance and height. */
export const NECK_PIVOT_POSITION = new Vector3(0, -0.05, 0.53)
/** Eye position relative to the neck pivot in the head frame (forward is -Z). */
export const EYE_OFFSET = new Vector3(0, 0.27, -0.05)

/** Add dead-reckoned head translation from the accelerometer on top of the neck-arc model. Drifts; off by default. */
export const USE_ACCELERATION_TRACKER = false
/** IMU position relative to the eye in the head frame. Only needed for accelerometer lever-arm compensation. */
export const SENSOR_OFFSET = new Vector3(0, 6, -6)

/** Defaults for the accelerometer position tracker. */
export const DEFAULT_TRACKER_OPTIONS: PositionTrackerOptions = {                 
  warmupSamples: 25,
  accelDeadbandMps2: 0.08,
  stationarySamples: 10,
  recenterTimeS: 1.5,
  velocityDamping: 1.5,
  maxOffsetM: 0.3,
}

/** Simulation mode: head rolls +/- SIMULATION_TILT_DEG with a sine of period SIMULATION_PERIOD_S. */
export const SIMULATION_TILT_DEG = 36
export const SIMULATION_PERIOD_S = 5
