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

g4wgpu::FlatBoxScene scene() {
  g4wgpu::FlatBoxScene s;
  s.world = {{100.0, 100.0, 100.0}};
  s.daughters = {
      {11u, {{8.0, 10.0, 12.0}}, {-35.0, 0.0, 0.0}},
      {22u, {{10.0, 8.0, 14.0}}, {25.0, 0.0, 0.0}}};
  return s;
}

bool same(const g4wgpu::FlatBoundary& lhs,
          const g4wgpu::FlatBoundary& rhs) {
  return lhs.kind == rhs.kind &&
         lhs.daughter_id == rhs.daughter_id &&
         std::fabs(lhs.distance_mm - rhs.distance_mm) <=
             7.0e-4 + 5.0e-5 * rhs.distance_mm;
}

template<class Fn>
bool rejects(Fn&& f) {
  try { f(); } catch (const std::invalid_argument&) { return true; }
  return false;
}
}  // namespace

int main() {
  g4wgpu::WebGpuBackend gpu;
  const auto detector = scene();
  std::vector<Ray> rays{
      {{0,0,0}, {1,0,0}},
      {{0,0,0}, {-1,0,0}},
      {{0,0,0}, {0,1,0}},
      {{-35,0,0}, {1,0,0}},
      {{25,0,0}, {-1,0,0}},
      {{60,0,0}, {-1,0,0}},
      {{-50,30,0}, {1,0,0}},
      {{0,0,0}, {1,1,0}},
      {{0,0,0}, {-2,0,0}},
      {{14.999,0,0}, {1,0,0}},
      {{34.999,0,0}, {1,0,0}},
      {{0,0,99.999}, {0,0,1}},
  };

  for (int i = 0; i < 768; ++i) {
    V p;
    if (i % 5 == 0) {
      p = {-35.0 + ((i % 13) - 6) * 0.5,
           ((i % 11) - 5) * 0.5, ((i % 7) - 3) * 0.5};
    } else if (i % 5 == 1) {
      p = {25.0 + ((i % 13) - 6) * 0.5,
           ((i % 11) - 5) * 0.5, ((i % 7) - 3) * 0.5};
    } else if (i % 5 == 2) {
      p = {0.0, ((i % 17) - 8) * 0.3, 0.0};
    } else if (i % 5 == 3) {
      p = {60.0, ((i % 19) - 9) * 0.4, 0.0};
    } else {
      p = {-50.0, 30.0, ((i % 17) - 8) * 0.7};
    }
    const V d =
        i % 6 == 0 ? V{1,0,0}
      : i % 6 == 1 ? V{-1,0,0}
      : i % 6 == 2 ? V{0,1,0}
      : i % 6 == 3 ? V{0,0,-1}
      : i % 6 == 4 ? V{1,0.25,-0.2}
                   : V{-2,-0.2,0.3};
    rays.push_back({p,d});
  }

  g4wgpu::FlatBoxGpuBatchStats stats;
  const auto results = g4wgpu::flat_box_next_boundary_webgpu_batch(
      gpu, detector, rays, &stats);
  if (results.size() != rays.size() ||
      stats.gpu_box_dispatches < 3) {
    std::cerr << "GPU candidate batch did not dispatch\n";
    return EXIT_FAILURE;
  }

  bool world = false, entry = false, exit = false;
  for (std::size_t i = 0; i < rays.size(); ++i) {
    const auto cpu = g4wgpu::flat_box_next_boundary_mm(
        detector, rays[i].position_mm, rays[i].direction);
    if (!same(results[i], cpu)) {
      std::cerr << "GPU/CPU next-boundary mismatch at ray " << i << '\n';
      return EXIT_FAILURE;
    }
    world |= results[i].kind == g4wgpu::FlatBoundaryKind::world_exit;
    entry |= results[i].kind == g4wgpu::FlatBoundaryKind::daughter_entry;
    exit |= results[i].kind == g4wgpu::FlatBoundaryKind::daughter_exit;
  }
  if (!world || !entry || !exit) {
    std::cerr << "Missing GPU candidate boundary category\n";
    return EXIT_FAILURE;
  }

  // A strict tolerance must fall back to f64 when a coordinate rounds
  // onto an f32 world face; the CPU reference remains authoritative.
  g4wgpu::FlatBoxScene empty_world;
  empty_world.world = detector.world;
  g4wgpu::FlatBoxGpuBatchStats exact_stats;
  const Ray near_face{{99.999999, 0, 0}, {1, 0, 0}};
  const auto exact = g4wgpu::flat_box_next_boundary_webgpu_batch(
      gpu, empty_world, {near_face}, &exact_stats, 0.0, 0.0);
  const auto expected = g4wgpu::flat_box_next_boundary_mm(
      empty_world, near_face.position_mm, near_face.direction);
  if (exact.size() != 1 || !same(exact[0], expected) ||
      exact_stats.cpu_reference_fallbacks != 1) {
    std::cerr << "Strict f32 fallback mismatch\n";
    return EXIT_FAILURE;
  }

  g4wgpu::FlatBoxGpuBatchStats empty_stats;
  if (!g4wgpu::flat_box_next_boundary_webgpu_batch(
           gpu, detector, {}, &empty_stats).empty() ||
      empty_stats.gpu_box_dispatches != 0) {
    std::cerr << "Empty batch mismatch\n";
    return EXIT_FAILURE;
  }
  auto duplicate = detector;
  duplicate.daughters[1].id = 11u;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  if (!rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_batch(
            gpu, duplicate, rays);
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_batch(
            gpu, detector, {{{101,0,0}, {-1,0,0}}});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_batch(
            gpu, detector, {{{nan,0,0}, {1,0,0}}});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_webgpu_batch(
            gpu, detector, rays, nullptr, -1.0);
      })) {
    std::cerr << "Invalid flat Box GPU input was accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
