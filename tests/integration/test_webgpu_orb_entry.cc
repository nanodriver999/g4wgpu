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

bool close_distance(const double cpu, const double gpu) {
  if (std::isinf(cpu) || std::isinf(gpu)) {
    return std::isinf(cpu) && std::isinf(gpu) &&
           cpu > 0.0 && gpu > 0.0;
  }
  return std::isfinite(gpu) && gpu >= 0.0 &&
         std::fabs(cpu - gpu) <= 2.0e-4 +
             2.0e-5 * std::fabs(cpu);
}

}  // namespace

int main() {
  using V = g4wgpu::OrbVector3;
  const g4wgpu::PortableOrb orb{10.0};
  g4wgpu::WebGpuBackend gpu;

  std::vector<g4wgpu::OrbRay> rays{
      {{15.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{20.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
      {{0.0, 0.0, 20.0}, {0.0, 0.0, -1.0}},
      {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{10.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{10.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{15.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{15.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},
      {{15.0, 15.0, 0.0}, {-1.0, -1.0, 0.0}},
      {{20.0, 0.0, 0.0}, {-1.0, 0.2, 0.0}},
      {{20.0, 0.0, 0.0}, {-1.0, 0.8, 0.0}},
      {{5.0, 5.0, 5.0}, {-1.0, 0.0, 0.0}},
      {{10.0001, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{100.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
  };

  // Rays are intentionally non-tangent and unambiguous in f32.
  // Exercise many GPU workgroups with deterministic hit/miss/inside cases.
  for (int i = 0; i < 768; ++i) {
    const V p{
        15.0 + static_cast<double>(i % 31) * 0.17,
        static_cast<double>((i % 23) - 11) * 0.29,
        static_cast<double>((i % 17) - 8) * 0.23};
    const V dir = i % 3 == 0
        ? V{-p.x, -p.y, -p.z}
        : i % 3 == 1
            ? V{p.x, p.y, p.z}
            : V{-2.0 * p.x, -2.0 * p.y, -2.0 * p.z};
    rays.push_back({p, dir});
  }

  const auto distances = gpu.orb_distance_to_in_batch(orb, rays);
  if (distances.size() != rays.size()) {
    std::cerr << "Orb GPU entry count mismatch\n";
    return EXIT_FAILURE;
  }

  for (std::size_t i = 0; i < rays.size(); ++i) {
    const auto& ray = rays[i];
    const double reference = g4wgpu::orb_distance_to_in_mm(
        orb, ray.position_mm, ray.direction);
    if (!close_distance(reference, distances[i])) {
      std::cerr << "Orb entry CPU/GPU mismatch at ray " << i
                << ": CPU=" << reference
                << " GPU=" << distances[i] << '\n';
      return EXIT_FAILURE;
    }
  }

  if (!gpu.orb_distance_to_in_batch(orb, {}).empty()) {
    std::cerr << "empty Orb entry batch must remain empty\n";
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
        (void)gpu.orb_distance_to_in_batch({0.0}, rays);
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_in_batch({inf}, rays);
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_in_batch(
            orb, {{{nan, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_in_batch(
            orb, {{{huge, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.orb_distance_to_in_batch(
            orb, {{{15.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}});
      })) {
    std::cerr << "invalid Orb entry input accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
