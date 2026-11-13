# One-dispatch flattened Box next-boundary experiment (PR 8)

This increment runs **next-boundary candidate generation and winner selection
inside one WGSL compute dispatch**. It is a separate opt-in path named
`flat_box_next_boundary_webgpu_fused_batch`; PR #61's multi-dispatch
GPU candidate path remains available for comparison. Neither path replaces
Geant4's authoritative CPU navigator.

## Packed memory layout

The host uploads one array of normalized rays (`GpuBoxRay`, 32 bytes each),
one array of Box placements (`GpuFlatBox`, 32 bytes each), and a 16-byte
uniform with counts. The placement at index 0 is the origin-centered
world (ID 0), followed by translated daughter volumes (distinct nonzero IDs).
Each element contains center xyz, ID, and positive half-length xyz. The
shader writes one `GpuFlatResult` (16 bytes) per ray: distance in mm, kind
(0 world exit / 1 daughter entry / 2 daughter exit), and daughter ID.

The shader first tests daughter containment and immediately returns the
current daughter's outward boundary. Otherwise, it computes the world
outward boundary, intersects the ray with each daughter, and retains the
nearest entry using strict less-than comparisons to preserve the CPU
reference's tie ordering. It uses 128 threads per workgroup. The prototype
is explicitly bounded to **8,192 rays and 64 daughter Boxes** per call.

## Verification and fallback

`flat_box_next_boundary_webgpu_fused_batch` validates the scene and every
ray with the independent f64 CPU implementation. Non-f32-representable input
or prototype-size limits cause a full CPU fallback. After one GPU
dispatch/readback, any GPU output with a different kind/ID, nonfinite
distance, or excessive f32 numerical difference is replaced with the CPU
reference for that ray. Diagnostics count fused dispatches and reference
substitutions; a zero-tolerance near-face case specifically checks that f32
rounding invokes the CPU path.

The software-Vulkan integration test checks raw GPU results directly
against f64 reference for more than 1,000 rays spanning world exit,
daughter entry/exit and multiple workgroups. It also compares the previous
multi-dispatch path with the new result for selected rays, checks one
fused dispatch per batch, and rejects invalid scenes or rays.

## Important limits

This is **one shader dispatch, not persistent device-resident navigation**.
The host still constructs buffers/pipelines, uploads the scene and rays, does
a complete f64 CPU preflight, reads results back, and validates each ray.
The new GPU method itself also validates original f64 rays, so this is
deliberately *not* a speedup demonstration. Precision near Geant4 surface
tolerances, tie handling, overlaps, rotations, nested geometry, and moving
placements remain out of scope.

The next bounded step is to record **end-to-end wall times across batch
sizes**, including CPU preflight, staging, transfer, dispatch and readback,
and to evaluate whether persistent buffers/pipeline reuse are justified.
Keep GPU physics, RNG and Geant4 track navigation unchanged until broader
differential and performance evidence exists.
