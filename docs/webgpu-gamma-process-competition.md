# WebGPU gamma process competition

The physics backend now exposes a batched gamma process-competition API.

For each track, the backend receives the macroscopic cross sections for:

- Compton scattering,
- photoelectric absorption,
- pair production.

The CPU backend evaluates the existing reference selector directly. The
WebGPU backend uploads the three cross sections plus stable RNG addresses and
executes the same exponential free-path sampling and proportional process
selection in WGSL.

The WGSL path uses the same counter-based RNG address contract as the CPU
reference and writes the updated counter back with each result.

This PR validates CPU/WebGPU agreement but does not yet route Geant4 shadow
competition through the backend. That wiring is a separate integration step.
