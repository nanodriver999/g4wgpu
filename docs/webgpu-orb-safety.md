# Experimental WebGPU Orb safety

This opt-in experiment adds only radial `SafetyToIn` and `SafetyToOut`
for an origin-centered full sphere (`PortableOrb`). The two WGSL entry
points use `max(0, length(position) - radius)` and
`max(0, radius - length(position))`. Results are millimeters, computed
in f32 and checked against the independent f64 CPU reference.

The implementation reuses the Box staging/dispatch path (the radius is
stored in the first half-extent parameter; the other extents are unused).
Input validation rejects nonpositive or nonfinite radius, nonfinite
coordinates and f32 overflow. The fixed ray direction in the staging
buffer is ignored by the Orb shaders.

The software-Vulkan integration test compares 652 positions across
multiple workgroups, including inside/outside, face, near-boundary,
diagonal and invalid-input cases. This does not implement Orb ray
intersection, Geant4 navigation or a measured performance improvement.
Geant4 CPU fallback and RNG/physics calculations remain unchanged.

## Orb ray DistanceToIn (incremental GPU kernel)

A standalone `orb_distance_to_in_batch` operation now dispatches
`orb_entry_main` through the same verified WebGPU input/output staging
path. It computes full-sphere entry distance with the near-root quadratic
formula, returns zero for inside/on-surface rays and positive infinity
for outward, missing, or tangent external rays. Rays are normalized on
the CPU in f64 before staging to f32, and invalid/nonfinite input is
rejected before GPU execution.

The software-Vulkan test compares >640 varied hit/miss/inside rays against
the portable `orb_distance_to_in_mm` f64 reference across several GPU
workgroups. Test tolerances explicitly allow f32 rounding. Boundary
classification for nearly grazing rays requires additional focused tests
before any navigation promotion.

This does not change Geant4's authoritative navigator, tracking,
physics, RNG or CPU fallback, and it does not claim a speedup.

## Orb ray DistanceToOut (incremental GPU kernel)

A further standalone, opt-in `orb_distance_to_out_batch` operation now
dispatches `orb_exit_main` using the existing f32 ray buffers and GPU
readback. The shader computes the positive root of the full-sphere
ray-boundary quadratic and returns zero for outward rays starting on the
surface. A zero-length or nonfinite ray direction, invalid radius, or
point outside the sphere is rejected on the host.

Crucially, the host validates the original f64 input using
`orb_distance_to_out_mm`, **not** a Box-only containment check; points
outside Orb but inside its surrounding Box are invalid. The new
software-Vulkan integration test compares more than 650 non-grazing
interior/surface rays with the independent CPU reference, including
non-unit directions and explicit f32 error bounds.

The operation remains advisory: no Geant4 navigator promotion, CPU
fallback removal, physics changes, RNG changes, or speedup claim.
