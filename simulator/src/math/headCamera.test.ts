import { describe, expect, it } from 'vitest'
import { Quaternion, Vector3 } from 'three'
import { eyePosition, offAxisProjection } from './headCamera'
import { createScreen } from './headPointer'
import { headQuaternion } from './orientation'

const screen = createScreen()

describe('eyePosition', () => {
  it('is at the origin for a neutral pose', () => {
    const eye = eyePosition(new Quaternion())
    expect(eye.length()).toBeCloseTo(0, 6)
  })

  it('moves left when the head yaws left', () => {
    expect(eyePosition(headQuaternion(20, 0, 0)).x).toBeLessThan(0)
  })
})

describe('offAxisProjection', () => {
  it('is symmetric when the eye is centred on the screen', () => {
    const m = offAxisProjection(new Vector3(), screen)
    expect(m).not.toBeNull()
    expect(m!.elements[8]).toBeCloseTo(0, 6)
    expect(m!.elements[9]).toBeCloseTo(0, 6)
  })

  it('skews the frustum when the eye moves sideways', () => {
    const m = offAxisProjection(new Vector3(0.1, 0, 0), screen)
    expect(m!.elements[8]).not.toBeCloseTo(0, 3)
  })

  it('maps the screen centre point to its edge consistently', () => {
    const eye = new Vector3(0.1, 0.05, 0)
    const m = offAxisProjection(eye, screen)!
    const edge = new Vector3(screen.widthM / 2, screen.heightM / 2, screen.center.z).sub(eye)
    const ndc = edge.applyMatrix4(m)
    expect(ndc.x).toBeCloseTo(1, 5)
    expect(ndc.y).toBeCloseTo(1, 5)
  })

  it('returns null when the eye is behind the screen plane', () => {
    expect(offAxisProjection(new Vector3(0, 0, -1), screen)).toBeNull()
  })
})
