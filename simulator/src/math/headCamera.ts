import { Matrix4, Quaternion, Vector3 } from 'three'
import { EYE_OFFSET, NECK_PIVOT } from '../config'
import type { Screen } from './headPointer'

/** Eye position in screen space: an arc around the neck pivot, plus optional tracked translation (scene frame). */
export function eyePosition(orientation: Quaternion, translation?: Vector3): Vector3 {
  const eye = EYE_OFFSET.clone().applyQuaternion(orientation).add(NECK_PIVOT)
  return translation ? eye.add(translation) : eye
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
  far = 5,
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
