import { useEffect, useMemo, useState } from 'react'
import { Euler, MathUtils, Quaternion, Vector2, Vector3 } from 'three'
import './App.css'
import { ema } from './math/filters'
import { createScreen, forwardVector, intersectNoseRayWithScreen } from './math/headPointer'
import { headQuaternion, relativeImuOrientation } from './math/orientation'
import { BlePoseClient, type PosePacket, type PoseStats } from './ble/ble-client'
import { SimulatorScene } from './scene/SimulatorScene'
import { applyCalibration, type Angles } from './simulation/calibration'
import { BleControls } from './ui/BleControls'
import { ControlPanel } from './ui/ControlPanel'
import { Diagnostics } from './ui/Diagnostics'
import { ScreenPreview } from './ui/ScreenPreview'

const DEFAULT_ANGLES: Angles = { yawDeg: 0, pitchDeg: 0, rollDeg: 0 }
const DEFAULT_DISTANCE_M = 0.6
const DEFAULT_SMOOTHING_ALPHA = 0.22
const DEFAULT_CURSOR = new Vector2(0.5, 0.5)
const NOSE_ORIGIN = new Vector3(0, 0, 0)
const DEFAULT_POSE_STATS: PoseStats = { packetRate: 0, droppedPackets: 0 }

function quaternionToAngles(quaternion: Quaternion): Angles {
  const euler = new Euler().setFromQuaternion(quaternion, 'YXZ')
  return {
    yawDeg: MathUtils.radToDeg(euler.y),
    pitchDeg: MathUtils.radToDeg(euler.x),
    rollDeg: MathUtils.radToDeg(euler.z),
  }
}

function poseQuaternion(pose: PosePacket): Quaternion {
  const [w, x, y, z] = pose.quaternion
  return new Quaternion(x, y, z, w)
}

function App() {
  const client = useMemo(() => new BlePoseClient(), [])
  const [angles, setAngles] = useState<Angles>(DEFAULT_ANGLES)
  const [distanceM, setDistanceM] = useState(DEFAULT_DISTANCE_M)
  const [smoothingAlpha, setSmoothingAlpha] = useState(DEFAULT_SMOOTHING_ALPHA)
  const [calibration, setCalibration] = useState<Angles>(DEFAULT_ANGLES)
  const [smoothedCursor, setSmoothedCursor] = useState(() => DEFAULT_CURSOR.clone())
  const [livePose, setLivePose] = useState<PosePacket | null>(null)
  const [liveReference, setLiveReference] = useState<Quaternion | null>(null)
  const [bleConnected, setBleConnected] = useState(false)
  const [bleStats, setBleStats] = useState(DEFAULT_POSE_STATS)
  const [bleError, setBleError] = useState('')

  useEffect(() => {
    const unsubscribePose = client.onPose((pose, stats) => {
      setLivePose(pose)
      const receivedOrientation = poseQuaternion(pose)
      setLiveReference((reference) => reference ?? receivedOrientation)
      setBleStats(stats)
    })
    const unsubscribeConnect = client.onConnect(() => {
      setBleConnected(true)
      setLivePose(null)
      setLiveReference(null)
      setBleError('')
    })
    const unsubscribeDisconnect = client.onDisconnect(() => {
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
  }, [client])

  const liveOrientation = useMemo(() => {
    if (!livePose || !liveReference) {
      return null
    }
    const deviceOrientation = poseQuaternion(livePose)
    return relativeImuOrientation(deviceOrientation, liveReference)
  }, [livePose, liveReference])
  const effectiveAngles = useMemo(
    () => (liveOrientation ? quaternionToAngles(liveOrientation) : applyCalibration(angles, calibration)),
    [liveOrientation, angles, calibration],
  )
  const screen = useMemo(() => createScreen({ distanceM }), [distanceM])
  const orientation = useMemo(
    () =>
      liveOrientation
        ? liveOrientation
        : headQuaternion(effectiveAngles.yawDeg, effectiveAngles.pitchDeg, effectiveAngles.rollDeg),
    [liveOrientation, effectiveAngles],
  )
  const rawHit = useMemo(
    () => intersectNoseRayWithScreen(NOSE_ORIGIN, orientation, screen),
    [orientation, screen],
  )
  const rawCursor = useMemo(
    () => (rawHit === null ? null : new Vector2(rawHit.u, rawHit.v)),
    [rawHit],
  )
  const forward = useMemo(() => forwardVector(orientation), [orientation])

  useEffect(() => {
    if (rawCursor === null) {
      return
    }

    const frame = window.requestAnimationFrame(() => {
      setSmoothedCursor((previous) => ema(previous, rawCursor, smoothingAlpha))
    })

    return () => window.cancelAnimationFrame(frame)
  }, [rawCursor, smoothingAlpha])

  const handleCalibrate = () => {
    if (livePose) {
      setLiveReference(poseQuaternion(livePose))
    } else {
      setCalibration(angles)
    }
    setSmoothedCursor(DEFAULT_CURSOR.clone())
  }

  const handleReset = () => {
    setAngles(DEFAULT_ANGLES)
    setDistanceM(DEFAULT_DISTANCE_M)
    setSmoothingAlpha(DEFAULT_SMOOTHING_ALPHA)
    setCalibration(DEFAULT_ANGLES)
    setLiveReference(livePose ? poseQuaternion(livePose) : null)
    setSmoothedCursor(DEFAULT_CURSOR.clone())
  }

  return (
    <main className="app-shell">
      <section className="hero-card card">
        <p className="eyebrow">imu_sandbox · simulator</p>
        <h1>Head-pose cursor math simulator</h1>
        <p className="hero-copy">
          This reference UI visualizes the face-forward ray, monitor intersection, and raw vs.
          smoothed cursor output for an eventual XIAO BLE wearable controller.
        </p>
      </section>

      <section className="layout-grid">
        <SimulatorScene
          orientation={orientation}
          screen={screen}
          rawHit={rawHit}
          smoothedCursor={smoothedCursor}
        />
        <div className="sidebar-stack">
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
            angles={angles}
            distanceM={distanceM}
            smoothingAlpha={smoothingAlpha}
            calibration={calibration}
            onAnglesChange={setAngles}
            onDistanceChange={setDistanceM}
            onSmoothingChange={setSmoothingAlpha}
            onCalibrate={handleCalibrate}
            onReset={handleReset}
          />
          <Diagnostics
            screen={screen}
            effectiveAngles={effectiveAngles}
            rawHit={rawHit}
            orientation={orientation}
            forward={forward}
          />
        </div>
      </section>

      <ScreenPreview
        rawCursor={rawCursor}
        smoothedCursor={smoothedCursor}
        hasIntersection={rawHit !== null}
      />
    </main>
  )
}

export default App
