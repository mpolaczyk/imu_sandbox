import { useEffect, useMemo, useState } from 'react'
import { Vector2, Vector3 } from 'three'
import './App.css'
import { ema } from './math/filters'
import { createScreen, forwardVector, intersectNoseRayWithScreen } from './math/headPointer'
import { headQuaternion } from './math/orientation'
import { SimulatorScene } from './scene/SimulatorScene'
import { applyCalibration, type Angles } from './simulation/calibration'
import { ControlPanel } from './ui/ControlPanel'
import { Diagnostics } from './ui/Diagnostics'
import { ScreenPreview } from './ui/ScreenPreview'

const DEFAULT_ANGLES: Angles = { yawDeg: 0, pitchDeg: 0, rollDeg: 0 }
const DEFAULT_DISTANCE_M = 0.6
const DEFAULT_SMOOTHING_ALPHA = 0.22
const DEFAULT_CURSOR = new Vector2(0.5, 0.5)
const NOSE_ORIGIN = new Vector3(0, 0, 0)

function App() {
  const [angles, setAngles] = useState<Angles>(DEFAULT_ANGLES)
  const [distanceM, setDistanceM] = useState(DEFAULT_DISTANCE_M)
  const [smoothingAlpha, setSmoothingAlpha] = useState(DEFAULT_SMOOTHING_ALPHA)
  const [calibration, setCalibration] = useState<Angles>(DEFAULT_ANGLES)
  const [smoothedCursor, setSmoothedCursor] = useState(() => DEFAULT_CURSOR.clone())

  const effectiveAngles = useMemo(() => applyCalibration(angles, calibration), [angles, calibration])
  const screen = useMemo(() => createScreen({ distanceM }), [distanceM])
  const orientation = useMemo(
    () => headQuaternion(effectiveAngles.yawDeg, effectiveAngles.pitchDeg, effectiveAngles.rollDeg),
    [effectiveAngles],
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
    setCalibration(angles)
    setSmoothedCursor(DEFAULT_CURSOR.clone())
  }

  const handleReset = () => {
    setAngles(DEFAULT_ANGLES)
    setDistanceM(DEFAULT_DISTANCE_M)
    setSmoothingAlpha(DEFAULT_SMOOTHING_ALPHA)
    setCalibration(DEFAULT_ANGLES)
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
