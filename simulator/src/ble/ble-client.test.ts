import { describe, expect, it } from 'vitest'
import { parsePoseFrame, POSE_FRAME_SIZE } from './ble-client'

describe('parsePoseFrame', () => {
  it('decodes the little-endian pose packet and normalizes Q14 orientation', () => {
    const bytes = new ArrayBuffer(POSE_FRAME_SIZE)
    const view = new DataView(bytes)
    view.setUint8(0, 1)
    view.setUint8(1, 3)
    view.setUint16(2, 0x1234, true)
    view.setUint32(4, 987654, true)
    view.setInt16(8, 0, true)
    view.setInt16(10, 0, true)
    view.setInt16(12, 16384, true)
    view.setInt16(14, 0, true)
    view.setInt16(16, -1234, true)
    view.setInt16(18, 250, true)
    view.setInt16(20, 0, true)

    expect(parsePoseFrame(view)).toEqual({
      version: 1,
      flags: 3,
      sequence: 0x1234,
      timestampMs: 987654,
      orientationValid: true,
      buttonPressed: true,
      quaternion: [0, 0, 1, 0],
      gyroDps: [-12.34, 2.5, 0],
    })
  })

  it('rejects unsupported versions and zero-length quaternions', () => {
    const bytes = new ArrayBuffer(POSE_FRAME_SIZE)
    const view = new DataView(bytes)
    view.setUint8(0, 2)
    expect(() => parsePoseFrame(view)).toThrow('Unsupported pose protocol version')
    view.setUint8(0, 1)
    expect(() => parsePoseFrame(view)).toThrow('zero-length quaternion')
  })
})
