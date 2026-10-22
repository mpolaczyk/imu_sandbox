import { describe, expect, it } from 'vitest'
import { Quaternion, Vector3 } from 'three'
import { createScreen, intersectNoseRayWithScreen } from './headPointer'
import { headQuaternion } from './orientation'

const nose = new Vector3(0, 0, 0)
const monitor = { widthM: 0.531, heightM: 0.299 }
const screen = createScreen({ distanceM: 0.6, ...monitor })

describe('intersectNoseRayWithScreen', () => {
  it('centers the cursor for a neutral head pose', () => {
    const hit = intersectNoseRayWithScreen(nose, new Quaternion(), screen)

    expect(hit).not.toBeNull()
    expect(hit?.u).toBeCloseTo(0.5, 6)
    expect(hit?.v).toBeCloseTo(0.5, 6)
    expect(hit?.screenX).toBeCloseTo(0, 6)
    expect(hit?.screenY).toBeCloseTo(0, 6)
  })

  it('moves the hit point left for positive right-hand-rule yaw', () => {
    const hit = intersectNoseRayWithScreen(nose, headQuaternion(12, 0, 0), screen)

    expect(hit).not.toBeNull()
    expect(hit?.screenX ?? 0).toBeLessThan(0)
    expect(hit?.u ?? 0).toBeLessThan(0.5)
  })

  it('moves the hit point up for positive pitch', () => {
    const hit = intersectNoseRayWithScreen(nose, headQuaternion(0, 10, 0), screen)

    expect(hit).not.toBeNull()
    expect(hit?.screenY ?? 0).toBeGreaterThan(0)
    expect(hit?.v ?? 1).toBeLessThan(0.5)
  })

  it('returns null when the user looks away from the screen', () => {
    const hit = intersectNoseRayWithScreen(nose, headQuaternion(180, 0, 0), screen)

    expect(hit).toBeNull()
  })

  it('scales physical hit distance with monitor distance', () => {
    const nearHit = intersectNoseRayWithScreen(nose, headQuaternion(10, 0, 0), createScreen({ distanceM: 0.6, ...monitor }))
    const farHit = intersectNoseRayWithScreen(nose, headQuaternion(10, 0, 0), createScreen({ distanceM: 1.0, ...monitor }))

    expect(nearHit).not.toBeNull()
    expect(farHit).not.toBeNull()
    expect(Math.abs(farHit?.screenX ?? 0)).toBeGreaterThan(Math.abs(nearHit?.screenX ?? 0))
  })
})
