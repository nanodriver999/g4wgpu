# Geant4 shadow process competition

The Geant4 tracking adapter now records an observational gamma-process
competition sample for every buffered gamma when shadow physics is enabled.

For each track, the adapter:

1. reads the configured Geant4 material,
2. converts the Geant4 material into portable inputs,
3. evaluates portable Compton, photoelectric Sandia, and pair-production
   macroscopic cross sections,
4. samples the total interaction distance and winning process through the
   backend-neutral competition algorithm,
5. keeps the existing Klein-Nishina shadow sample for validation,
6. leaves authoritative Geant4 transport unchanged.

The competition RNG uses a copy of the track's stable RNG address, so this
validation path does not modify the RNG stream passed to the existing
Klein-Nishina shadow backend.

This remains a shadow stage because material/Sandia preprocessing is still
performed from Geant4 objects and no sampled interaction is promoted into
transport state. The numerical competition inputs themselves are now portable.
