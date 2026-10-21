import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { useMemo, useRef } from 'react'
import { BackSide, type DirectionalLight, type Quaternion } from 'three'
import { eyePosition, offAxisProjection } from '../math/headCamera'
import type { Screen } from '../math/headPointer'

type HeadCameraViewProps = {
  orientation: Quaternion
  screen: Screen
}

function HeadCamera({ orientation, screen }: HeadCameraViewProps) {
  const camera = useThree((state) => state.camera)
  const eye = useMemo(() => eyePosition(orientation), [orientation])
  const headlight = useRef<DirectionalLight>(null)

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

    const light = headlight.current
    if (light) {
      light.position.copy(eye)
      light.target.position.set(eye.x, eye.y, eye.z - 1)
      light.target.updateMatrixWorld()
    }
  })

  return (
    <directionalLight
      ref={headlight}
      intensity={0.9}
      castShadow
      shadow-mapSize={[2048, 2048]}
      shadow-camera-left={-0.4}
      shadow-camera-right={0.4}
      shadow-camera-top={0.4}
      shadow-camera-bottom={-0.4}
      shadow-camera-near={0.01}
      shadow-camera-far={1.5}
      shadow-bias={-0.0002}
      shadow-normalBias={0.002}
      shadow-radius={4}
    />
  )
}

const ROOM_DEPTH_M = 0.5
const WALL_WHITE = '#9ca3af'
// BoxGeometry face order: +x, -x, +y, -y, +z, -z. The +z face (towards the viewer) is culled, leaving the screen as an opening.
const WALL_COLORS = ['#22c55e', '#ef4444', WALL_WHITE, WALL_WHITE, WALL_WHITE, WALL_WHITE]

type Shape =
  | { kind: 'sphere'; radius: number }
  | { kind: 'cone'; radius: number; height: number }
  | { kind: 'cylinder'; radius: number; height: number }
  | { kind: 'torusKnot'; radius: number; tube: number }
  | { kind: 'box'; width: number; height: number; depth: number }

type RoomObject = {
  x: number
  z: number
  /** Centre height above the floor. */
  lift: number
  rotY: number
  color: string
  shape: Shape
}

// x/z are relative to the room centre.
const ROOM_OBJECTS: RoomObject[] = [
  { x: -0.17, z: -0.1, lift: 0.08, rotY: 0, color: '#f97316', shape: { kind: 'cone', radius: 0.06, height: 0.16 } },
  { x: 0.0, z: -0.14, lift: 0.055, rotY: 0, color: '#eab308', shape: { kind: 'cylinder', radius: 0.045, height: 0.11 } },
  { x: 0.15, z: -0.02, lift: 0.07, rotY: 0, color: '#3b82f6', shape: { kind: 'sphere', radius: 0.07 } },
  { x: -0.05, z: 0.06, lift: 0.07, rotY: 0.5, color: '#a855f7', shape: { kind: 'torusKnot', radius: 0.04, tube: 0.013 } },
  { x: 0.1, z: 0.12, lift: 0.03, rotY: -0.4, color: '#14b8a6', shape: { kind: 'box', width: 0.08, height: 0.06, depth: 0.08 } },
]

function ShapeGeometry({ shape }: { shape: Shape }) {
  switch (shape.kind) {
    case 'sphere':
      return <sphereGeometry args={[shape.radius, 48, 32]} />
    case 'cone':
      return <coneGeometry args={[shape.radius, shape.height, 48]} />
    case 'cylinder':
      return <cylinderGeometry args={[shape.radius, shape.radius, shape.height, 48]} />
    case 'torusKnot':
      return <torusKnotGeometry args={[shape.radius, shape.tube, 160, 24]} />
    case 'box':
      return <boxGeometry args={[shape.width, shape.height, shape.depth]} />
  }
}

function World({ screen }: { screen: Screen }) {
  const { widthM: w, heightM: h, center } = screen
  const d = ROOM_DEPTH_M
  const roomZ = center.z - d / 2
  const floorY = center.y - h / 2
  return (
    <>
      <color attach="background" args={['#020617']} />
      <ambientLight intensity={0.2} />
      <mesh position={[center.x, center.y, roomZ]} receiveShadow>
        <boxGeometry args={[w, h, d]} />
        {WALL_COLORS.map((color, index) => (
          <meshPhongMaterial
            key={index}
            attach={`material-${index}`}
            color={color}
            specular="#222222"
            shininess={16}
            side={BackSide}
          />
        ))}
      </mesh>
      {ROOM_OBJECTS.map((object, index) => (
        <mesh
          key={index}
          position={[center.x + object.x, floorY + object.lift, roomZ + object.z]}
          rotation={[0, object.rotY, 0]}
          castShadow
          receiveShadow
        >
          <ShapeGeometry shape={object.shape} />
          <meshPhongMaterial color={object.color} specular="#aaaaaa" shininess={64} />
        </mesh>
      ))}
    </>
  )
}

export function HeadCameraView({ orientation, screen }: HeadCameraViewProps) {
  const aspect = screen.widthM / screen.heightM
  return (
    <section className="preview-card">
      <div className="screen-preview" style={{ aspectRatio: String(aspect) }}>
        <Canvas shadows="soft" camera={{ manual: true, position: [0, 0, 0] }}>
          <HeadCamera orientation={orientation} screen={screen} />
          <World screen={screen} />
        </Canvas>
      </div>
    </section>
  )
}
