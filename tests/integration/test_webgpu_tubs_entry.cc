#include "g4wgpu/TubsGeometry.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
bool matches(double expected, double actual) {
  if (std::isinf(expected)) {
    return std::isinf(actual) && actual > 0.0;
  }
  return std::isfinite(actual) && actual >= 0.0 &&
         std::fabs(expected - actual) <=
             5.0e-4 + 5.0e-5 * std::fabs(expected);
}
}

int main() {
  using g4wgpu::TubsRay;
  using g4wgpu::TubsVector3;
  g4wgpu::WebGpuBackend gpu;
  for (double scale : {0.1, 1.0, 100.0}) {
    const g4wgpu::PortableSolidTubs tubs{10.0 * scale, 20.0 * scale};
    const std::vector<TubsRay> rays{
        {{0, 0, 0}, {1, 0, 0}},
        {{12 * scale, 0, 0}, {-2, 0, 0}},
        {{0, 0, 25 * scale}, {0, 0, -3}},
        {{12 * scale, 0, 25 * scale}, {-1, 0, -1}},
        {{12 * scale, 0, 25 * scale}, {1, 0, 1}},
        {{12 * scale, 0, 0}, {0, 1, 0}},
        {{0, 0, 25 * scale}, {1, 0, 0}},
        {{12 * scale, 0, 0}, {-1, 0, 0.5}},
        {{-12 * scale, 0, 0}, {1, 0, 0.25}},
        {{0, 0, -25 * scale}, {0, 0, 1}},
        {{0, 0, -25 * scale}, {0, 0, -1}},
        {{0, 0, 0}, {0, 0, 1}},
    };
    const auto result = gpu.tubs_distance_to_in_batch(tubs, rays);
    if (result.size() != rays.size()) {
      std::cerr << "Tubs entry batch size mismatch\n";
      return EXIT_FAILURE;
    }
    for (std::size_t i = 0; i < rays.size(); ++i) {
      const auto expected = g4wgpu::tubs_distance_to_in_mm(
          tubs, rays[i].position_mm, rays[i].direction);
      if (!matches(expected, result[i])) {
        std::cerr << "Tubs entry mismatch scale=" << scale
                  << " ray=" << i << " CPU=" << expected
                  << " WebGPU=" << result[i] << '\n';
        return EXIT_FAILURE;
      }
    }
  }
  try {
    (void)gpu.tubs_distance_to_in_batch({-1, 20}, {{{0, 0, 0}, {1, 0, 0}}});
    std::cerr << "invalid Tubs geometry accepted\n";
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
  try {
    (void)gpu.tubs_distance_to_in_batch({10, 20}, {{{0, 0, 0}, {0, 0, 0}}});
    std::cerr << "zero direction accepted\n";
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
  std::cout << "Tubs WebGPU entry CPU differential checks passed\n";
  return EXIT_SUCCESS;
}
