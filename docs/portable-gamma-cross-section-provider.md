# Portable gamma cross-section provider

The portable transport layer now exposes one provider that assembles all
three macroscopic gamma EM cross sections used by process competition:

- Compton scattering,
- photoelectric absorption,
- electron-positron pair production.

Compton and pair production are evaluated from the portable material
composition. Photoelectric evaluation uses the portable Sandia polynomial,
with the active Sandia segment still selected by the host-side Geant4 adapter.

This consolidates the cross-section inputs behind a single backend-neutral
function so CPU, WebGPU, and Geant4 shadow paths can share the same process
competition contract.

The provider remains observational in the current Geant4 integration. CPU
Geant4 transport is still authoritative.
