type ControlPanelProps = {
  distanceM: number
  onDistanceChange: (distanceM: number) => void
  onCalibrate: () => void
  onReset: () => void
}

export function ControlPanel({
  distanceM,
  onDistanceChange,
  onCalibrate,
  onReset,
}: ControlPanelProps) {
  return (
    <section className="panel-card">
      <div className="card-header">
        <div>
          <h2>Parameters</h2>
          <p>Adjust monitor distance or recalibrate the neutral head pose.</p>
        </div>
      </div>

      <div className="slider-list">
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

      </div>

      <div className="button-row">
        <button type="button" onClick={onCalibrate}>
          Calibrate neutral pose
        </button>
        <button type="button" className="secondary-button" onClick={onReset}>
          Reset
        </button>
      </div>
    </section>
  )
}
