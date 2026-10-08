# Portable gamma cross sections in Geant4 shadow mode

The portable gamma transport path now assembles all three macroscopic process
cross sections used by process competition:

- Compton scattering,
- photoelectric absorption,
- electron-positron pair production.

Compton and pair-production are evaluated from the portable material
composition. Photoelectric evaluation uses the portable Sandia polynomial,
with the active Sandia segment still selected by the Geant4 host adapter.

The custom tracking manager records a shadow process-competition sample for
each buffered gamma before the existing Klein-Nishina shadow sampler runs.
This remains observational only: Geant4's normal CPU transport is still
authoritative and no track state is changed by the competition result.

A follow-up stage can use the selected process to gate the corresponding GPU
final-state sampler instead of running Compton sampling for every gamma.
