import { MathUtils, Quaternion, Vector3 } from 'three'

const X_AXIS = new Vector3(1, 0, 0)
const Y_AXIS = new Vector3(0, 1, 0)
const Z_AXIS = new Vector3(0, 0, 1)
const SENSOR_TO_SCENE = new Quaternion(-0.5, -0.5, -0.5, 0.5)
const SCENE_TO_SENSOR = SENSOR_TO_SCENE.clone().invert()

export function relativeImuOrientation(current: Quaternion, reference: Quaternion): Quaternion {
  const relativeSensor = reference.clone().invert().multiply(current).normalize()
  return SENSOR_TO_SCENE.clone()
    .multiply(relativeSensor)
    .multiply(SCENE_TO_SENSOR)
    .normalize()
}

/** Converts a world-frame sensor displacement to the scene frame, relative to the reference pose. */
export function relativeImuTranslation(worldDisplacement: Vector3, reference: Quaternion): Vector3 {
  return worldDisplacement.clone().applyQuaternion(reference.clone().invert()).applyQuaternion(SENSOR_TO_SCENE)
}

export function headQuaternion(
  yawDeg: number,
  pitchDeg: number,
  rollDeg: number,
): Quaternion {
  const yaw = new Quaternion().setFromAxisAngle(Y_AXIS, MathUtils.degToRad(yawDeg))
  const pitch = new Quaternion().setFromAxisAngle(X_AXIS, MathUtils.degToRad(pitchDeg))
  const roll = new Quaternion().setFromAxisAngle(Z_AXIS, MathUtils.degToRad(rollDeg))

  return yaw.multiply(pitch).multiply(roll)
}
