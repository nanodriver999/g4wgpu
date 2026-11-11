#include "g4wgpu/TubsGeometry.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

bool near(double value, double reference, double tolerance = 1.0e-9) {
  return std::fabs(value - reference) <= tolerance;
}

}  // namespace

int main() {
  using V = g4wgpu::TubsVector3;
  const g4wgpu::PortableSolidTubs tubs{10.0, 20.0};

  const auto entry = [&](V p, V v) {
    return g4wgpu::tubs_distance_to_in_mm(tubs, p, v);
  };
  const auto exit = [&](V p, V v) {
    return g4wgpu::tubs_distance_to_out_mm(tubs, p, v);
  };

  if (!near(entry({15.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}), 5.0) ||
      !near(entry({15.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}), 5.0) ||
      !near(entry({0.0, 0.0, 30.0}, {0.0, 0.0, -1.0}), 10.0) ||
      !near(entry({15.0, 0.0, 30.0}, {-1.0, 0.0, -1.0}),
            10.0 * std::sqrt(2.0)) ||
      !near(entry({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}), 0.0) ||
      !std::isinf(entry({15.0, 0.0, 0.0}, {1.0, 0.0, 0.0})) ||
      !std::isinf(entry({15.0, 0.0, 0.0}, {0.0, 0.0, 1.0})) ||
      !std::isinf(entry({0.0, 0.0, 30.0}, {1.0, 0.0, 0.0}))) {
    std::cerr << "solid Tubs entry ray distances are incorrect\n";
    return EXIT_FAILURE;
  }

  if (!near(exit({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}), 10.0) ||
      !near(exit({0.0, 0.0, 0.0}, {0.0, 0.0, 2.0}), 20.0) ||
      !near(exit({0.0, 0.0, 15.0}, {0.0, 0.0, 1.0}), 5.0) ||
      !near(exit({10.0, 0.0, 0.0}, {1.0, 0.0, 0.0}), 0.0) ||
      !near(exit({10.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}), 20.0) ||
      !near(exit({0.0, 0.0, 20.0}, {0.0, 0.0, 1.0}), 0.0)) {
    std::cerr << "solid Tubs exit distances are incorrect\n";
    return EXIT_FAILURE;
  }

  if (!near(g4wgpu::tubs_safety_to_in_mm(
                tubs, {15.0, 0.0, 0.0}), 5.0) ||
      !near(g4wgpu::tubs_safety_to_in_mm(
                tubs, {0.0, 0.0, 26.0}), 6.0) ||
      !near(g4wgpu::tubs_safety_to_in_mm(
                tubs, {15.0, 0.0, 26.0}), 6.0) ||
      !near(g4wgpu::tubs_safety_to_out_mm(
                tubs, {3.0, 4.0, 0.0}), 5.0) ||
      !near(g4wgpu::tubs_safety_to_out_mm(
                tubs, {0.0, 0.0, 18.0}), 2.0) ||
      !near(g4wgpu::tubs_safety_to_out_mm(
                tubs, {15.0, 0.0, 0.0}), 0.0)) {
    std::cerr << "solid Tubs safety distances are incorrect\n";
    return EXIT_FAILURE;
  }

  const auto rejects = [](const auto& fn) {
    try {
      fn();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  if (!rejects([&] { (void)entry(
           {15.0, 0.0, 0.0}, {0.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)entry(
           {nan, 0.0, 0.0}, {-1.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)entry(
           {15.0, 0.0, 0.0}, {inf, 0.0, 0.0}); }) ||
      !rejects([&] { (void)exit(
           {11.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)g4wgpu::tubs_safety_to_in_mm(
           {0.0, 20.0}, {15.0, 0.0, 0.0}); }) ||
      !rejects([&] { (void)g4wgpu::tubs_safety_to_out_mm(
           {10.0, inf}, {0.0, 0.0, 0.0}); })) {
    std::cerr << "invalid solid Tubs input was accepted\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
