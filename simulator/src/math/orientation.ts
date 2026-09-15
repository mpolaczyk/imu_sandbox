import { MathUtils, Quaternion, Vector3 } from 'three'

const X_AXIS = new Vector3(1, 0, 0)
const Y_AXIS = new Vector3(0, 1, 0)
const Z_AXIS = new Vector3(0, 0, 1)

export function headQuaternion(
  yawDeg: number,
  pitchDeg: number,
  rollDeg: number,
): Quaternion {
  const yaw = new Quaternion().setFromAxisAngle(Y_AXIS, MathUtils.degToRad(yawDeg))
  const pitch = new Quaternion().setFromAxisAngle(X_AXIS, MathUtils.degToRad(-pitchDeg))
  const roll = new Quaternion().setFromAxisAngle(Z_AXIS, MathUtils.degToRad(rollDeg))

  return yaw.multiply(pitch).multiply(roll)
}
