import { Quaternion, Vector2, Vector3 } from 'three'

export type Screen = {
  center: Vector3
  normal: Vector3
  right: Vector3
  up: Vector3
  widthM: number
  heightM: number
}

export type CursorHit = {
  hitWorld: Vector3
  screenX: number
  screenY: number
  u: number
  v: number
}

const FORWARD = new Vector3(0, 0, -1)

export function createScreen(options: {
  distanceM: number
  widthM: number
  heightM: number
  /** Neutral eye height as a fraction of the screen height from the bottom edge. */
  eyeHeightRatio?: number
}): Screen {
  const { distanceM, widthM, heightM } = options
  const eyeHeightRatio = options.eyeHeightRatio ?? 0.5

  return {
    center: new Vector3(0, (0.5 - eyeHeightRatio) * heightM, -distanceM),
    normal: new Vector3(0, 0, -1),
    right: new Vector3(1, 0, 0),
    up: new Vector3(0, 1, 0),
    widthM,
    heightM,
  }
}

export function forwardVector(faceOrientation: Quaternion): Vector3 {
  return FORWARD.clone().applyQuaternion(faceOrientation).normalize()
}

export function intersectNoseRayWithScreen(
  noseWorld: Vector3,
  faceOrientation: Quaternion,
  screen: Screen,
): CursorHit | null {
  const forward = forwardVector(faceOrientation)
  const denominator = screen.normal.dot(forward)

  if (denominator <= 0.0001) {
    return null
  }

  const t = screen.normal.dot(screen.center.clone().sub(noseWorld)) / denominator

  if (t <= 0) {
    return null
  }

  const hitWorld = noseWorld.clone().addScaledVector(forward, t)
  const offset = hitWorld.clone().sub(screen.center)
  const screenX = offset.dot(screen.right)
  const screenY = offset.dot(screen.up)

  return {
    hitWorld,
    screenX,
    screenY,
    u: screenX / screen.widthM + 0.5,
    v: 0.5 - screenY / screen.heightM,
  }
}

export function screenUvToWorld(screen: Screen, cursor: Vector2): Vector3 {
  const screenX = (cursor.x - 0.5) * screen.widthM
  const screenY = (0.5 - cursor.y) * screen.heightM

  return screen.center
    .clone()
    .addScaledVector(screen.right, screenX)
    .addScaledVector(screen.up, screenY)
}

export function clamp01(value: number): number {
  return Math.max(0, Math.min(1, value))
}
