# Geant4 shadow process competition through PhysicsBackend

The custom Geant4 tracking manager now routes gamma process competition
through the configured `PhysicsBackend` instead of evaluating the selector
directly in the Geant4 integration layer.

The host side still prepares portable macroscopic cross sections for each
buffered gamma:

- Compton scattering,
- photoelectric absorption,
- pair production.

It then passes the cross-section batch and stable RNG addresses to
`sample_gamma_process_competition_batch()`.

With `CpuBackend`, this executes the CPU reference selector. With
`WebGpuBackend`, the same batch is dispatched to the WGSL process-competition
kernel before the existing WebGPU Klein-Nishina shadow sampler runs.

This remains shadow-only. Geant4 CPU transport remains authoritative and the
selected GPU process does not yet gate or mutate final-state transport.
