type ControlPanelProps = {
  onReset: () => void
}

export function ControlPanel({ onReset }: ControlPanelProps) {
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
      </div>
    </section>
  )
}
