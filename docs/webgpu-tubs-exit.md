# Experimental WebGPU Tubs DistanceToOut

This is a bounded extension of the **solid, full-azimuth** Tubs geometry
experiment (zero inner radius, no angular sector). It executes the
`tubs_exit_main` WGSL compute shader for local-coordinate rays that
start inside or on the radial cylinder and within its axial end caps.

The output is the first outward intersection of the curved radial wall or
either z end cap, in mm. Outward rays from a surface return zero. Rays
perpendicular to z use only the radial crossing; axial rays use the end
cap. The GPU uses f32 and the CPU reference uses f64; comparisons use
explicit absolute/relative tolerances across 0.1x, 1x, and 100x geometry
scales and multiple 128-thread workgroups.

The host **first validates f64 ray origins and directions using
`tubs_distance_to_out_mm`**, not the enclosing Box containment predicate:
the Box contains points that lie outside the round cylinder. The host
therefore duplicates CPU distance computations for correctness before
GPU staging; this is not yet a performance-optimized or authoritative
navigator. Unsupported f32 input ranges and invalid geometry are rejected.

Existing Geant4 `G4Tubs` comparisons apply to the portable CPU
reference. This PR changes neither Geant4 navigation, tracking, RNG, nor
physics interactions. Near-tangent and very large dynamic-range precision
will require additional focused validation prior to any promotion.
