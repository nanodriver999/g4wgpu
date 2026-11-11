#include "g4wgpu/BoxGeometry.hh"
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
  if (std::isinf(cpu) || std::isinf(gpu)) {
    return std::isinf(cpu) && std::isinf(gpu) &&
           cpu > 0.0 && gpu > 0.0;
  }
  return std::isfinite(gpu) &&
         std::fabs(cpu - gpu) <= 2.0e-4 +
             2.0e-5 * std::fabs(cpu);
}

}  // namespace

int main() {
  using V = g4wgpu::BoxVector3;
  const g4wgpu::AxisAlignedBox box{{2.0, 3.0, 4.0}};
  g4wgpu::WebGpuBackend gpu;

  std::vector<g4wgpu::BoxRay> rays{
      {{5.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{5.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
      {{5.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{5.0, 5.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},
      {{2.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, 6.0}, {0.0, 0.0, -1.0}},
      {{3.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{2.0001, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{0.0, -8.0, 0.0}, {0.0, 2.0, 0.0}},
      {{0.0, 0.0, 10.0}, {0.0, 0.0, -3.0}}
  };

  // Exercise multiple workgroups and both hits and misses. Geometric
  // coordinates stay well inside f32 dynamic range; CPU is the reference.
  for (int i = 0; i < 512; ++i) {
    const double x = 4.0 + static_cast<double>(i % 23) * 0.27;
    const double y = static_cast<double>((i % 13) - 6) * 0.45;
    const double z = static_cast<double>((i % 17) - 8) * 0.43;
    const V direction =
        i % 4 == 0 ? V{-2.0, 0.0, 0.0}
      : i % 4 == 1 ? V{-1.0, 0.25, -0.15}
      : i % 4 == 2 ? V{1.0, 0.0, 0.0}
                   : V{-0.3, -1.0, 0.35};
    rays.push_back({{x, y, z}, direction});
  }

  const auto distances = gpu.box_distance_to_in_batch(box, rays);
  if (distances.size() != rays.size()) {
    std::cerr << "WebGPU Box entry produced the wrong batch size\n";
    return EXIT_FAILURE;
  }
  for (std::size_t i = 0; i < rays.size(); ++i) {
    const double reference = g4wgpu::box_distance_to_in_mm(
        box, rays[i].position_mm, rays[i].direction);
    if (!close_distance(reference, distances[i])) {
      std::cerr << "Box entry CPU/GPU mismatch at ray " << i
                << ": CPU=" << reference
                << " GPU=" << distances[i] << '\n';
      return EXIT_FAILURE;
    }
  }

  if (!gpu.box_distance_to_in_batch(box, {}).empty()) {
    std::cerr << "empty Box batch must stay empty\n";
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
  const double huge = std::numeric_limits<double>::max();
  if (!rejects([&] {
        (void)gpu.box_distance_to_in_batch(
            {{0.0, 3.0, 4.0}}, rays);
      }) ||
      !rejects([&] {
        (void)gpu.box_distance_to_in_batch(
            box, {{{5.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.box_distance_to_in_batch(
            box, {{{nan, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      }) ||
      !rejects([&] {
        (void)gpu.box_distance_to_in_batch(
            box, {{{huge, 0.0, 0.0}, {-1.0, 0.0, 0.0}}});
      })) {
    std::cerr << "invalid Box GPU input was accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
