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

bool close_safety(const double cpu, const double gpu) {
  return std::isfinite(gpu) && gpu >= 0.0 &&
         std::fabs(cpu - gpu) <= 2.0e-4 + 2.0e-5 * std::fabs(cpu);
}

}  // namespace

int main() {
  using V = g4wgpu::BoxVector3;
  const g4wgpu::AxisAlignedBox box{{2.0, 3.0, 4.0}};
  g4wgpu::WebGpuBackend gpu;

  std::vector<V> positions{
      {0.0, 0.0, 0.0},
      {1.0, -2.0, 3.0},
      {2.0, 0.0, 0.0},
      {-2.0, -3.0, 4.0},
      {2.0001, 0.0, 0.0},
      {1.9999, 0.0, 0.0},
      {-2.0001, 0.0, 0.0},
      {0.0, -3.0001, 0.0},
      {0.0, 0.0, 4.0001},
      {5.0, 6.0, 0.0},
      {-8.0, -7.0, -9.0},
      {0.0, 0.0, 20.0},
  };

  // Multiple 128-thread workgroups, both inside and outside the Box.
  for (int i = 0; i < 640; ++i) {
    positions.push_back({
        static_cast<double>((i % 53) - 26) * 0.137,
        static_cast<double>((i % 47) - 23) * 0.193,
        static_cast<double>((i % 43) - 21) * 0.241,
    });
  }

  const auto inside = gpu.box_safety_to_in_batch(box, positions);
  const auto outside = gpu.box_safety_to_out_batch(box, positions);
  if (inside.size() != positions.size() ||
      outside.size() != positions.size()) {
    std::cerr << "Box safety batch size mismatch\n";
    return EXIT_FAILURE;
  }

  for (std::size_t i = 0; i < positions.size(); ++i) {
    const double cpu_in =
        g4wgpu::box_safety_to_in_mm(box, positions[i]);
    const double cpu_out =
        g4wgpu::box_safety_to_out_mm(box, positions[i]);
    if (!close_safety(cpu_in, inside[i]) ||
        !close_safety(cpu_out, outside[i])) {
      std::cerr << "Box safety CPU/GPU mismatch at " << i
                << ": in " << cpu_in << "/" << inside[i]
                << ", out " << cpu_out << "/" << outside[i] << '\n';
      return EXIT_FAILURE;
    }
  }

  if (!gpu.box_safety_to_in_batch(box, {}).empty() ||
      !gpu.box_safety_to_out_batch(box, {}).empty()) {
    std::cerr << "empty Box safety batches must remain empty\n";
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
        (void)gpu.box_safety_to_in_batch(
            {{0.0, 3.0, 4.0}}, positions);
      }) ||
      !rejects([&] {
        (void)gpu.box_safety_to_out_batch(
            {{2.0, inf, 4.0}}, positions);
      }) ||
      !rejects([&] {
        (void)gpu.box_safety_to_in_batch(
            box, {{nan, 0.0, 0.0}});
      }) ||
      !rejects([&] {
        (void)gpu.box_safety_to_out_batch(
            box, {{0.0, inf, 0.0}});
      }) ||
      !rejects([&] {
        (void)gpu.box_safety_to_in_batch(
            box, {{0.0, 0.0, huge}});
      })) {
    std::cerr << "invalid Box safety GPU input accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
