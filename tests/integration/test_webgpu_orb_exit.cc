#include "g4wgpu/OrbGeometry.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

bool close_distance(double cpu, double gpu) {
  return std::isfinite(cpu) && std::isfinite(gpu) && gpu >= 0.0 &&
         std::fabs(cpu - gpu) <= 2.0e-4 +
             2.0e-5 * std::fabs(cpu);
}

}  // namespace

int main() {
  using V = g4wgpu::OrbVector3;
  const g4wgpu::PortableOrb orb{10.0};
  g4wgpu::WebGpuBackend gpu;

  std::vector<g4wgpu::OrbRay> rays{
      {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, 0.0}, {0.0, -3.0, 0.0}},
      {{0.0, 0.0, 0.0}, {1.0, 2.0, 3.0}},
      {{0.0, 0.0, 5.0}, {0.0, 0.0, 2.0}},
      {{0.0, 0.0, 5.0}, {0.0, 0.0, -2.0}},
      {{5.0, 0.0, 0.0}, {2.0, 0.0, 0.0}},
      {{5.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
      {{10.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{10.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{0.0, -10.0, 0.0}, {0.0, -1.0, 0.0}},
      {{0.0, -10.0, 0.0}, {0.0, 1.0, 0.0}},
      {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}},
      {{6.0, 8.0, 0.0}, {-3.0, 0.0, 0.0}},
      {{9.999, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, -9.999}, {0.0, 0.0, -1.0}},
  };

  // Deterministic interior positions with norm below 7 mm, far from
  // FP32 boundary-classification ambiguity. Exercise >5 workgroups.
  for (int i = 0; i < 768; ++i) {
    const V p{
        static_cast<double>((i % 31) - 15) * 0.30,
        static_cast<double>((i % 23) - 11) * 0.33,
        static_cast<double>((i % 29) - 14) * 0.26};
    const V dir = i % 4 == 0 ? V{1.0, 0.35, -0.10}
                : i % 4 == 1 ? V{-1.0, 0.10, 0.35}
                : i % 4 == 2 ? V{0.0, 1.0, -0.75}
                             : V{0.2, -0.35, -1.0};
    rays.push_back({p, dir});
  }

  const auto computed = gpu.orb_distance_to_out_batch(orb, rays);
  if (computed.size() != rays.size()) {
    std::cerr << "WebGPU Orb exit batch length mismatch\n";
    return EXIT_FAILURE;
  }

  for (std::size_t i = 0; i < rays.size(); ++i) {
    const auto& ray = rays[i];
    const double expected =
        g4wgpu::orb_distance_to_out_mm(
            orb, ray.position_mm, ray.direction);
    if (!close_distance(expected, computed[i])) {
      std::cerr << "Orb exit GPU/CPU mismatch at " << i
                << ": CPU=" << expected
                << " GPU=" << computed[i] << '\n';
      return EXIT_FAILURE;
    }
  }

  // Geometric distance is invariant under positive rescaling of the ray
  // direction. Exercise the host normalization and f32 staging path with
  // both sub-unit and large (but finite) direction magnitudes.
  const std::vector<double> scales{0.125, 0.5, 2.0, 16.0, 1024.0};
  std::vector<g4wgpu::OrbRay> scaled_rays;
  for (std::size_t i = 0; i < 32; ++i) {
    const auto& ray = rays[i];
    for (double scale : scales) {
      scaled_rays.push_back({ray.position_mm,
                             {ray.direction.x * scale,
                              ray.direction.y * scale,
                              ray.direction.z * scale}});
    }
  }
  const auto scaled_distances = gpu.orb_distance_to_out_batch(orb, scaled_rays);
  if (scaled_distances.size() != scaled_rays.size()) {
    std::cerr << "Scaled Orb exit batch length mismatch\\n";
    return EXIT_FAILURE;
  }
  for (std::size_t i = 0; i < scaled_rays.size(); ++i) {
    const std::size_t original = i / scales.size();
    const double cpu = g4wgpu::orb_distance_to_out_mm(
        orb, scaled_rays[i].position_mm, scaled_rays[i].direction);
    if (!close_distance(cpu, scaled_distances[i]) ||
        !close_distance(computed[original], scaled_distances[i])) {
      std::cerr << "Orb exit direction scaling mismatch at " << i << '\\n';
      return EXIT_FAILURE;
    }
  }

  if (!gpu.orb_distance_to_out_batch(orb, {}).empty()) {
    std::cerr << "Empty Orb exit batch must remain empty\n";
    return EXIT_FAILURE;
  }

  const auto rejects = [](const auto& operation) {
    try {
      operation();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double huge = std::numeric_limits<double>::max();
  if (!rejects([&] {
        (void)gpu.orb_distance_to_out_batch({0.0}, rays);
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_out_batch({inf}, rays);
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_out_batch(
            orb, {{{11.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        // Inside bounding Box [-10,10]^3, but outside Orb radius 10.
        (void)gpu.orb_distance_to_out_batch(
            orb, {{{9.0, 9.0, 0.0}, {-1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_out_batch(
            orb, {{{nan, 0.0, 0.0}, {1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_out_batch(
            orb, {{{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_out_batch(
            orb, {{{0.0, 0.0, 0.0}, {inf, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_out_batch(
            orb, {{{huge, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      })) {
    std::cerr << "Invalid Orb exit input was accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
