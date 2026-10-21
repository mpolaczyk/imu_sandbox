// Keep it consistent with imu_ble.h
export const IMU_POSE_SERVICE_UUID = '7e400001-b5a3-f393-e0a9-e50e24ccca9e'
export const IMU_POSE_CHARACTERISTIC_UUID = '7e400002-b5a3-f393-e0a9-e50e24ccca9e'
export const IMU_INFO_CHARACTERISTIC_UUID = '7e400003-b5a3-f393-e0a9-e50e24ccca9e'

export const POSE_FRAME_SIZE = 20
const STANDARD_GRAVITY = 9.80665

export type PosePacket = {
  sequence: number
  timestampMs: number
  quaternion: [number, number, number, number]
  /** Body-frame specific force (gravity included) in m/s^2. */
  accelMps2: [number, number, number]
}

export type PoseStats = {
  packetRate: number
  droppedPackets: number
}

type BluetoothCharacteristic = {
  value: DataView | null
  startNotifications(): Promise<BluetoothCharacteristic>
  addEventListener(type: 'characteristicvaluechanged', listener: (event: Event) => void): void
}

type BluetoothService = {
  getCharacteristic(uuid: string): Promise<BluetoothCharacteristic>
}

type BluetoothServer = {
  connected: boolean
  connect(): Promise<BluetoothServer>
  getPrimaryService(uuid: string): Promise<BluetoothService>
  disconnect(): void
}

type BluetoothDevice = {
  gatt?: BluetoothServer | null
  addEventListener(type: 'gattserverdisconnected', listener: () => void): void
}

type BluetoothAPI = {
  requestDevice(options: { filters: { services: string[] }[] }): Promise<BluetoothDevice>
}

export function parsePoseFrame(view: DataView): PosePacket {
  if (view.byteLength !== POSE_FRAME_SIZE) {
    throw new Error(`Invalid pose frame length: expected ${POSE_FRAME_SIZE} bytes`)
  }

  const quaternion = [
    view.getInt16(6, true) / 16384,
    view.getInt16(8, true) / 16384,
    view.getInt16(10, true) / 16384,
    view.getInt16(12, true) / 16384,
  ] as [number, number, number, number]
  const magnitude = Math.hypot(...quaternion)
  if (magnitude < 1e-6) {
    throw new Error('Pose frame contains a zero-length quaternion')
  }

  return {
    sequence: view.getUint16(0, true),
    timestampMs: view.getUint32(2, true),
    quaternion: quaternion.map((value) => value / magnitude) as [number, number, number, number],
    accelMps2: [
      (view.getInt16(14, true) * STANDARD_GRAVITY) / 1000,
      (view.getInt16(16, true) * STANDARD_GRAVITY) / 1000,
      (view.getInt16(18, true) * STANDARD_GRAVITY) / 1000,
    ],
  }
}

export class BlePoseClient {
  private device: BluetoothDevice | null = null
  private server: BluetoothServer | null = null
  private lastSequence: number | null = null
  private droppedPackets = 0
  private rateWindowStart = 0
  private rateWindowPackets = 0
  private packetRate = 0
  private readonly poseListeners = new Set<(pose: PosePacket, stats: PoseStats) => void>()
  private readonly connectListeners = new Set<() => void>()
  private readonly disconnectListeners = new Set<() => void>()
  private readonly errorListeners = new Set<(error: Error) => void>()

  onPose(listener: (pose: PosePacket, stats: PoseStats) => void): () => void {
    this.poseListeners.add(listener)
    return () => this.poseListeners.delete(listener)
  }

  onConnect(listener: () => void): () => void {
    this.connectListeners.add(listener)
    return () => this.connectListeners.delete(listener)
  }

  onDisconnect(listener: () => void): () => void {
    this.disconnectListeners.add(listener)
    return () => this.disconnectListeners.delete(listener)
  }

  onError(listener: (error: Error) => void): () => void {
    this.errorListeners.add(listener)
    return () => this.errorListeners.delete(listener)
  }

  get supported(): boolean {
    return 'bluetooth' in navigator
  }

  async connect(): Promise<void> {
    if (!this.supported) {
      this.emitError(new Error('Web Bluetooth is unavailable. Use Chrome or Edge on HTTPS or localhost.'))
      return
    }

    try {
      const bluetooth = (navigator as Navigator & { bluetooth: BluetoothAPI }).bluetooth
      this.device = await bluetooth.requestDevice({
        filters: [{ services: [IMU_POSE_SERVICE_UUID] }],
      })
      this.device.addEventListener('gattserverdisconnected', this.handleDisconnect)
      await this.connectDevice()
    } catch (error) {
      this.emitError(error instanceof Error ? error : new Error(String(error)))
    }
  }

  async reconnect(): Promise<void> {
    if (!this.device) {
      return this.connect()
    }
    try {
      await this.connectDevice()
    } catch (error) {
      this.emitError(error instanceof Error ? error : new Error(String(error)))
    }
  }

  disconnect(): void {
    if (this.server?.connected) {
      this.server.disconnect()
    } else {
      this.handleDisconnect()
    }
  }

  private readonly handleDisconnect = (): void => {
    this.server = null
    this.lastSequence = null
    this.disconnectListeners.forEach((listener) => listener())
  }

  private async connectDevice(): Promise<void> {
    const server = await this.device?.gatt?.connect()
    if (!server) {
      throw new Error('The selected BLE device does not support GATT.')
    }
    const service = await server.getPrimaryService(IMU_POSE_SERVICE_UUID)
    const poseCharacteristic = await service.getCharacteristic(IMU_POSE_CHARACTERISTIC_UUID)
	poseCharacteristic.addEventListener('characteristicvaluechanged', () => {
		const value = poseCharacteristic.value
		if (!value) {
			return
		}

		try {
			this.handlePose(parsePoseFrame(value))
		} catch (error) {
			console.error('[BLE] frame rejected', error, value)
			this.emitError(error instanceof Error ? error : new Error(String(error)))
		}
	})
    await poseCharacteristic.startNotifications()
    console.info('[BLE] startNotifications resolved')
    this.server = server
    this.lastSequence = null
    this.droppedPackets = 0
    this.rateWindowStart = performance.now()
    this.rateWindowPackets = 0
    this.packetRate = 0
    this.connectListeners.forEach((listener) => listener())
  }

  private handlePose(pose: PosePacket): void {
    if (this.lastSequence !== null) {
      const sequenceDelta = (pose.sequence - this.lastSequence + 0x10000) % 0x10000
      if (sequenceDelta > 1 && sequenceDelta < 0x8000) {
        this.droppedPackets += sequenceDelta - 1
      }
    }
    this.lastSequence = pose.sequence

    const now = performance.now()
    this.rateWindowPackets += 1
    const elapsed = now - this.rateWindowStart
    if (elapsed >= 1000) {
      this.packetRate = (this.rateWindowPackets * 1000) / elapsed
      console.info('[BLE] receiving pose notifications', {
        packets: this.rateWindowPackets,
        packetsPerSecond: Number(this.packetRate.toFixed(1)),
        sequence: pose.sequence,
      })
      this.rateWindowStart = now
      this.rateWindowPackets = 0
    }

    const stats = { packetRate: this.packetRate, droppedPackets: this.droppedPackets }
    this.poseListeners.forEach((listener) => listener(pose, stats))
  }

  private emitError(error: Error): void {
    this.errorListeners.forEach((listener) => listener(error))
  }
}
