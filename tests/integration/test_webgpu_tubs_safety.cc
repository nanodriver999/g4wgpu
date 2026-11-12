#include "g4wgpu/TubsGeometry.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
bool near(double cpu, double gpu) {
  return std::isfinite(gpu) && gpu >= 0.0 &&
         std::fabs(cpu - gpu) <= 3.0e-4 + 3.0e-5 * std::fabs(cpu);
}
}

int main() {
  using g4wgpu::TubsVector3;
  g4wgpu::WebGpuBackend gpu;
  for (double scale : {0.1, 1.0, 100.0}) {
    const g4wgpu::PortableSolidTubs tubs{10.0 * scale, 20.0 * scale};
    std::vector<TubsVector3> points{
        {0.0, 0.0, 0.0},
        {10.0 * scale, 0.0, 0.0},
        {0.0, 0.0, 20.0 * scale},
        {3.0 * scale, 4.0 * scale, 5.0 * scale},
        {11.0 * scale, 0.0, 0.0},
        {0.0, 0.0, 21.0 * scale},
        {11.0 * scale, 0.0, 21.0 * scale},
        {6.0 * scale, 8.0 * scale, 19.0 * scale},
    };
    for (int i = 0; i < 512; ++i) {
      points.push_back({
          (static_cast<double>(i % 37) - 18.0) * scale,
          (static_cast<double>(i % 29) - 14.0) * scale,
          (static_cast<double>(i % 53) - 26.0) * scale});
    }
    const auto in = gpu.tubs_safety_to_in_batch(tubs, points);
    const auto out = gpu.tubs_safety_to_out_batch(tubs, points);
    if (in.size() != points.size() || out.size() != points.size()) {
      std::cerr << "Tubs safety batch size mismatch\n";
      return EXIT_FAILURE;
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
      if (!near(g4wgpu::tubs_safety_to_in_mm(tubs, points[i]), in[i]) ||
          !near(g4wgpu::tubs_safety_to_out_mm(tubs, points[i]), out[i])) {
        std::cerr << "Tubs safety CPU/GPU mismatch at scale "
                  << scale << " index " << i << '\n';
        return EXIT_FAILURE;
      }
    }
  }
  // Invalid geometry must be rejected before narrowing to f32.
  try {
    (void)gpu.tubs_safety_to_in_batch({-1.0, 20.0}, {{0, 0, 0}});
    std::cerr << "invalid Tubs radius accepted\n";
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
  std::cout << "solid Tubs WebGPU safety differential checks passed\n";
  return EXIT_SUCCESS;
}
