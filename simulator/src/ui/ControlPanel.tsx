type ControlPanelProps = {
  simulating: boolean
  onReset: () => void
  onToggleSimulation: () => void
}

export function ControlPanel({ simulating, onReset, onToggleSimulation }: ControlPanelProps) {
  return (
    <section className="panel-card">
      <div className="card-header">
        <div>
          <h2>Parameters</h2>
          <p>Reset sets the current head pose as neutral.</p>
        </div>
      </div>

      <div className="button-row">
        <button type="button" className="secondary-button" onClick={onReset}>
          Reset
        </button>
        <button type="button" onClick={onToggleSimulation}>
          {simulating ? 'Stop simulation' : 'Simulate head tilt'}
        </button>
      </div>
    </section>
  )
}
