#pragma once

#include <cstddef>
#include <vector>
#include "g4wgpu/FlatBoxNavigation.hh"

namespace g4wgpu {
class WebGpuBackend;

// Non-authoritative experiment diagnostics: each nonempty Box kernel call
// counts as one GPU dispatch. Reference substitutions are per ray.
struct FlatBoxGpuBatchStats {
  std::size_t gpu_box_dispatches = 0;
  std::size_t cpu_reference_fallbacks = 0;
  std::size_t gpu_fused_dispatches = 0;
};

// Submit world/daughter Box candidate distances to WebGPU in batches;
// select the next boundary on the CPU. Always validate each ray against
// the independent f64 reference, and substitute that reference if the
// f32 GPU answer differs in boundary identity or exceeds tolerance.
// Geant4 CPU navigation and track state are never modified.
//
// Geometry: axis-aligned Box world + disjoint translated Box daughters.
// This is not yet a fused GPU navigator or a performance-optimized path.
std::vector<FlatBoundary> flat_box_next_boundary_webgpu_batch(
    WebGpuBackend& gpu,
    const FlatBoxScene& scene,
    const std::vector<BoxRay>& rays,
    FlatBoxGpuBatchStats* stats = nullptr,
    double absolute_tolerance_mm = 5.0e-4,
    double relative_tolerance = 5.0e-5);
// Parallel experimental alternative: a single WGSL dispatch computes all
// candidate distances and selects the next boundary/ID on the device.
// Independently compare every GPU answer with the f64 CPU reference,
// substitute the reference on discrepancies, and preserve Geant4 fallback.
// Storage descriptors are re-uploaded on each call (not resident yet).
std::vector<FlatBoundary> flat_box_next_boundary_webgpu_fused_batch(
    WebGpuBackend& gpu,
    const FlatBoxScene& scene,
    const std::vector<BoxRay>& rays,
    FlatBoxGpuBatchStats* stats = nullptr,
    double absolute_tolerance_mm = 5.0e-4,
    double relative_tolerance = 5.0e-5);

}  // namespace g4wgpu
