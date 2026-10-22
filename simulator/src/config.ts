import { Vector3 } from 'three'
import type { PositionTrackerOptions } from './math/positionTracker'

/*
 * Geometry (metres). Origin = neutral eye; +X right, +Y up, forward = -Z.
 *
 * SIDE VIEW (head looks right)
 *
 *                                                  +----+ -- top edge
 *                                                  |    |  ^
 *                                                  |    |  | (1 - NEUTRAL_EYE_HEIGHT_RATIO) * H
 *                                                  |    |  v
 *           EYE (0, 0, 0) O - - - - - - - - - - - -|- - | -- eye level
 *                        ^ \                       |    |  ^
 *      EYE_OFFSET        |   '- - - - - - - - - -> |    |  | NEUTRAL_EYE_HEIGHT_RATIO * H
 *      (pivot -> eye)    |    MONITOR_DISTANCE_M   |    |  v
 *                        |                         +----+ -- bottom edge
 *                        |                         monitor, height H = MONITOR_HEIGHT_M
 *                        |
 *                        x NECK_PIVOT (eye -> pivot, behind and below the eye)
 *
 *   eye = NECK_PIVOT + EYE_OFFSET (rotated by the head orientation) -> (0, 0, 0) when neutral.
 *
 * FRONT VIEW (monitor as seen from the eye)
 *
 *          |<------ MONITOR_WIDTH_M ------>|
 *          +-------------------------------+ --
 *          |                               |  ^ (1 - ratio) * H
 *    y = 0 | - - - - - - - - O - - - - - - | --
 *          |                               |  ^ ratio * H
 *          +-------------------------------+ -- bottom edge
 *                            O = eye, centred horizontally (x = 0)
 */

/** Physical monitor, in metres. */
export const MONITOR_DISTANCE_M = 0.58
export const MONITOR_WIDTH_M = 0.32
export const MONITOR_HEIGHT_M = 0.18

/** Neutral eye height as a fraction of the monitor height, from the bottom edge. */
export const NEUTRAL_EYE_HEIGHT_RATIO = 1

/** Rotation centre (neck) relative to the neutral eye position, in metres. */
export const NECK_PIVOT = new Vector3(0, -0.41, 0.1)
/** Eye position relative to the neck pivot in the head frame (forward is -Z). */
export const EYE_OFFSET = new Vector3(0, 0.39, -0.1)

/** Add dead-reckoned head translation from the accelerometer on top of the neck-arc model. Drifts; off by default. */
export const USE_ACCELERATION_TRACKER = false

/** Defaults for the accelerometer position tracker. */
export const DEFAULT_TRACKER_OPTIONS: PositionTrackerOptions = {
  warmupSamples: 25,
  accelDeadbandMps2: 0.08,
  stationarySamples: 10,
  recenterTimeS: 1.5,
  velocityDamping: 1.5,
  maxOffsetM: 0.3,
}
