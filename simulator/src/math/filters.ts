import { Vector2 } from 'three'

export function ema(previous: Vector2, raw: Vector2, alpha: number): Vector2 {
  return previous.clone().lerp(raw, alpha)
}
