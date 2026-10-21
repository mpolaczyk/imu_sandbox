import { describe, expect, it } from 'vitest'
import { parsePoseFrame, POSE_FRAME_SIZE } from './ble-client'

describe('parsePoseFrame', () => {
  it('decodes the little-endian pose packet and normalizes Q14 orientation', () => {
    const bytes = new ArrayBuffer(POSE_FRAME_SIZE)
    const view = new DataView(bytes)
    view.setUint16(0, 0x1234, true)
    view.setUint32(2, 987654, true)
    view.setInt16(6, 0, true)
    view.setInt16(8, 0, true)
    view.setInt16(10, 16384, true)
    view.setInt16(12, 0, true)
    view.setInt16(14, -1000, true)
    view.setInt16(16, 250, true)
    view.setInt16(18, 0, true)

    expect(parsePoseFrame(view)).toMatchObject({
      sequence: 0x1234,
      timestampMs: 987654,
      quaternion: [0, 0, 1, 0],
      accelMps2: [-9.80665, 2.4516625, 0],
    })
  })

  it('rejects zero-length quaternions', () => {
    const bytes = new ArrayBuffer(POSE_FRAME_SIZE)
    const view = new DataView(bytes)
    view.setUint8(0, 1)
    expect(() => parsePoseFrame(view)).toThrow('zero-length quaternion')
  })
})
