import { Matrix4, Quaternion, Vector3 } from 'three'
import { EYE_OFFSET, NECK_PIVOT_POSITION } from '../config'
import type { Screen } from './headPointer'

/** Eye position in monitor space: an arc around the neck pivot, plus optional tracked translation. */
export function eyePosition(orientation: Quaternion, translation?: Vector3): Vector3 {
  const eye = EYE_OFFSET.clone().applyQuaternion(orientation).add(NECK_PIVOT_POSITION)
  return translation ? eye.add(translation) : eye
}

export type Focus = {
  /** Scene point the frustum axis aims at, behind the screen plane. Omit for a point at infinity straight ahead (-Z). */
  point?: Vector3
  /** 0 = pure window (axis through the screen centre), 1 = axis through `point`. */
  blend: number
}

/**
 * Off-axis (head-coupled) perspective matrix for a screen-aligned display.
 * The camera keeps the screen's orientation and only translates to the eye, so the
 * frustum must be skewed so its far-plane cross-section passes through the screen edges.
 * With `focus`, the aperture (same size) is shifted towards where the eye-to-focus line pierces the screen plane.
 * Returns null when the eye is not in front of the screen plane.
 */
export function offAxisProjection(
  eye: Vector3,
  screen: Screen,
  near = 0.05,
  far = 5,
  focus?: Focus,
): Matrix4 | null {
  const distance = screen.center.z - eye.z
  if (-distance <= 1e-4) {
    return null
  }
  const depth = -distance
  const scale = near / depth
  let centerX = screen.center.x
  let centerY = screen.center.y
  if (focus) {
    let pierceX = eye.x
    let pierceY = eye.y
    if (focus.point) {
      const focusDepth = eye.z - focus.point.z
      if (focusDepth > depth) {
        const t = depth / focusDepth
        pierceX = eye.x + (focus.point.x - eye.x) * t
        pierceY = eye.y + (focus.point.y - eye.y) * t
      } else {
        pierceX = centerX
        pierceY = centerY
      }
    }
    centerX += focus.blend * (pierceX - centerX)
    centerY += focus.blend * (pierceY - centerY)
  }
  const left = (centerX - screen.widthM / 2 - eye.x) * scale
  const right = (centerX + screen.widthM / 2 - eye.x) * scale
  const bottom = (centerY - screen.heightM / 2 - eye.y) * scale
  const top = (centerY + screen.heightM / 2 - eye.y) * scale
  return new Matrix4().makePerspective(left, right, top, bottom, near, far)
}
