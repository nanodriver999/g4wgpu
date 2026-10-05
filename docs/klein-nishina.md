# Klein-Nishina kernel

This is the first physics-level numerical kernel in g4wgpu.

## Scope

The implementation samples Compton scattering on a free electron using the
Klein-Nishina distribution.

It deliberately does not depend on:

- `G4Track`
- `G4DynamicParticle`
- `G4ParticleChangeForGamma`
- Geant4 material/object graphs

The numerical interface operates on explicit MeV energies and a counter RNG
address.

## Why this boundary

The target architecture is:

```text
Geant4 objects
    ↓ conversion
portable POD batch
    ↓
physics sampler
    ├ CPU reference (double)
    └ WGSL implementation (f32)
    ↓
portable result
    ↓ conversion
Geant4 state update
```

This keeps Geant4 orchestration on the CPU while allowing many independent
scatter samples to execute as a GPU batch.

## Reference model

The repository's Geant4 fork contains
`G4KleinNishinaCompton::SampleSecondaries`, which uses a rejection sampler
for the Klein-Nishina distribution and cites the Butcher-Messel random-number
technique.

g4wgpu keeps an independently structured backend-neutral implementation rather
than copying Geant4 object-management code.

## Current validation

The numerical CPU test checks multiple incident energies and verifies:

- accepted sample within the iteration bound
- scattered photon energy range
- recoil electron energy range
- energy conservation
- cosine/sine physical range
- direction-angle normalization
- azimuth range

These are invariant tests, not yet a distribution-equivalence test.

## Required before claiming Geant4 equivalence

Later validation must compare large CPU/GPU samples against an accepted
reference distribution or Geant4 reference run, including:

- scattered-energy histograms
- angular distributions
- mean energy transfer
- high/low incident energy cases
- rejection iteration behavior
- multiple seeds/streams

The portable WGSL path uses f32, while the CPU reference uses double. Expected
precision differences must therefore be quantified statistically rather than
hidden.
