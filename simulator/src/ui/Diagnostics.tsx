import { Quaternion, Vector3 } from 'three'
import type { CursorHit, Screen } from '../math/headPointer'
import type { Angles } from '../simulation/calibration'

type DiagnosticsProps = {
  screen: Screen
  effectiveAngles: Angles
  rawHit: CursorHit | null
  orientation: Quaternion
  forward: Vector3
}

function formatVector(vector: Vector3): string {
  return `${vector.x.toFixed(3)}, ${vector.y.toFixed(3)}, ${vector.z.toFixed(3)}`
}

function formatQuaternion(quaternion: Quaternion): string {
  return `${quaternion.x.toFixed(3)}, ${quaternion.y.toFixed(3)}, ${quaternion.z.toFixed(3)}, ${quaternion.w.toFixed(3)}`
}

export function Diagnostics({ screen, effectiveAngles, rawHit, orientation, forward }: DiagnosticsProps) {
  return (
    <section className="card panel-card diagnostics-card">
      <div className="card-header">
        <div>
          <h2>Diagnostics</h2>
          <p>Projection data stays in TypeScript math modules for later firmware porting.</p>
        </div>
      </div>

      <dl className="metrics-grid">
        <div>
          <dt>Effective yaw / pitch / roll</dt>
          <dd>
            {effectiveAngles.yawDeg.toFixed(1)}° / {effectiveAngles.pitchDeg.toFixed(1)}° /{' '}
            {effectiveAngles.rollDeg.toFixed(1)}°
          </dd>
        </div>
        <div>
          <dt>Forward vector</dt>
          <dd>{formatVector(forward)}</dd>
        </div>
        <div>
          <dt>Quaternion</dt>
          <dd>{formatQuaternion(orientation)}</dd>
        </div>
        <div>
          <dt>Monitor center</dt>
          <dd>{formatVector(screen.center)}</dd>
        </div>
        <div>
          <dt>Monitor size</dt>
          <dd>
            {(screen.widthM * 1000).toFixed(0)} × {(screen.heightM * 1000).toFixed(0)} mm
          </dd>
        </div>
        <div>
          <dt>Intersection</dt>
          <dd>{rawHit === null ? 'none (ray not facing screen)' : 'valid'}</dd>
        </div>
        <div>
          <dt>Hit point from center</dt>
          <dd>
            {rawHit === null
              ? '—'
              : `${(rawHit.screenX * 1000).toFixed(1)} mm, ${(rawHit.screenY * 1000).toFixed(1)} mm`}
          </dd>
        </div>
        <div>
          <dt>Normalized u / v</dt>
          <dd>
            {rawHit === null ? '—' : `${rawHit.u.toFixed(3)}, ${rawHit.v.toFixed(3)}`}
          </dd>
        </div>
      </dl>
    </section>
  )
}
