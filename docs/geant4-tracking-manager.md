# Geant4 custom tracking manager integration

PR #6 introduces the first real Geant4 integration boundary.

## Integration point

Geant4's `G4EventManager` asks the particle definition for a
`G4VTrackingManager`. If present, ownership of the track is handed to that
manager via `HandOverOneTrack()`. Processing may be deferred until
`FlushEvent()`.

g4wgpu uses exactly that extension point.

```text
G4EventManager
    ↓
G4WgpuTrackingManager::HandOverOneTrack
    ├ unsupported / below threshold → default G4TrackingManager
    └ gamma candidate → buffer
                         ↓
                    batch capacity or
                    FlushEvent
                         ↓
                    TrackBatch snapshot
                         ↓
                    default G4TrackingManager
                    (PR #6 fallback)
```

## Why CPU fallback first

This PR does **not** replace Geant4 transport with GPU transport yet.

The goal is to prove that:

- deferred ownership is handled correctly
- candidate tracks can be converted to portable SoA data
- event/track identity can seed a stable RNG stream address
- secondaries are returned to the Geant4 event stack
- unsupported tracks continue through the existing CPU tracking path

Only after this lifecycle boundary is validated will a later PR consume the
portable batch with WebGPU physics kernels.

## Portable batch mapping

For each buffered track:

- position → mm
- direction → dimensionless unit vector
- kinetic energy → MeV
- particle ID → PDG encoding
- material ID → Geant4 material-table index
- RNG stream low → Geant4 track ID
- RNG stream high → Geant4 event ID
- RNG counter → zero at first hand-over

The run ID is not yet part of the RNG stream identity. This is sufficient for
within-event development but must be extended before cross-run reproducibility
is claimed.

## Current fallback limitation

The deferred fallback currently accepts terminal statuses:

- `fStopAndKill`
- `fKillTrackAndSecondaries`

Other terminal/suspended statuses are rejected until their ownership semantics
are explicitly implemented and tested.

## Build

Geant4 integration is opt-in:

```bash
cmake -S . -B build-g4 \
  -DG4WGPU_ENABLE_GEANT4=ON \
  -DGeant4_DIR=/path/to/Geant4Config.cmake
```

The core project remains buildable without Geant4.
