# Portable solid Tubs geometry CPU reference

This is the first `G4Tubs` subset in the `docs/implementation-plan.md`
geometry primitive milestone: a **solid cylinder** (`rmin = 0`), centered in
local coordinates, with full azimuth (`deltaPhi = 2*pi`). Annular tubes and
phi-sector clipping are intentionally deferred to separate PRs.

The portable POD specifies the outer radius and half-length along z, in mm.
Ray directions are normalized internally. DistanceToIn intersects the
infinite radial cylinder with the two z end-cap planes. An external ray that
misses or is tangent returns positive infinity. DistanceToOut requires an
origin inside/on the cylinder and returns the first positive radial/end-cap
boundary. SafetyToIn uses a conservative maximum of positive radial and
axial excesses (not the Euclidean corner distance); SafetyToOut uses the
minimum radial/end-cap clearance.

CPU regression tests cover radial and axial entry/exit, non-unit directions,
compound end-cap/radial entry, surface directions, misses, safety, and input
validation. A Geant4 integration test compares nondegenerate rays with
`G4Tubs(0, rmax, halfZ, 0, 2*pi)`, including near-boundary samples.

**No accelerated geometry ownership in this PR:** the CPU reference does not
change Geant4 navigation, track state, current CPU fallback, shaders, or
physics/RNG. A later PR may port the verified algorithm to WGSL with numerical
differential tests and documented f32 tolerances; no speedup is claimed.
