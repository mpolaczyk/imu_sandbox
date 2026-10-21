type BleControlsProps = {
  supported: boolean
  connected: boolean
  packetRate: number
  droppedPackets: number
  error: string
  onConnect: () => void
  onDisconnect: () => void
}

export function BleControls({
  supported,
  connected,
  packetRate,
  droppedPackets,
  error,
  onConnect,
  onDisconnect,
}: BleControlsProps) {
  return (
    <section className="panel-card">
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
    </section>
  )
}
