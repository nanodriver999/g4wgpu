#include "g4wgpu/FlatBoxNavigationWebGpu.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using V = g4wgpu::BoxVector3;
using Ray = g4wgpu::BoxRay;
using Boundary = g4wgpu::FlatBoundary;

g4wgpu::FlatBoxScene reference_scene() {
  g4wgpu::FlatBoxScene s;
  s.world = {{100.0, 100.0, 100.0}};
  s.daughters = {
      {11u, {{8.0, 10.0, 12.0}}, {-35.0, 0.0, 0.0}},
      {22u, {{10.0, 8.0, 14.0}}, {25.0, 0.0, 0.0}},
      {33u, {{5.0, 6.0, 7.0}}, {0.0, 0.0, 30.0}}};
  return s;
}

bool equivalent(const Boundary a, const Boundary b,
                const double tolerance = 6.0e-4) {
  return a.kind == b.kind && a.daughter_id == b.daughter_id &&
         std::isfinite(a.distance_mm) &&
         std::fabs(a.distance_mm - b.distance_mm) <=
             tolerance + 5.0e-5 * b.distance_mm;
}

template <class Fn>
bool rejects(Fn&& fn) {
  try {
    fn();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace

int main() {
  g4wgpu::WebGpuBackend backend;
  const auto scene = reference_scene();
  std::vector<Ray> rays{
      {{0,0,0}, {1,0,0}},        // enter daughter 22
      {{0,0,0}, {-1,0,0}},       // enter daughter 11
      {{0,0,0}, {0,1,0}},        // world exit
      {{0,0,0}, {0,0,1}},        // enter daughter 33
      {{25,0,0}, {1,0,0}},       // daughter 22 exit
      {{-35,0,0}, {-1,0,0}},    // daughter 11 exit
      {{0,0,30}, {0,0,1}},      // daughter 33 exit
      {{60,0,0}, {-2,0,0}},     // enter daughter 22
      {{-50,30,0}, {1,0,0}},    // world exit
      {{0,0,90}, {0,0,2}},      // world exit
      {{14.0,0,0}, {1,0,0}},    // 1 mm daughter entry
      {{34.0,0,0}, {1,0,0}},    // 1 mm daughter exit
      {{0,0,21}, {0,0,1}},      // daughter 33 entry
      {{0,0,36}, {0,0,1}},      // daughter 33 exit
  };

  // Repeatable non-grazing rays across more than eight GPU workgroups.
  // Positions never touch a daughter surface, apart from seed cases.
  for (int i = 0; i < 1024; ++i) {
    V p;
    if (i % 6 == 0) {
      p = {-35.0 + ((i % 17) - 8) * 0.3,
           ((i % 13) - 6) * 0.3, ((i % 11) - 5) * 0.3};
    } else if (i % 6 == 1) {
      p = {25.0 + ((i % 17) - 8) * 0.3,
           ((i % 13) - 6) * 0.3, ((i % 11) - 5) * 0.3};
    } else if (i % 6 == 2) {
      p = {((i % 11) - 5) * 0.3,
           ((i % 9) - 4) * 0.3, 30.0 + ((i % 7) - 3) * 0.3};
    } else if (i % 6 == 3) {
      p = {0.0, ((i % 17) - 8) * 0.33, 0.0};
    } else if (i % 6 == 4) {
      p = {60.0, ((i % 19) - 9) * 0.3, 0.0};
    } else {
      p = {-60.0, 20.0, ((i % 13) - 6) * 0.3};
    }
    const V d =
        i % 5 == 0 ? V{1,0,0}
      : i % 5 == 1 ? V{-2,0,0}
      : i % 5 == 2 ? V{0,1,0}
      : i % 5 == 3 ? V{0,0,-1}
                   : V{1,0.25,-0.2};
    rays.push_back({p,d});
  }

  // Verify the actual single-dispatch WGSL candidate, NOT only the safe
  // CPU-substituted wrapper. Stable cases should already match the f64
  // reference without requiring CPU repair.
  const auto raw = backend.flat_box_fused_candidates_batch(scene, rays);
  if (raw.size() != rays.size()) {
    std::cerr << "Raw fused GPU result count mismatch\n";
    return EXIT_FAILURE;
  }
  bool world = false, entry = false, exit = false;
  for (std::size_t i = 0; i < rays.size(); ++i) {
    const Boundary expected = g4wgpu::flat_box_next_boundary_mm(
        scene, rays[i].position_mm, rays[i].direction);
    if (!equivalent(raw[i], expected)) {
      std::cerr << "Raw fused GPU mismatch at ray " << i
                << " distance " << raw[i].distance_mm << "/"
                << expected.distance_mm << " kind "
                << static_cast<unsigned>(raw[i].kind) << "/"
                << static_cast<unsigned>(expected.kind)
                << " daughter " << raw[i].daughter_id << "/"
                << expected.daughter_id << '\n';
      return EXIT_FAILURE;
    }
    world |= raw[i].kind == g4wgpu::FlatBoundaryKind::world_exit;
    entry |= raw[i].kind == g4wgpu::FlatBoundaryKind::daughter_entry;
    exit |= raw[i].kind == g4wgpu::FlatBoundaryKind::daughter_exit;
  }
  if (!world || !entry || !exit) {
    std::cerr << "Raw shader missed a boundary category\n";
    return EXIT_FAILURE;
  }

  g4wgpu::FlatBoxGpuBatchStats stats;
  const auto checked = g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
      backend, scene, rays, &stats);
  if (checked.size() != rays.size() ||
      stats.gpu_box_dispatches != 1u ||
      stats.gpu_fused_dispatches != 1u ||
      stats.cpu_reference_fallbacks != 0u) {
    std::cerr << "Fused GPU dispatch/fallback counters mismatch\n";
    return EXIT_FAILURE;
  }
  for (std::size_t i = 0; i < rays.size(); ++i) {
    if (!equivalent(checked[i], raw[i])) {
      std::cerr << "Validated fused GPU changed stable raw result\n";
      return EXIT_FAILURE;
    }
  }

  // Test the previous multi-dispatch path is still usable and gives
  // equivalent results for the representative subset.
  std::vector<Ray> subset(rays.begin(), rays.begin() + 14);
  g4wgpu::FlatBoxGpuBatchStats old_stats;
  const auto multi = g4wgpu::flat_box_next_boundary_webgpu_batch(
      backend, scene, subset, &old_stats);
  if (old_stats.gpu_box_dispatches < 3u ||
      old_stats.gpu_fused_dispatches != 0u ||
      multi.size() != subset.size()) {
    std::cerr << "Existing multi-dispatch baseline regressed\n";
    return EXIT_FAILURE;
  }
  for (std::size_t i = 0; i < subset.size(); ++i) {
    if (!equivalent(multi[i], checked[i])) {
      std::cerr << "Fused and previous multi-dispatch results differ\n";
      return EXIT_FAILURE;
    }
  }

  // A coordinate 0.000001 mm from the face rounds onto it in f32.
  // With zero tolerance, the safe wrapper must use the f64 reference.
  g4wgpu::FlatBoxScene only_world;
  only_world.world = scene.world;
  const Ray near_face{{99.999999,0,0}, {1,0,0}};
  g4wgpu::FlatBoxGpuBatchStats exact_stats;
  const auto exact = g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
      backend, only_world, {near_face}, &exact_stats, 0.0, 0.0);
  const auto expected = g4wgpu::flat_box_next_boundary_mm(
      only_world, near_face.position_mm, near_face.direction);
  if (exact.size() != 1u || !equivalent(exact[0], expected, 1e-10) ||
      exact_stats.cpu_reference_fallbacks != 1u ||
      exact_stats.gpu_fused_dispatches != 1u) {
    std::cerr << "Fused near-face CPU fallback failed\n";
    return EXIT_FAILURE;
  }

  g4wgpu::FlatBoxGpuBatchStats empty_stats;
  if (!g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
           backend, scene, {}, &empty_stats).empty() ||
      empty_stats.gpu_fused_dispatches != 0u) {
    std::cerr << "Empty fused batch not handled\n";
    return EXIT_FAILURE;
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  auto invalid = scene;
  invalid.daughters[1].id = invalid.daughters[0].id;
  if (!rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
            backend, invalid, rays);
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
            backend, scene, {{{101,0,0}, {-1,0,0}}});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
            backend, scene, {{{nan,0,0}, {1,0,0}}});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
            backend, scene, {{{0,0,0}, {0,0,0}}});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
            backend, scene, rays, nullptr, -1.0);
      })) {
    std::cerr << "Invalid fused GPU input was accepted\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
