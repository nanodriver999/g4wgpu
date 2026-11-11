#include "g4wgpu/OrbGeometry.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

bool near(const double a, const double b, const double tolerance = 1.0e-9) {
  return std::fabs(a - b) <= tolerance;
}

}  // namespace

int main() {
  using V = g4wgpu::OrbVector3;
  const g4wgpu::PortableOrb orb{10.0};
  const auto enter = [&](V p, V v) {
    return g4wgpu::orb_distance_to_in_mm(orb, p, v);
  };
  const auto leave = [&](V p, V v) {
    return g4wgpu::orb_distance_to_out_mm(orb, p, v);
  };

  if (!near(enter({15.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}), 5.0) ||
      !near(enter({15.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}), 5.0) ||
      !near(enter({0.0, 0.0, 10.0}, {0.0, 0.0, -1.0}), 0.0) ||
      !near(enter({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}), 0.0) ||
      !std::isinf(enter({15.0, 0.0, 0.0}, {1.0, 0.0, 0.0})) ||
      !std::isinf(enter({15.0, 0.0, 0.0}, {0.0, 1.0, 0.0}))) {
    std::cerr << "orb entry rays or miss cases are incorrect\n";
    return EXIT_FAILURE;
  }

  if (!near(leave({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}), 10.0) ||
      !near(leave({0.0, 0.0, 5.0}, {0.0, 0.0, 2.0}), 5.0) ||
      !near(leave({0.0, 0.0, 5.0}, {0.0, 0.0, -2.0}), 15.0) ||
      !near(leave({10.0, 0.0, 0.0}, {1.0, 0.0, 0.0}), 0.0) ||
      !near(leave({10.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}), 20.0)) {
    std::cerr << "orb exit rays or direction scaling are incorrect\n";
    return EXIT_FAILURE;
  }

  if (!near(g4wgpu::orb_safety_to_in_mm(orb, {13.0, 0.0, 0.0}), 3.0) ||
      !near(g4wgpu::orb_safety_to_in_mm(orb, {0.0, 0.0, 0.0}), 0.0) ||
      !near(g4wgpu::orb_safety_to_out_mm(orb, {0.0, 0.0, 6.0}), 4.0) ||
      !near(g4wgpu::orb_safety_to_out_mm(orb, {13.0, 0.0, 0.0}), 0.0)) {
    std::cerr << "orb safety distances are incorrect\n";
    return EXIT_FAILURE;
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const auto rejects = [](const auto& fn) {
    try {
      fn();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  if (!rejects([&] { (void)g4wgpu::orb_distance_to_in_mm(
            {0.0}, {15.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)g4wgpu::orb_safety_to_in_mm(
            {inf}, {15.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)enter({15.0, 0.0, 0.0}, {0.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)enter({nan, 0.0, 0.0}, {-1.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)enter({15.0, 0.0, 0.0}, {inf, 0.0, 0.0}); }) ||
      !rejects([&] { (void)leave({11.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}); })) {
    std::cerr << "invalid orb geometry input was accepted\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
