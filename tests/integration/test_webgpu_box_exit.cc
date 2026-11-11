#include "g4wgpu/BoxGeometry.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

bool close_distance(double cpu, double gpu) {
  return std::isfinite(cpu) && std::isfinite(gpu) &&
         std::fabs(cpu - gpu) <=
             2.0e-4 + 2.0e-5 * std::fabs(cpu);
}

}  // namespace

int main() {
  using V = g4wgpu::BoxVector3;
  const g4wgpu::AxisAlignedBox box{{2.0, 3.0, 4.0}};
  g4wgpu::WebGpuBackend gpu;

  std::vector<g4wgpu::BoxRay> rays{
      {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, 0.0}, {0.0, -2.0, 0.0}},
      {{0.0, 0.0, 0.0}, {0.0, 0.0, 3.0}},
      {{0.0, 0.0, 0.0}, {1.0, 2.0, 3.0}},
      {{1.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
      {{2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{2.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{0.0, -3.0, 0.0}, {0.0, -1.0, 0.0}},
      {{0.0, -3.0, 0.0}, {0.0, 1.0, 0.0}},
      {{0.0, 0.0, 4.0}, {0.0, 0.0, 1.0}},
      {{0.0, 0.0, 4.0}, {0.0, 0.0, -1.0}},
      {{1.9999, 0.0, 0.0}, {1.0, 0.0, 0.0}}
  };

  // More than four 128-thread workgroups, covering all local octants.
  for (int i = 0; i < 640; ++i) {
    const double x = static_cast<double>((i % 31) - 15) * 0.113;
    const double y = static_cast<double>((i % 23) - 11) * 0.19;
    const double z = static_cast<double>((i % 29) - 14) * 0.22;
    const V direction =
        i % 4 == 0 ? V{1.0, 0.35, -0.1}
      : i % 4 == 1 ? V{-1.0, 0.1, 0.35}
      : i % 4 == 2 ? V{0.0, 1.0, -0.75}
                   : V{0.2, -0.35, -1.0};
    rays.push_back({{x, y, z}, direction});
  }

  const auto gpu_results =
      gpu.box_distance_to_out_batch(box, rays);
  if (gpu_results.size() != rays.size()) {
    std::cerr << "wrong WebGPU Box exit batch length\n";
    return EXIT_FAILURE;
  }

  for (std::size_t i = 0; i < rays.size(); ++i) {
    const auto& ray = rays[i];
    const double reference =
        g4wgpu::box_distance_to_out_mm(
            box, ray.position_mm, ray.direction);
    if (!close_distance(reference, gpu_results[i])) {
      std::cerr << "Box exit CPU/GPU mismatch at " << i
                << ": CPU=" << reference
                << " GPU=" << gpu_results[i] << '\n';
      return EXIT_FAILURE;
    }
  }

  if (!gpu.box_distance_to_out_batch(box, {}).empty()) {
    std::cerr << "empty Box exit batch must be empty\n";
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

  const double nan =
      std::numeric_limits<double>::quiet_NaN();
  if (!rejects([&] {
        (void)gpu.box_distance_to_out_batch(
            box, {{{3.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.box_distance_to_out_batch(
            box, {{{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.box_distance_to_out_batch(
            box, {{{nan, 0.0, 0.0}, {1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.box_distance_to_out_batch(
            {{0.0, 3.0, 4.0}}, rays);
      })) {
    std::cerr << "invalid Box exit input accepted by GPU API\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
