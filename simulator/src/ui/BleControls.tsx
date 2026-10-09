type BleControlsProps = {
  supported: boolean
  connected: boolean
  packetRate: number
  droppedPackets: number
  error: string
  inputSource: 'simulated' | 'ble'
  onSourceChange: (source: 'simulated' | 'ble') => void
  onConnect: () => void
  onDisconnect: () => void
}

export function BleControls({
  supported,
  connected,
  packetRate,
  droppedPackets,
  error,
  inputSource,
  onSourceChange,
  onConnect,
  onDisconnect,
}: BleControlsProps) {
  return (
    <section className="card panel-card">
      <div className="card-header">
        <div>
          <h2>BLE pose input</h2>
          <p>Connect to an IMU Sandbox device to visualize its live orientation.</p>
        </div>
      </div>

      <div className="button-row">
        <button type="button" onClick={connected ? onDisconnect : onConnect}>
          {connected ? 'Disconnect' : 'Connect BLE device'}
        </button>
      </div>
      <p className="ble-status" role="status">
        {connected ? `Connected · ${packetRate.toFixed(1)} packets/s · ${droppedPackets} dropped` : 'Disconnected'}
      </p>
      {!supported && (
        <p className="ble-message">
          Web Bluetooth is unsupported. Use Chrome or Edge on HTTPS or localhost.
        </p>
      )}
      {error && <p className="ble-message" role="alert">{error}</p>}

      <fieldset className="input-source">
        <legend>Simulator input source</legend>
        <label>
          <input
            type="radio"
            name="input-source"
            value="simulated"
            checked={inputSource === 'simulated'}
            onChange={() => onSourceChange('simulated')}
          />
          Simulated controls
        </label>
        <label>
          <input
            type="radio"
            name="input-source"
            value="ble"
            checked={inputSource === 'ble'}
            onChange={() => onSourceChange('ble')}
          />
          Live BLE pose
        </label>
      </fieldset>
    </section>
  )
}
