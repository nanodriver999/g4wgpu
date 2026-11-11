# Experimental WebGPU Box DistanceToIn kernel

This is the first standalone geometry compute kernel in
`docs/implementation-plan.md`. It accelerates **only** ray entry distance
for local-coordinate, axis-aligned Box primitives, and does not yet replace
Geant4 navigation.

The experimental `WebGpuBackend::box_distance_to_in_batch` API takes one
`AxisAlignedBox` and a batch of `BoxRay` values. It stages validated,
normalized directions and f32-compatible positions and dimensions, dispatches
the Box slab-intersection WGSL compute shader, and reads back f32 distances.

- Output distances are in millimeters; positive infinity means no entry.
- Inputs too large or nonfinite for f32 are rejected, never silently clamped.
- Both parallel and approaching rays, and rays originating on/inside the
  Box, follow the portable CPU reference semantics.
- The shader is currently embedded in `src/webgpu/WebGpuBackend.cc` like
  the existing AXPY shader. WGSL and C++ input/output layouts are paired.
- An independent WebGPU integration test executes on software Vulkan,
  compares every result with `box_distance_to_in_mm` (including more than
  one workgroup), tests invalid inputs, and explicitly accounts for f32
  numerical error. Existing Geant4 G4Box comparisons cover the CPU reference.
- This compute kernel does not consume project RNG, modify any Geant4 track,
  influence navigation decisions, or change the default CPU fallback.

**Next bounded increments:** Box `DistanceToOut` and safety with GPU/CPU
differential tests, then Orb and Tubs kernels. Only after broader boundary
and geometry tests should any result be considered for authoritative
navigation, and actual speedup requires end-to-end timing.
