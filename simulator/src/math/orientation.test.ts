import { describe, expect, it } from 'vitest'
import { Quaternion, Vector3 } from 'three'
import { relativeImuOrientation } from './orientation'

const SENSOR_TO_SCENE = new Quaternion(-0.5, -0.5, -0.5, 0.5)

describe('relativeImuOrientation', () => {
  it('rebases the reference pose to neutral', () => {
    const reference = new Quaternion().setFromAxisAngle(new Vector3(0, 1, 0), 0.7)

    expect(relativeImuOrientation(reference, reference).angleTo(new Quaternion())).toBeCloseTo(0, 6)
  })

  it('maps sensor +Z yaw into scene +Y rotation', () => {
    const sensorYaw = new Quaternion().setFromAxisAngle(new Vector3(0, 0, 1), 0.2)
    const sceneYaw = relativeImuOrientation(sensorYaw, new Quaternion())
    const expected = SENSOR_TO_SCENE.clone()
      .multiply(sensorYaw)
      .multiply(SENSOR_TO_SCENE.clone().invert())

    expect(sceneYaw.angleTo(expected)).toBeCloseTo(0, 6)
    expect(new Vector3(0, 0, -1).applyQuaternion(sceneYaw).x).toBeLessThan(0)
  })

  it('maps sensor +Y pitch upward and sensor +X roll by the right-hand rule', () => {
    const sensorPitch = new Quaternion().setFromAxisAngle(new Vector3(0, 1, 0), 0.2)
    const scenePitch = relativeImuOrientation(sensorPitch, new Quaternion())
    const sensorRoll = new Quaternion().setFromAxisAngle(new Vector3(1, 0, 0), 0.2)
    const sceneRoll = relativeImuOrientation(sensorRoll, new Quaternion())

    expect(new Vector3(0, 0, -1).applyQuaternion(scenePitch).y).toBeGreaterThan(0)
    expect(new Vector3(1, 0, 0).applyQuaternion(sceneRoll).y).toBeGreaterThan(0)
  })
})