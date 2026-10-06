# Geant4 → WebGPU shadow physics integration

This stage connects the previously validated Geant4 deferred-tracking batch
boundary to the previously validated WebGPU Klein-Nishina batch sampler.

## Data path

```text
G4EventManager
    ↓
G4WgpuTrackingManager::HandOverOneTrack
    ↓
portable TrackBatch
    ↓
PhysicsBackend
    ├ CPU reference
    └ WebGpuBackend / WGSL
    ↓
shadow Klein-Nishina samples
    ↓
stored for validation only

Geant4 transport state
    ↓
existing default G4TrackingManager
```

The shadow result does not modify the Geant4 track, create secondaries, or
change transport decisions.

## Why shadow mode

This isolates two questions:

1. Can real Geant4 tracks be converted into the portable batch contract and
   passed through the GPU physics backend?
2. Can that be done without changing the reference Geant4 simulation result?

Only after both are demonstrated should GPU results become authoritative
transport state.

## API

`G4WgpuTrackingManager` accepts an optional non-owning `PhysicsBackend*`.
When present, each flushed gamma batch is sampled with
`sample_klein_nishina_batch()` before the original Geant4 CPU fallback runs.

The latest shadow samples are available through
`last_shadow_samples()` for validation and profiling.

## Validation

The combined CI configuration enables both:

- `G4WGPU_ENABLE_GEANT4=ON`
- `G4WGPU_ENABLE_WEBGPU=ON`

and executes a one-event gamma simulation on Mesa Vulkan.

The test requires:

- Geant4 event completion,
- one flushed portable gamma track,
- one accepted WebGPU Klein-Nishina sample,
- physically valid sampled scattered energy,
- zero tracks left buffered at event completion.

## Important limitation

A gamma track being eligible for the shadow sampler does **not** mean a
Compton interaction actually occurred in Geant4. At this stage the sampler is
used to validate end-to-end plumbing only.

The next physics-integration milestone must add interaction selection,
material/cross-section handling, and a controlled mechanism for promoting GPU
results into Geant4 track/secondary state.
