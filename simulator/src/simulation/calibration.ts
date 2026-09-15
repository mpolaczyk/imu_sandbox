export type Angles = {
  yawDeg: number
  pitchDeg: number
  rollDeg: number
}

export function applyCalibration(angles: Angles, offsets: Angles): Angles {
  return {
    yawDeg: angles.yawDeg - offsets.yawDeg,
    pitchDeg: angles.pitchDeg - offsets.pitchDeg,
    rollDeg: angles.rollDeg - offsets.rollDeg,
  }
}
