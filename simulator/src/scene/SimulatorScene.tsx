import { OrbitControls, Line } from '@react-three/drei'
import { Canvas } from '@react-three/fiber'
import { useMemo } from 'react'
import { DoubleSide, Quaternion, Vector2, Vector3 } from 'three'
import type { CursorHit, Screen } from '../math/headPointer'
import { forwardVector, screenUvToWorld } from '../math/headPointer'

type SimulatorSceneProps = {
  orientation: Quaternion
  screen: Screen
  rawHit: CursorHit | null
  smoothedCursor: Vector2
}

const NOSE_ORIGIN = new Vector3(0, 0, 0)

function HeadModel({ orientation }: { orientation: Quaternion }) {
  return (
    <group quaternion={orientation}>
      <mesh position={[0, 0, -0.09]}>
        <sphereGeometry args={[0.11, 32, 32]} />
        <meshStandardMaterial color="#9ca3af" metalness={0.1} roughness={0.85} />
      </mesh>
      <mesh position={[0, 0, -0.005]}>
        <sphereGeometry args={[0.015, 16, 16]} />
        <meshStandardMaterial color="#f59e0b" />
      </mesh>
      <mesh position={[0, 0, -0.02]} rotation={[Math.PI / 2, 0, 0]}>
        <coneGeometry args={[0.025, 0.08, 20]} />
        <meshStandardMaterial color="#d1d5db" />
      </mesh>
    </group>
  )
}

function MonitorPlane({ screen }: { screen: Screen }) {
  return (
    <group position={screen.center.toArray()}>
      <mesh>
        <planeGeometry args={[screen.widthM, screen.heightM]} />
        <meshStandardMaterial
          color="#1f2937"
          transparent
          opacity={0.22}
          side={DoubleSide}
        />
      </mesh>
      <mesh position={[0, 0, 0.0005]}>
        <planeGeometry args={[screen.widthM, screen.heightM]} />
        <meshBasicMaterial color="#94a3b8" wireframe side={DoubleSide} />
      </mesh>
    </group>
  )
}

function SceneContents({ orientation, screen, rawHit, smoothedCursor }: SimulatorSceneProps) {
  const forward = useMemo(() => forwardVector(orientation), [orientation])
  const forwardTip = useMemo(() => NOSE_ORIGIN.clone().addScaledVector(forward, 0.2), [forward])
  const smoothedWorld = useMemo(
    () => screenUvToWorld(screen, smoothedCursor).add(new Vector3(0, 0, 0.004)),
    [screen, smoothedCursor],
  )

  return (
    <>
      <color attach="background" args={['#020617']} />
      <ambientLight intensity={0.75} />
      <directionalLight position={[1.5, 2.2, 1.4]} intensity={1.1} />
      <gridHelper args={[2, 20, '#1d4ed8', '#1f2937']} position={[0, -0.22, 0.5]} />
      <axesHelper args={[0.4]} />
      <HeadModel orientation={orientation} />
      <MonitorPlane screen={screen} />

      <Line points={[NOSE_ORIGIN, forwardTip]} color="#22c55e" lineWidth={3} />

      {rawHit !== null ? (
        <>
          <Line points={[NOSE_ORIGIN, rawHit.hitWorld]} color="#fb7185" lineWidth={2} />
          <mesh position={rawHit.hitWorld.toArray()}>
            <sphereGeometry args={[0.015, 24, 24]} />
            <meshStandardMaterial color="#fb7185" emissive="#7f1d1d" emissiveIntensity={0.4} />
          </mesh>
        </>
      ) : null}

      <mesh position={smoothedWorld.toArray()}>
        <sphereGeometry args={[0.012, 24, 24]} />
        <meshStandardMaterial color="#22d3ee" emissive="#164e63" emissiveIntensity={0.45} />
      </mesh>

      <OrbitControls makeDefault />
    </>
  )
}

export function SimulatorScene(props: SimulatorSceneProps) {
  return (
    <div className="scene-card">
      <div className="card-header">
        <div>
          <h2>3D simulator</h2>
          <p>Head origin, face-forward ray, and monitor intersection in metres.</p>
        </div>
      </div>
      <div className="scene-canvas">
        <Canvas camera={{ position: [0.8, 0.4, 1.1], fov: 45 }}>
          <SceneContents {...props} />
        </Canvas>
      </div>
    </div>
  )
}
