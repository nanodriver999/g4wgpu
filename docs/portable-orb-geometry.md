# Portable Orb geometry reference

The second geometry primitive in the Geant4-to-WebGPU implementation plan is
a **full sphere** (Geant4 `G4Orb`), not an angularly clipped `G4Sphere`.

This change implements portable, Geant4-independent CPU calculations of ray
distance to enter/leave and radial safety. Positions, radii and output
distances are in millimeters. Rays use local coordinates and their directions
are normalized internally. A ray that does not cross into the sphere returns
positive infinity; grazing/tangent external rays are treated as misses.

A numerically stable near-root form is used for external ray entry to reduce
loss of precision when rays start far away. Distances to exit require
an origin on/inside the Orb, and input geometry and ray values are validated.

The new CPU regression covers normalized/non-unit directions, entry/exit,
outward/inward surface rays, misses, safety and invalid inputs. Geant4
integration compares non-grazing rays, including 1-µm-near-boundary examples,
against `G4Orb` using explicit floating-point tolerances.

**Scope boundary:** no GPU geometry kernel, `G4Navigator` replacement,
Geant4 tracking/CPU fallback, or RNG/physics behavior changes in this PR.
Future WGSL implementation must be tested against these CPU and G4Orb
references before accelerated navigation can become authoritative.
