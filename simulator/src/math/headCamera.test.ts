import { describe, expect, it } from 'vitest'
import { Quaternion, Vector3 } from 'three'
import { EYE_OFFSET, NECK_PIVOT_POSITION } from '../config'
import { eyePosition, offAxisProjection } from './headCamera'
import { createScreen } from './headPointer'
import { headQuaternion } from './orientation'

const screen = createScreen({ widthM: 0.531, heightM: 0.299 })
const eyeLevelY = screen.heightM / 2
const eyeDistance = 0.6

describe('eyePosition', () => {
  it('is at the pivot plus the eye offset for a neutral pose', () => {
    const eye = eyePosition(new Quaternion())
    expect(eye.distanceTo(NECK_PIVOT_POSITION.clone().add(EYE_OFFSET))).toBeCloseTo(0, 6)
  })

  it('swings sideways on the arc when the head rolls', () => {
    expect(eyePosition(headQuaternion(0, 0, 20)).x).toBeLessThan(0)
  })
})

describe('offAxisProjection', () => {
  it('is symmetric when the eye is centred on the screen', () => {
    const m = offAxisProjection(new Vector3(0, eyeLevelY, eyeDistance), screen)
    expect(m).not.toBeNull()
    expect(m!.elements[8]).toBeCloseTo(0, 6)
    expect(m!.elements[9]).toBeCloseTo(0, 6)
  })

  it('skews the frustum when the eye moves sideways', () => {
    const m = offAxisProjection(new Vector3(0.1, eyeLevelY, eyeDistance), screen)
    expect(m!.elements[8]).not.toBeCloseTo(0, 3)
  })

  it('maps the screen centre point to its edge consistently', () => {
    const eye = new Vector3(0.1, eyeLevelY + 0.05, eyeDistance)
    const m = offAxisProjection(eye, screen)!
    const edge = new Vector3(screen.widthM / 2, screen.heightM, screen.center.z).sub(eye)
    const ndc = edge.applyMatrix4(m)
    expect(ndc.x).toBeCloseTo(1, 5)
    expect(ndc.y).toBeCloseTo(1, 5)
  })

  it('returns null when the eye is behind the screen plane', () => {
    expect(offAxisProjection(new Vector3(0, eyeLevelY, -1), screen)).toBeNull()
  })
})
