import { Vector2 } from 'three'
import { clamp01 } from '../math/headPointer'

type ScreenPreviewProps = {
  rawCursor: Vector2 | null
  smoothedCursor: Vector2
  hasIntersection: boolean
}

function previewStyle(cursor: Vector2): { left: string; top: string } {
  return {
    left: `${clamp01(cursor.x) * 100}%`,
    top: `${clamp01(cursor.y) * 100}%`,
  }
}

export function ScreenPreview({ rawCursor, smoothedCursor, hasIntersection }: ScreenPreviewProps) {
  return (
    <section className="card preview-card">
      <div className="card-header">
        <div>
          <h2>Cursor preview</h2>
          <p>Pink is the raw ray-plane hit, cyan is the filtered cursor output.</p>
        </div>
      </div>

      <div className="screen-preview" aria-label="Monitor preview">
        <div className="preview-crosshair preview-crosshair-horizontal" />
        <div className="preview-crosshair preview-crosshair-vertical" />
        {rawCursor !== null ? (
          <div className="preview-marker raw-marker" style={previewStyle(rawCursor)} />
        ) : null}
        <div className="preview-marker smoothed-marker" style={previewStyle(smoothedCursor)} />
      </div>

      <dl className="inline-metrics">
        <div>
          <dt>Raw</dt>
          <dd>
            {rawCursor === null ? 'held' : `${rawCursor.x.toFixed(3)}, ${rawCursor.y.toFixed(3)}`}
          </dd>
        </div>
        <div>
          <dt>Smoothed</dt>
          <dd>{smoothedCursor.x.toFixed(3)}, {smoothedCursor.y.toFixed(3)}</dd>
        </div>
        <div>
          <dt>Status</dt>
          <dd>{hasIntersection ? 'tracking' : 'no screen hit'}</dd>
        </div>
      </dl>
    </section>
  )
}
