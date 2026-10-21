import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { useMemo } from 'react'
import { Quaternion, Vector3 } from 'three'
import { eyePosition, offAxisProjection } from '../math/headCamera'
import type { Screen } from '../math/headPointer'

type HeadCameraViewProps = {
  orientation: Quaternion
  screen: Screen
  translation?: Vector3
}

function HeadCamera({ orientation, screen, translation }: HeadCameraViewProps) {
  const camera = useThree((state) => state.camera)
  const eye = useMemo(() => eyePosition(orientation, translation), [orientation, translation])

  useFrame(() => {
    const projection = offAxisProjection(eye, screen)
    if (projection === null) {
      return
    }
    camera.position.copy(eye)
    camera.quaternion.identity()
    camera.updateMatrixWorld(true)
    camera.projectionMatrix.copy(projection)
    camera.projectionMatrixInverse.copy(projection).invert()
  })

  return null
}

const BOXES: Array<{ position: [number, number, number]; size: number; color: string }> = [
  { position: [-0.12, -0.05, -0.45], size: 0.08, color: '#f97316' },
  { position: [0.1, 0.02, -0.7], size: 0.12, color: '#22d3ee' },
  { position: [0, -0.06, -0.95], size: 0.16, color: '#a78bfa' },
  { position: [0.2, 0.05, -0.5], size: 0.05, color: '#facc15' },
  { position: [-0.22, 0.04, -0.85], size: 0.1, color: '#4ade80' },
  { position: [0.02, 0.05, -0.3], size: 0.04, color: '#fb7185' },
]

function World({ screen }: { screen: Screen }) {
  const frameZ = screen.center.z
  return (
    <>
      <color attach="background" args={['#020617']} />
      <ambientLight intensity={0.8} />
      <directionalLight position={[1, 2, 1]} intensity={1.2} />
      <gridHelper args={[3, 30, '#1d4ed8', '#1f2937']} position={[0, -screen.heightM / 2, -1]} />
      <mesh position={[0, 0, frameZ]}>
        <planeGeometry args={[screen.widthM, screen.heightM]} />
        <meshBasicMaterial color="#38bdf8" wireframe />
      </mesh>
      {BOXES.map((box) => (
        <mesh key={box.color} position={box.position}>
          <boxGeometry args={[box.size, box.size, box.size]} />
          <meshStandardMaterial color={box.color} />
        </mesh>
      ))}
    </>
  )
}

export function HeadCameraView({ orientation, screen, translation }: HeadCameraViewProps) {
  const aspect = screen.widthM / screen.heightM
  return (
    <section className="card preview-card">
      <div className="card-header">
        <div>
          <h2>Head-coupled view</h2>
          <p>
            Virtual camera follows the sensor; the off-axis projection makes the screen act as a
            window into the scene.
          </p>
        </div>
      </div>
      <div className="screen-preview" style={{ aspectRatio: String(aspect) }}>
        <Canvas camera={{ manual: true, position: [0, 0, 0] }}>
          <HeadCamera orientation={orientation} screen={screen} translation={translation} />
          <World screen={screen} />
        </Canvas>
      </div>
    </section>
  )
}
