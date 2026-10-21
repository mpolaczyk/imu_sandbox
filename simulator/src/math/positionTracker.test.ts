import { describe, expect, it } from 'vitest'
import { Quaternion, Vector3 } from 'three'
import { PositionTracker } from './positionTracker'

const identity = new Quaternion()
const gravity = new Vector3(0, 0, 9.80665)

function settle(tracker: PositionTracker) {
  for (let i = 0; i < 40; i += 1) {
    tracker.update(identity, gravity, 0.02)
  }
}

describe('PositionTracker', () => {
  it('stays at the origin while at rest', () => {
    const tracker = new PositionTracker()
    settle(tracker)
    for (let i = 0; i < 200; i += 1) {
      tracker.update(identity, gravity, 0.02)
    }
    expect(tracker.update(identity, gravity, 0.02).length()).toBeLessThan(1e-6)
  })

  it('moves in the direction of a sustained acceleration', () => {
    const tracker = new PositionTracker()
    settle(tracker)
    const push = gravity.clone().add(new Vector3(1, 0, 0))
    let position = new Vector3()
    for (let i = 0; i < 10; i += 1) {
      position = tracker.update(identity, push, 0.02)
    }
    expect(position.x).toBeGreaterThan(0.005)
    expect(Math.abs(position.y)).toBeLessThan(1e-6)
  })

  it('cancels gravity when the sensor is tilted', () => {
    const tracker = new PositionTracker()
    const tilt = new Quaternion().setFromAxisAngle(new Vector3(1, 0, 0), 0.5)
    const body = gravity.clone().applyQuaternion(tilt.clone().invert())
    for (let i = 0; i < 100; i += 1) {
      tracker.update(tilt, body, 0.02)
    }
    expect(tracker.update(tilt, body, 0.02).length()).toBeLessThan(1e-6)
  })

  it('returns to the origin after the head stops', () => {
    const tracker = new PositionTracker()
    settle(tracker)
    const push = gravity.clone().add(new Vector3(2, 0, 0))
    for (let i = 0; i < 10; i += 1) {
      tracker.update(identity, push, 0.02)
    }
    let position = new Vector3()
    for (let i = 0; i < 500; i += 1) {
      position = tracker.update(identity, gravity, 0.02)
    }
    expect(position.length()).toBeLessThan(0.002)
  })

  it('clamps displacement', () => {
    const tracker = new PositionTracker({ maxOffsetM: 0.1 })
    settle(tracker)
    const push = gravity.clone().add(new Vector3(20, 0, 0))
    let position = new Vector3()
    for (let i = 0; i < 100; i += 1) {
      position = tracker.update(identity, push, 0.02)
    }
    expect(position.x).toBeLessThanOrEqual(0.1)
  })
})
