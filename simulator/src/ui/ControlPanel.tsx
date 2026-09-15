import type { Angles } from '../simulation/calibration'

type ControlPanelProps = {
  angles: Angles
  distanceM: number
  smoothingAlpha: number
  calibration: Angles
  onAnglesChange: (next: Angles) => void
  onDistanceChange: (distanceM: number) => void
  onSmoothingChange: (alpha: number) => void
  onCalibrate: () => void
  onReset: () => void
}

type SliderConfig = {
  key: keyof Angles
  label: string
  min: number
  max: number
  step: number
  suffix: string
}

const ANGLE_SLIDERS: SliderConfig[] = [
  { key: 'yawDeg', label: 'Yaw', min: -45, max: 45, step: 1, suffix: '°' },
  { key: 'pitchDeg', label: 'Pitch', min: -30, max: 30, step: 1, suffix: '°' },
  { key: 'rollDeg', label: 'Roll', min: -30, max: 30, step: 1, suffix: '°' },
]

export function ControlPanel({
  angles,
  distanceM,
  smoothingAlpha,
  calibration,
  onAnglesChange,
  onDistanceChange,
  onSmoothingChange,
  onCalibrate,
  onReset,
}: ControlPanelProps) {
  return (
    <section className="card panel-card">
      <div className="card-header">
        <div>
          <h2>Parameters</h2>
          <p>Use head pose sliders or recalibrate the neutral forward pose.</p>
        </div>
      </div>

      <div className="slider-list">
        {ANGLE_SLIDERS.map((slider) => (
          <label className="slider-row" key={slider.key}>
            <div className="slider-label-row">
              <span>{slider.label}</span>
              <output>{angles[slider.key].toFixed(0)}{slider.suffix}</output>
            </div>
            <input
              type="range"
              min={slider.min}
              max={slider.max}
              step={slider.step}
              value={angles[slider.key]}
              onChange={(event) =>
                onAnglesChange({
                  ...angles,
                  [slider.key]: Number(event.target.value),
                })
              }
            />
          </label>
        ))}

        <label className="slider-row">
          <div className="slider-label-row">
            <span>Monitor distance</span>
            <output>{distanceM.toFixed(2)} m</output>
          </div>
          <input
            type="range"
            min={0.35}
            max={1.1}
            step={0.01}
            value={distanceM}
            onChange={(event) => onDistanceChange(Number(event.target.value))}
          />
        </label>

        <label className="slider-row">
          <div className="slider-label-row">
            <span>Smoothing alpha</span>
            <output>{smoothingAlpha.toFixed(2)}</output>
          </div>
          <input
            type="range"
            min={0.05}
            max={1}
            step={0.01}
            value={smoothingAlpha}
            onChange={(event) => onSmoothingChange(Number(event.target.value))}
          />
        </label>
      </div>

      <div className="button-row">
        <button type="button" onClick={onCalibrate}>
          Calibrate neutral pose
        </button>
        <button type="button" className="secondary-button" onClick={onReset}>
          Reset
        </button>
      </div>

      <dl className="inline-metrics calibration-metrics">
        <div>
          <dt>Yaw offset</dt>
          <dd>{calibration.yawDeg.toFixed(1)}°</dd>
        </div>
        <div>
          <dt>Pitch offset</dt>
          <dd>{calibration.pitchDeg.toFixed(1)}°</dd>
        </div>
        <div>
          <dt>Roll offset</dt>
          <dd>{calibration.rollDeg.toFixed(1)}°</dd>
        </div>
      </dl>
    </section>
  )
}
