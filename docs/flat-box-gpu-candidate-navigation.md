# GPU-assisted flat Box navigation candidate experiment

This increment follows the CPU reference and Geant4 private-navigator
crosschecks from PRs #51–#60. It uses one origin-centered Box world and
unrotated, translated, strictly disjoint daughter Boxes.

`flat_box_next_boundary_webgpu_batch` validates every ray and its
scene via the f64 CPU reference, dispatches existing WebGPU Box
`DistanceToOut` on world rays, and dispatches `DistanceToIn` or
`DistanceToOut` for each daughter's outside/inside rays. A **CPU
reducer** selects the nearest boundary, including daughter ID/kind.
When a f32 candidate differs from the CPU reference in kind, ID or
distance beyond tolerance, the f64 reference is returned for that ray.
Unsupported f32 geometry or position ranges also fall back to CPU.

The software-Vulkan test compares 780 rays (multiple workgroups) to
the independent CPU reference, exercises all boundary kinds and near-face
cases, and confirms CPU fallback when strict zero tolerance detects an
f32-rounded coordinate. The previously merged Geant4 integration tests
compare the CPU reference to a private G4Navigator oracle.

**Limitations:** This is not a fused GPU navigation kernel. It makes
several GPU calls and buffer readbacks per scene, runs expensive f64
preflight for all rays, and performs reduction on the CPU. Its timing
cannot support speedup claims. The next step is to pack descriptors and
rays into a persistent GPU buffer, perform selection within a single
GPU dispatch, and benchmark end-to-end wall time over batch sizes.

Geant4 authoritative navigation, CPU fallback, particle state, physics,
and RNG are unchanged.
