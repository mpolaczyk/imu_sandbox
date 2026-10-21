import { Quaternion, Vector3 } from 'three'

export type PositionTrackerOptions = {
  /** Samples averaged right after a reset to estimate gravity (device must be still). */
  warmupSamples: number
  /** World-frame linear acceleration below this (m/s^2) counts as "not accelerating". */
  accelDeadbandMps2: number
  /** Consecutive quiet samples needed before the head is considered stationary. */
  stationarySamples: number
  /** Time constant (s) of the pull back towards the origin while stationary. */
  recenterTimeS: number
  /** Velocity decay per second while moving, limits integration drift. */
  velocityDamping: number
  /** Maximum displacement from the origin in metres per axis. */
  maxOffsetM: number
}

export const DEFAULT_TRACKER_OPTIONS: PositionTrackerOptions = {
  warmupSamples: 25,
  accelDeadbandMps2: 0.08,
  stationarySamples: 10,
  recenterTimeS: 1.5,
  velocityDamping: 1.5,
  maxOffsetM: 0.3,
}

/**
 * Dead-reckons head translation from body-frame specific force and a body-to-world quaternion.
 * Gravity is calibrated at rest, then removed; double integration is stabilised with a
 * deadband, zero-velocity updates, velocity damping, and a slow pull back to the origin.
 * Output is the displacement from the calibration pose in the world frame (metres).
 */
export class PositionTracker {
  private readonly options: PositionTrackerOptions
  private readonly gravityWorld = new Vector3()
  private readonly velocity = new Vector3()
  private readonly position = new Vector3()
  private warmupCount = 0
  private quietCount = 0

  constructor(options: Partial<PositionTrackerOptions> = {}) {
    this.options = { ...DEFAULT_TRACKER_OPTIONS, ...options }
  }

  reset(): void {
    this.gravityWorld.set(0, 0, 0)
    this.velocity.set(0, 0, 0)
    this.position.set(0, 0, 0)
    this.warmupCount = 0
    this.quietCount = 0
  }

  get ready(): boolean {
    return this.warmupCount >= this.options.warmupSamples
  }

  /** Feed one sample; returns the current world-frame displacement. */
  update(orientation: Quaternion, accelBody: Vector3, dtS: number): Vector3 {
    const world = accelBody.clone().applyQuaternion(orientation)

    if (!this.ready) {
      this.warmupCount += 1
      this.gravityWorld.add(world.clone().sub(this.gravityWorld).divideScalar(this.warmupCount))
      return this.position.clone()
    }
    if (!(dtS > 0)) {
      return this.position.clone()
    }

    const linear = world.sub(this.gravityWorld)
    const quiet = linear.length() < this.options.accelDeadbandMps2
    this.quietCount = quiet ? this.quietCount + 1 : 0
    const stationary = this.quietCount >= this.options.stationarySamples

    if (stationary) {
      this.velocity.set(0, 0, 0)
      this.position.multiplyScalar(Math.max(0, 1 - dtS / this.options.recenterTimeS))
    } else {
      if (!quiet) {
        this.velocity.addScaledVector(linear, dtS)
      }
      this.velocity.multiplyScalar(Math.max(0, 1 - this.options.velocityDamping * dtS))
      this.position.addScaledVector(this.velocity, dtS)
    }

    const limit = this.options.maxOffsetM
    this.position.set(
      Math.max(-limit, Math.min(limit, this.position.x)),
      Math.max(-limit, Math.min(limit, this.position.y)),
      Math.max(-limit, Math.min(limit, this.position.z)),
    )
    return this.position.clone()
  }
}
