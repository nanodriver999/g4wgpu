# Geant4 shadow process competition

The Geant4 tracking adapter now records an observational gamma-process
competition sample for every buffered gamma when shadow physics is enabled.

For each track, the adapter:

1. inspects only material information that is already safe to access,
2. converts an available Geant4 material into portable inputs,
3. evaluates portable Compton, photoelectric Sandia, and pair-production
   macroscopic cross sections,
4. samples the total interaction distance and winning process through the
   backend-neutral competition algorithm,
5. records a no-interaction shadow sample when the custom tracking hand-off
   happens before Geant4 has initialized a step/material,
6. keeps the existing Klein-Nishina shadow sample for validation,
7. leaves authoritative Geant4 transport unchanged.

A custom tracking manager receives primary tracks before
`G4TrackingManager::ProcessOneTrack()` initializes the first `G4Step`.
Therefore this path must not call `G4Track::GetMaterial()` unless
`GetStep()` is non-null. This guard is part of the shadow contract, not a
physics approximation.

The competition RNG uses a copy of the track's stable RNG address, so this
validation path does not modify the RNG stream passed to the existing
Klein-Nishina shadow backend.

This remains a shadow stage because material/Sandia preprocessing is still
performed from Geant4 objects and no sampled interaction is promoted into
transport state. The numerical competition inputs themselves are now portable.
