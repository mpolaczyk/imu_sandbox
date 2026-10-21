import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { Quaternion, Vector3 } from 'three'
import './App.css'
import { createScreen } from './math/headPointer'
import { PositionTracker } from './math/positionTracker'
import { relativeImuOrientation, relativeImuTranslation } from './math/orientation'
import { BlePoseClient, type PosePacket, type PoseStats } from './ble/ble-client'
import { BleControls } from './ui/BleControls'
import { ControlPanel } from './ui/ControlPanel'
import { HeadCameraView } from './ui/HeadCameraView'

const DEFAULT_DISTANCE_M = 0.6
const NEUTRAL_ORIENTATION = new Quaternion()
const DEFAULT_POSE_STATS: PoseStats = { packetRate: 0, droppedPackets: 0 }

function poseQuaternion(pose: PosePacket): Quaternion {
  const [w, x, y, z] = pose.quaternion
  return new Quaternion(x, y, z, w)
}

function App() {
  const client = useMemo(() => new BlePoseClient(), [])
  const [distanceM, setDistanceM] = useState(DEFAULT_DISTANCE_M)
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
      trackerReference.current ??= receivedOrientation
      const previous = lastTimestampMs.current
      lastTimestampMs.current = pose.timestampMs
      const rawDt = previous === null ? 0 : ((pose.timestampMs - previous) >>> 0) / 1000
      const dtS = rawDt > 0 && rawDt <= 0.1 ? rawDt : 0.02
      const world = tracker.update(receivedOrientation, new Vector3(...pose.accelMps2), dtS)
      setLivePosition(
        tracker.ready ? relativeImuTranslation(world, trackerReference.current) : null,
      )
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
  const screen = useMemo(() => createScreen({ distanceM }), [distanceM])
  const orientation = liveOrientation ?? NEUTRAL_ORIENTATION
  const handleCalibrate = () => {
    if (livePose) {
      setLiveReference(poseQuaternion(livePose))
      tracker.reset()
      trackerReference.current = poseQuaternion(livePose)
      setLivePosition(null)
    }
  }

  const handleReset = () => {
    setDistanceM(DEFAULT_DISTANCE_M)
    setLiveReference(livePose ? poseQuaternion(livePose) : null)
    tracker.reset()
    trackerReference.current = livePose ? poseQuaternion(livePose) : null
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
          <ControlPanel
            distanceM={distanceM}
            onDistanceChange={setDistanceM}
            onCalibrate={handleCalibrate}
            onReset={handleReset}
          />
        </div>
      </section>

      <HeadCameraView
        orientation={orientation}
        screen={screen}
        translation={livePosition ?? undefined}
      />
    </main>
  )
}

export default App
