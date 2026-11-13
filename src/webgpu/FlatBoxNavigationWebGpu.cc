#include "g4wgpu/FlatBoxNavigationWebGpu.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace g4wgpu {
namespace {

bool f32(const double x) {
  return std::isfinite(x) &&
         std::fabs(x) <=
             static_cast<double>(std::numeric_limits<float>::max());
}

bool f32_box(const AxisAlignedBox& box) {
  const auto h = box.half_lengths_mm;
  return f32(h.x) && f32(h.y) && f32(h.z) &&
         static_cast<float>(h.x) > 0.0f &&
         static_cast<float>(h.y) > 0.0f &&
         static_cast<float>(h.z) > 0.0f;
}

bool f32_position(const BoxVector3 p) {
  return f32(p.x) && f32(p.y) && f32(p.z);
}

BoxVector3 local(const BoxVector3 p, const BoxVector3 c) {
  return {p.x - c.x, p.y - c.y, p.z - c.z};
}

bool contains(const AxisAlignedBox& box, const BoxVector3 p) {
  const auto h = box.half_lengths_mm;
  return std::fabs(p.x) <= h.x &&
         std::fabs(p.y) <= h.y &&
         std::fabs(p.z) <= h.z;
}

bool agrees(const FlatBoundary& gpu, const FlatBoundary& cpu,
            double absolute_tolerance, double relative_tolerance) {
  return gpu.kind == cpu.kind &&
         gpu.daughter_id == cpu.daughter_id &&
         std::isfinite(gpu.distance_mm) && gpu.distance_mm >= 0.0 &&
         std::fabs(gpu.distance_mm - cpu.distance_mm) <=
             absolute_tolerance + relative_tolerance * cpu.distance_mm;
}

}  // namespace

std::vector<FlatBoundary> flat_box_next_boundary_webgpu_batch(
    WebGpuBackend& gpu,
    const FlatBoxScene& scene,
    const std::vector<BoxRay>& rays,
    FlatBoxGpuBatchStats* stats,
    const double absolute_tolerance_mm,
    const double relative_tolerance) {
  if (!std::isfinite(absolute_tolerance_mm) ||
      !std::isfinite(relative_tolerance) ||
      absolute_tolerance_mm < 0.0 || relative_tolerance < 0.0) {
    throw std::invalid_argument(
        "flat Box GPU tolerances must be finite and nonnegative");
  }
  FlatBoxGpuBatchStats diagnostics;

  // Validate geometry even when no rays were requested.
  if (rays.empty()) {
    (void)flat_box_next_boundary_mm(
        scene, BoxVector3{}, BoxVector3{1.0, 0.0, 0.0});
    if (stats != nullptr) *stats = diagnostics;
    return {};
  }

  // This costly f64 preflight deliberately preserves an independent
  // correctness oracle and CPU fallback. GPU speedups are not claimed.
  std::vector<FlatBoundary> reference;
  reference.reserve(rays.size());
  for (const auto& ray : rays) {
    reference.push_back(flat_box_next_boundary_mm(
        scene, ray.position_mm, ray.direction));
  }

  bool supported = f32_box(scene.world);
  for (const auto& daughter : scene.daughters) {
    supported = supported && f32_box(daughter.box) &&
                f32_position(daughter.center_mm);
  }
  for (const auto& ray : rays) {
    supported = supported && f32_position(ray.position_mm);
    for (const auto& daughter : scene.daughters) {
      supported = supported && f32_position(
          local(ray.position_mm, daughter.center_mm));
    }
  }
  // WebGpuBackend currently permits 65535 workgroups of 128 rays.
  if (!supported || rays.size() > 65535u * 128u) {
    diagnostics.cpu_reference_fallbacks = rays.size();
    if (stats != nullptr) *stats = diagnostics;
    return reference;
  }

  const auto world_distances =
      gpu.box_distance_to_out_batch(scene.world, rays);
  ++diagnostics.gpu_box_dispatches;
  std::vector<FlatBoundary> selected;
  selected.reserve(rays.size());
  for (double d : world_distances) {
    selected.push_back(
        FlatBoundary{FlatBoundaryKind::world_exit, d, 0u});
  }

  for (const auto& daughter : scene.daughters) {
    std::vector<BoxRay> entries, exits;
    std::vector<std::size_t> entry_indices, exit_indices;
    entries.reserve(rays.size());
    exits.reserve(rays.size());
    for (std::size_t i = 0; i < rays.size(); ++i) {
      const BoxVector3 position =
          local(rays[i].position_mm, daughter.center_mm);
      const BoxRay translated{position, rays[i].direction};
      if (contains(daughter.box, position)) {
        exit_indices.push_back(i);
        exits.push_back(translated);
      } else {
        entry_indices.push_back(i);
        entries.push_back(translated);
      }
    }

    if (!entries.empty()) {
      const auto distances =
          gpu.box_distance_to_in_batch(daughter.box, entries);
      ++diagnostics.gpu_box_dispatches;
      for (std::size_t j = 0; j < distances.size(); ++j) {
        auto& candidate = selected[entry_indices[j]];
        if (candidate.kind != FlatBoundaryKind::daughter_exit &&
            distances[j] < candidate.distance_mm) {
          candidate = FlatBoundary{
              FlatBoundaryKind::daughter_entry,
              distances[j], daughter.id};
        }
      }
    }
    if (!exits.empty()) {
      const auto distances =
          gpu.box_distance_to_out_batch(daughter.box, exits);
      ++diagnostics.gpu_box_dispatches;
      for (std::size_t j = 0; j < distances.size(); ++j) {
        selected[exit_indices[j]] = FlatBoundary{
            FlatBoundaryKind::daughter_exit,
            distances[j], daughter.id};
      }
    }
  }

  for (std::size_t i = 0; i < rays.size(); ++i) {
    if (!agrees(selected[i], reference[i],
                absolute_tolerance_mm, relative_tolerance)) {
      selected[i] = reference[i];
      ++diagnostics.cpu_reference_fallbacks;
    }
  }
  if (stats != nullptr) *stats = diagnostics;
  return selected;
}

std::vector<FlatBoundary> flat_box_next_boundary_webgpu_fused_batch(
    WebGpuBackend& gpu,
    const FlatBoxScene& scene,
    const std::vector<BoxRay>& rays,
    FlatBoxGpuBatchStats* stats,
    const double absolute_tolerance_mm,
    const double relative_tolerance) {
  if (!std::isfinite(absolute_tolerance_mm) ||
      !std::isfinite(relative_tolerance) ||
      absolute_tolerance_mm < 0.0 || relative_tolerance < 0.0) {
    throw std::invalid_argument(
        "fused flat Box tolerances must be finite and nonnegative");
  }
  FlatBoxGpuBatchStats diagnostics;

  if (rays.empty()) {
    (void)flat_box_next_boundary_mm(
        scene, BoxVector3{}, BoxVector3{1.0, 0.0, 0.0});
    if (stats != nullptr) *stats = diagnostics;
    return {};
  }

  // The independent f64 CPU reference still validates the complete
  // scene, each direction, and every ray. This is deliberately not
  // a performance comparison or an authoritative navigator.
  std::vector<FlatBoundary> reference;
  reference.reserve(rays.size());
  for (const auto& ray : rays) {
    reference.push_back(flat_box_next_boundary_mm(
        scene, ray.position_mm, ray.direction));
  }

  bool supported = f32_box(scene.world) &&
                   scene.daughters.size() <= 64u &&
                   rays.size() <= 8192u;
  for (const auto& daughter : scene.daughters) {
    supported = supported && f32_box(daughter.box) &&
                f32_position(daughter.center_mm);
  }
  for (const auto& ray : rays) {
    supported = supported && f32_position(ray.position_mm);
    for (const auto& daughter : scene.daughters) {
      supported = supported && f32_position(
          local(ray.position_mm, daughter.center_mm));
    }
  }
  if (!supported) {
    diagnostics.cpu_reference_fallbacks = rays.size();
    if (stats != nullptr) *stats = diagnostics;
    return reference;
  }

  std::vector<FlatBoundary> candidates =
      gpu.flat_box_fused_candidates_batch(scene, rays);
  diagnostics.gpu_box_dispatches = 1u;
  diagnostics.gpu_fused_dispatches = 1u;
  if (candidates.size() != rays.size()) {
    throw std::runtime_error(
        "fused flat Box GPU returned wrong ray count");
  }

  for (std::size_t i = 0; i < rays.size(); ++i) {
    if (!agrees(candidates[i], reference[i],
                absolute_tolerance_mm, relative_tolerance)) {
      candidates[i] = reference[i];
      ++diagnostics.cpu_reference_fallbacks;
    }
  }
  if (stats != nullptr) *stats = diagnostics;
  return candidates;
}

}  // namespace g4wgpu
