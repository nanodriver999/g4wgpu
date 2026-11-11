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
