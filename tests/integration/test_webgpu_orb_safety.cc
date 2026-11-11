#include "g4wgpu/OrbGeometry.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
bool close_safety(double cpu, double gpu) {
  return std::isfinite(gpu) && gpu >= 0.0 &&
         std::fabs(cpu - gpu) <= 2.0e-4 + 2.0e-5 * std::fabs(cpu);
}
}  // namespace

int main() {
  using V = g4wgpu::OrbVector3;
  const g4wgpu::PortableOrb orb{10.0};
  g4wgpu::WebGpuBackend gpu;
  std::vector<V> positions{
      {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
      {0.0, -10.0, 0.0}, {0.0, 0.0, 10.0},
      {9.9999, 0.0, 0.0}, {10.0001, 0.0, 0.0},
      {-10.0001, 0.0, 0.0}, {20.0, 0.0, 0.0},
      {6.0, 8.0, 0.0}, {6.0, 8.0001, 0.0},
      {3.0, 4.0, 5.0}, {-12.0, -12.0, -12.0},
  };
  for (int i = 0; i < 640; ++i) {
    positions.push_back({
        static_cast<double>((i % 43) - 21) * 0.619,
        static_cast<double>((i % 47) - 23) * 0.433,
        static_cast<double>((i % 41) - 20) * 0.537,
    });
  }
  const auto inside = gpu.orb_safety_to_in_batch(orb, positions);
  const auto outside = gpu.orb_safety_to_out_batch(orb, positions);
  if (inside.size() != positions.size() ||
      outside.size() != positions.size()) {
    std::cerr << "Orb safety output length mismatch\n";
    return EXIT_FAILURE;
  }
  for (std::size_t i = 0; i < positions.size(); ++i) {
    const double cpu_in = g4wgpu::orb_safety_to_in_mm(orb, positions[i]);
    const double cpu_out = g4wgpu::orb_safety_to_out_mm(orb, positions[i]);
    if (!close_safety(cpu_in, inside[i]) ||
        !close_safety(cpu_out, outside[i])) {
      std::cerr << "Orb safety mismatch at " << i << ": "
                << cpu_in << "/" << inside[i] << ", "
                << cpu_out << "/" << outside[i] << '\n';
      return EXIT_FAILURE;
    }
  }
  if (!gpu.orb_safety_to_in_batch(orb, {}).empty() ||
      !gpu.orb_safety_to_out_batch(orb, {}).empty()) {
    std::cerr << "empty Orb safety batch mismatch\n";
    return EXIT_FAILURE;
  }
  const auto rejects = [](const auto& fn) {
    try { fn(); } catch (const std::invalid_argument&) { return true; }
    return false;
  };
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double huge = std::numeric_limits<double>::max();
  if (!rejects([&] { (void)gpu.orb_safety_to_in_batch({0.0}, positions); }) ||
      !rejects([&] { (void)gpu.orb_safety_to_out_batch({inf}, positions); }) ||
      !rejects([&] { (void)gpu.orb_safety_to_in_batch(orb, {{nan, 0.0, 0.0}}); }) ||
      !rejects([&] { (void)gpu.orb_safety_to_out_batch(orb, {{inf, 0.0, 0.0}}); }) ||
      !rejects([&] { (void)gpu.orb_safety_to_in_batch(orb, {{huge, 0.0, 0.0}}); })) {
    std::cerr << "invalid Orb safety input accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
