# Simulator skill

Use this skill when working on the React/Vite application in `simulator/`.

## Goal

Make the simulator a practical visual reference for head-pose cursor behavior without coupling UI code to the math implementation.

## Workflow

1. Keep scene rendering under `simulator/src/scene/` and DOM controls under `simulator/src/ui/`.
2. Show the head origin, monitor plane, forward ray, and cursor previews before adding secondary polish.
3. Prefer controls that expose calibration and smoothing behavior directly.
4. Update `simulator/README.md` and root quick-start commands if the simulator developer workflow changes.
5. Validate with `npm run test`, `npm run build`, and `npm run lint` in `simulator/` before finishing UI-affecting work.

## Done when

- The app starts with Vite, renders a usable scene, and exposes the intended controls.
- Diagnostics reveal enough data to debug projection behavior.
- The simulator still depends only on the documented web stack.
