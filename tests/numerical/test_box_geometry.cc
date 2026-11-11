#include "g4wgpu/BoxGeometry.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

bool near(const double a, const double b, const double tolerance = 1.0e-10) {
  return std::fabs(a - b) <= tolerance;
}

}  // namespace

int main() {
  const g4wgpu::AxisAlignedBox box{{2.0, 3.0, 4.0}};
  using V = g4wgpu::BoxVector3;

  if (!near(g4wgpu::box_distance_to_in_mm(
                box, V{5.0, 0.0, 0.0}, V{-1.0, 0.0, 0.0}), 3.0) ||
      !near(g4wgpu::box_distance_to_in_mm(
                box, V{5.0, 0.0, 0.0}, V{-2.0, 0.0, 0.0}), 3.0) ||
      !near(g4wgpu::box_distance_to_in_mm(
                box, V{0.0, 0.0, 0.0}, V{0.0, 0.0, 1.0}), 0.0) ||
      !std::isinf(g4wgpu::box_distance_to_in_mm(
                box, V{5.0, 5.0, 0.0}, V{-1.0, 0.0, 0.0}))) {
    std::cerr << "ray-box entry distances are incorrect\n";
    return EXIT_FAILURE;
  }

  if (!near(g4wgpu::box_distance_to_out_mm(
                box, V{0.0, 0.0, 0.0}, V{1.0, 0.0, 0.0}), 2.0) ||
      !near(g4wgpu::box_distance_to_out_mm(
                box, V{1.0, 0.0, 0.0}, V{-2.0, 0.0, 0.0}), 3.0) ||
      !near(g4wgpu::box_distance_to_out_mm(
                box, V{2.0, 0.0, 0.0}, V{1.0, 0.0, 0.0}), 0.0)) {
    std::cerr << "ray-box exit distances are incorrect\n";
    return EXIT_FAILURE;
  }

  if (!near(g4wgpu::box_safety_to_in_mm(
                box, V{5.0, 5.0, 0.0}), 3.0) ||
      !near(g4wgpu::box_safety_to_in_mm(
                box, V{0.0, 0.0, 0.0}), 0.0) ||
      !near(g4wgpu::box_safety_to_out_mm(
                box, V{1.0, 0.0, 0.0}), 1.0) ||
      !near(g4wgpu::box_safety_to_out_mm(
                box, V{5.0, 0.0, 0.0}), 0.0)) {
    std::cerr << "box safety distances are incorrect\n";
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
  if (!rejects([&] {
        (void)g4wgpu::box_distance_to_in_mm(
            box, V{5.0, 0.0, 0.0}, V{0.0, 0.0, 0.0});
      }) ||
      !rejects([&] {
        (void)g4wgpu::box_distance_to_out_mm(
            box, V{5.0, 0.0, 0.0}, V{1.0, 0.0, 0.0});
      }) ||
      !rejects([&] {
        (void)g4wgpu::box_safety_to_in_mm(
            g4wgpu::AxisAlignedBox{{0.0, 3.0, 4.0}},
            V{5.0, 0.0, 0.0});
      }) ||
      !rejects([&] {
        (void)g4wgpu::box_distance_to_in_mm(
            box, V{nan, 0.0, 0.0}, V{-1.0, 0.0, 0.0});
      })) {
    std::cerr << "invalid ray-box input was accepted\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
