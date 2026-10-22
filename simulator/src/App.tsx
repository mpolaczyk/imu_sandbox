import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { Quaternion, Vector3 } from 'three'
import './App.css'
import {
  MONITOR_HEIGHT_M,
  MONITOR_WIDTH_M,
  SCENE_ORIGIN_OFFSET,
  USE_ACCELERATION_TRACKER,
} from './config'
import { createScreen } from './math/headPointer'
import { PositionTracker } from './math/positionTracker'
import { relativeImuOrientation, relativeImuTranslation } from './math/orientation'
import { BlePoseClient, type PosePacket, type PoseStats } from './ble/ble-client'
import { BleControls } from './ui/BleControls'
import { ControlPanel } from './ui/ControlPanel'
import { HeadCameraView } from './ui/HeadCameraView'

const MONITOR = createScreen({
  widthM: MONITOR_WIDTH_M,
  heightM: MONITOR_HEIGHT_M,
  originOffset: SCENE_ORIGIN_OFFSET,
})
const NEUTRAL_ORIENTATION = new Quaternion()
const DEFAULT_POSE_STATS: PoseStats = { packetRate: 0, droppedPackets: 0 }

function poseQuaternion(pose: PosePacket): Quaternion {
  const [w, x, y, z] = pose.quaternion
  return new Quaternion(x, y, z, w)
}

function App() {
  const client = useMemo(() => new BlePoseClient(), [])
  const [livePose, setLivePose] = useState<PosePacket | null>(null)
  const [liveReference, setLiveReference] = useState<Quaternion | null>(null)
  const tracker = useMemo(() => new PositionTracker(), [])
  const lastTimestampMs = useRef<number | null>(null)
  const trackerReference = useRef<Quaternion | null>(null)
  const [livePosition, setLivePosition] = useState<Vector3 | null>(null)
  const [bleConnected, setBleConnected] = useState(false)
  const [bleStats, setBleStats] = useState(DEFAULT_POSE_STATS)
  const [bleError, setBleError] = useState('')

  const resetTracking = useCallback(() => {
    tracker.reset()
    lastTimestampMs.current = null
    trackerReference.current = null
    setLivePosition(null)
  }, [tracker])

  useEffect(() => {
    const unsubscribePose = client.onPose((pose, stats) => {
      setLivePose(pose)
      const receivedOrientation = poseQuaternion(pose)
      setLiveReference((reference) => reference ?? receivedOrientation)
      if (USE_ACCELERATION_TRACKER) {
        trackerReference.current ??= receivedOrientation
        const previous = lastTimestampMs.current
        lastTimestampMs.current = pose.timestampMs
        const rawDt = previous === null ? 0 : ((pose.timestampMs - previous) >>> 0) / 1000
        const dtS = rawDt > 0 && rawDt <= 0.1 ? rawDt : 0.02
        const world = tracker.update(receivedOrientation, new Vector3(...pose.accelMps2), dtS)
        setLivePosition(
          tracker.ready ? relativeImuTranslation(world, trackerReference.current) : null,
        )
      }
      setBleStats(stats)
    })
    const unsubscribeConnect = client.onConnect(() => {
      setBleConnected(true)
      setLivePose(null)
      setLiveReference(null)
      resetTracking()
      setBleError('')
    })
    const unsubscribeDisconnect = client.onDisconnect(() => {
      resetTracking()
      setBleConnected(false)
      setLivePose(null)
      setLiveReference(null)
    })
    const unsubscribeError = client.onError((error) => setBleError(error.message))
    return () => {
      unsubscribePose()
      unsubscribeConnect()
      unsubscribeDisconnect()
      unsubscribeError()
      client.disconnect()
    }
  }, [client, tracker, resetTracking])

  const liveOrientation = useMemo(() => {
    if (!livePose || !liveReference) {
      return null
    }
    const deviceOrientation = poseQuaternion(livePose)
    return relativeImuOrientation(deviceOrientation, liveReference)
  }, [livePose, liveReference])
  const orientation = liveOrientation ?? NEUTRAL_ORIENTATION

  const handleReset = () => {
    const current = livePose ? poseQuaternion(livePose) : null
    setLiveReference(current)
    tracker.reset()
    trackerReference.current = current
    setLivePosition(null)
  }

  return (
    <main className="app-shell">
      <section className="card configuration-box">
        <div className="card-header">
          <h2>Parameters and BLE pose input</h2>
        </div>
        <div className="configuration-grid">
          <BleControls
            supported={client.supported}
            connected={bleConnected}
            packetRate={bleStats.packetRate}
            droppedPackets={bleStats.droppedPackets}
            error={bleError}
            onConnect={() => void client.reconnect()}
            onDisconnect={() => client.disconnect()}
          />
          <ControlPanel onReset={handleReset} />
        </div>
      </section>

      <HeadCameraView
        orientation={orientation}
        screen={MONITOR}
        translation={USE_ACCELERATION_TRACKER ? (livePosition ?? undefined) : undefined}
      />
    </main>
  )
}

export default App
