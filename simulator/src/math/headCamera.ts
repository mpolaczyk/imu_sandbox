import { Matrix4, Quaternion, Vector3 } from 'three'
import type { Screen } from './headPointer'

/** Rotation centre (neck) relative to the neutral eye position, in metres. */
export const NECK_PIVOT = new Vector3(0, -0.12, 0.1)
/** Eye position relative to the neck pivot in the head frame (forward is -Z). */
export const EYE_OFFSET = new Vector3(0, 0.12, -0.1)

/**
 * Eye position in screen space. The sensor only provides orientation, so the eye
 * moves on an arc around the neck pivot; neutral pose puts the eye at the origin.
 */
export function eyePosition(orientation: Quaternion): Vector3 {
  return EYE_OFFSET.clone().applyQuaternion(orientation).add(NECK_PIVOT)
}

/**
 * Off-axis (head-coupled) perspective matrix for a screen-aligned display.
 * The camera keeps the screen's orientation and only translates to the eye, so the
 * frustum must be skewed so its far-plane cross-section passes through the screen edges.
 * Returns null when the eye is not in front of the screen plane.
 */
export function offAxisProjection(
  eye: Vector3,
  screen: Screen,
  near = 0.05,
  far = 50,
): Matrix4 | null {
  const distance = screen.center.z - eye.z
  if (-distance <= 1e-4) {
    return null
  }
  const depth = -distance
  const scale = near / depth
  const left = (screen.center.x - screen.widthM / 2 - eye.x) * scale
  const right = (screen.center.x + screen.widthM / 2 - eye.x) * scale
  const bottom = (screen.center.y - screen.heightM / 2 - eye.y) * scale
  const top = (screen.center.y + screen.heightM / 2 - eye.y) * scale
  return new Matrix4().makePerspective(left, right, top, bottom, near, far)
}
