# Portable axis-aligned Box CPU reference

This is the first independent geometry primitive for the incremental
Geant4-to-WebGPU migration in `docs/implementation-plan.md`.

- Inputs are a box centered at the local origin, its positive half lengths,
  and a ray expressed in the same local coordinates (all positions in mm).
- Direction vectors are normalized internally; ray distances are returned in mm.
- `box_distance_to_in_mm` uses slab intersections. Missing rays return
  positive infinity; points already on/inside the box have zero entry distance.
- `box_distance_to_out_mm` requires a point on/inside the box.
- Safety functions use conservative axis-aligned clearances, not Euclidean
  corner distance; these match the Geant4 Box safety contract.
- Non-finite inputs, invalid box extents and zero direction are rejected.

CPU numerical tests cover hit/miss, scale invariance of direction, surface
exit and invalid input. Geant4 integration compares non-degenerate axis-aligned
rays and safety calculations against `G4Box`, using explicit tolerances.

This PR does **not** replace Geant4 navigation and does **not** claim GPU
geometry execution or speedup. The next bounded step is a WGSL Box kernel
with CPU/Geant4 differential tests, keeping CPU fallback authoritative.
