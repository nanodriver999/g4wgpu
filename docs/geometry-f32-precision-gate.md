# Geometry f32 input-quantization gate

This numerical test compares the portable CPU geometry references with
themselves after quantizing input positions and directions to IEEE-754
binary32. It covers Box, Orb and solid full-azimuth Tubs and checks ray
entry/exit and safety for well-separated, nondegenerate cases.

**What this verifies:** input rounding alone does not move these selected
distances by more than 1e-4 mm absolute plus 1e-5 relative, and miss/infinity
classification is unchanged. It also provides explicit sample rays for later
GPU differential tests.

**What it does not verify:** no WGSL geometry kernel is dispatched, so this
is not evidence of CPU/GPU numerical equivalence, f32 shader rounding, GPU
boundary classification, or performance. Those require separate tests against
Geant4, with targeted near-boundary and grazing rays and documented tolerances.

No CPU fallback, RNG, physics, or Geant4 integration behavior is modified.
