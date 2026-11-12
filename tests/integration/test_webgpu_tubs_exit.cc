#include "g4wgpu/TubsGeometry.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

bool agrees(double expected, double actual) {
  return std::isfinite(expected) && std::isfinite(actual) &&
         actual >= 0.0 &&
         std::fabs(expected - actual) <=
             5.0e-4 + 5.0e-5 * std::fabs(expected);
}

}  // namespace

int main() {
  using V = g4wgpu::TubsVector3;
  using Ray = g4wgpu::TubsRay;
  g4wgpu::WebGpuBackend gpu;

  for (const double scale : {0.1, 1.0, 100.0}) {
    const double r = 10.0 * scale;
    const double h = 20.0 * scale;
    const g4wgpu::PortableSolidTubs tubs{r, h};

    std::vector<Ray> rays{
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
        {{0.0, 0.0, 0.0}, {0.0, -3.0, 0.0}},
        {{0.0, 0.0, 0.0}, {0.0, 0.0, 3.0}},
        {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}},
        {{0.5 * r, 0.0, 0.0}, {2.0, 0.0, 0.0}},
        {{0.5 * r, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
        {{r, 0.0, 0.0}, {1.0, 0.0, 0.0}},
        {{r, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
        {{0.0, -r, 0.0}, {0.0, -1.0, 0.0}},
        {{0.0, -r, 0.0}, {0.0, 1.0, 0.0}},
        {{0.0, 0.0, h}, {0.0, 0.0, 1.0}},
        {{0.0, 0.0, h}, {0.0, 0.0, -1.0}},
        {{0.0, 0.0, -h}, {0.0, 0.0, -1.0}},
        {{0.0, 0.0, -h}, {0.0, 0.0, 1.0}},
        {{0.5 * r, 0.0, 0.5 * h}, {1.0, 0.0, 1.0}},
        {{0.5 * r, 0.0, 0.5 * h}, {-1.0, 0.0, -1.0}},
        {{0.0, 0.0, h - 0.001 * scale}, {0.0, 0.0, 1.0}},
        {{r - 0.001 * scale, 0.0, 0.0}, {1.0, 0.0, 0.0}},
    };

    // Rays remain well within the Tubs radial/axial bounds.
    // 768 rays exercise >6 WebGPU workgroups at each geometry scale.
    for (int i = 0; i < 768; ++i) {
      const V p{
          static_cast<double>((i % 31) - 15) * (0.035 * r),
          static_cast<double>((i % 23) - 11) * (0.034 * r),
          static_cast<double>((i % 29) - 14) * (0.045 * h)};
      const V direction =
          i % 4 == 0 ? V{1.0, 0.35, -0.1}
        : i % 4 == 1 ? V{-1.0, 0.1, 0.35}
        : i % 4 == 2 ? V{0.0, 1.0, -0.75}
                     : V{0.2, -0.35, -1.0};
      rays.push_back({p, direction});
    }

    const auto computed = gpu.tubs_distance_to_out_batch(tubs, rays);
    if (computed.size() != rays.size()) {
      std::cerr << "Tubs GPU exit batch length mismatch\n";
      return EXIT_FAILURE;
    }

    for (std::size_t i = 0; i < rays.size(); ++i) {
      const double expected = g4wgpu::tubs_distance_to_out_mm(
          tubs, rays[i].position_mm, rays[i].direction);
      if (!agrees(expected, computed[i])) {
        std::cerr << "Tubs exit CPU/GPU mismatch scale=" << scale
                  << " ray=" << i << " CPU=" << expected
                  << " GPU=" << computed[i] << '\n';
        return EXIT_FAILURE;
      }
    }
  }

  const g4wgpu::PortableSolidTubs tubs{10.0, 20.0};
  if (!gpu.tubs_distance_to_out_batch(tubs, {}).empty()) {
    std::cerr << "Empty Tubs exit batch must remain empty\n";
    return EXIT_FAILURE;
  }

  const auto rejects = [](const auto& run) {
    try {
      run();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double huge = std::numeric_limits<double>::max();
  if (!rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            {-1.0, 20.0}, {{{0, 0, 0}, {1, 0, 0}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            {10.0, inf}, {{{0, 0, 0}, {1, 0, 0}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{11, 0, 0}, {-1, 0, 0}}});
      }) ||
      !rejects([&] {
        // Inside the bounding Box, but outside the radial cylinder.
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{9, 9, 0}, {-1, 0, 0}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{0, 0, 21}, {0, 0, -1}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{nan, 0, 0}, {1, 0, 0}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{0, 0, 0}, {0, 0, 0}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{0, 0, 0}, {inf, 0, 0}}});
      }) ||
      !rejects([&] {
        (void)gpu.tubs_distance_to_out_batch(
            tubs, {{{huge, 0, 0}, {-1, 0, 0}}});
      })) {
    std::cerr << "Invalid Tubs exit input was accepted\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
