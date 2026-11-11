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

## Batched DistanceToOut extension

The optional `box_distance_to_out_batch` API executes a second WGSL compute
entry point in the same shader module and shares the validated f32 staging,
buffer dispatch and readback path with `DistanceToIn`. Ray origins must be
inside or on the Box; the C++ adapter uses the portable f64 reference to
reject outside origins **before** f32 conversion. The GPU returns the first
outward crossing distance, including zero for outward rays on a face.

A separate software-Vulkan integration test compares more than 650 exit
rays across multiple workgroups to `box_distance_to_out_mm`, with f32
absolute/relative tolerances and explicit invalid-input regressions. This
is only a standalone geometry calculation, not navigation authority. Box
safety, Orb and Tubs WGSL calculations are still separate tasks, as are
precision stress cases and end-to-end performance measurements.

## Batched Box safety extension

The `box_safety_to_in_batch` and `box_safety_to_out_batch` methods accept
positions only. They reuse the existing f32-compatible Box ray staging format
with an ignored fixed direction, and dispatch two new independent WGSL entry
points. The in-safety shader computes the largest positive excess beyond the
axis-aligned Box faces; the out-safety shader computes the smallest remaining
clearance, clamped to zero. Both mirror the existing portable f64 Box CPU
reference and return distances in millimeters, including zero for points
inside/outside as appropriate.

The software-Vulkan test checks both kernels against the CPU reference over
multiple workgroups, including on-face, outside-corner, near-face and invalid
f32 input cases, with explicit f32 error bounds. This is a standalone numerical
experiment, not Geant4 navigation authority. Reusing an input buffer for each
dispatch does not establish GPU residency or an end-to-end speedup.
