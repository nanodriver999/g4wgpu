#include "g4wgpu/BoxGeometry.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace g4wgpu {
namespace {

using Axes = std::array<double, 3>;

Axes components(const BoxVector3 value) {
  return {value.x, value.y, value.z};
}

void validate_position(const Axes& position) {
  for (const double value : position) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("box position must be finite");
    }
  }
}

Axes validate_half_lengths(const AxisAlignedBox& box) {
  const Axes half = components(box.half_lengths_mm);
  for (const double value : half) {
    if (!std::isfinite(value) || !(value > 0.0)) {
      throw std::invalid_argument(
          "box half lengths must be finite and positive");
    }
  }
  return half;
}

Axes unit_direction(const BoxVector3 direction) {
  const Axes values = components(direction);
  validate_position(values);
  const double magnitude =
      std::hypot(values[0], values[1], values[2]);
  if (!std::isfinite(magnitude) || !(magnitude > 0.0)) {
    throw std::invalid_argument(
        "box ray direction must be finite and nonzero");
  }
  return {values[0] / magnitude,
          values[1] / magnitude,
          values[2] / magnitude};
}

}  // namespace

double box_distance_to_in_mm(
    const AxisAlignedBox& box,
    const BoxVector3 position_mm,
    const BoxVector3 direction) {
  const Axes half = validate_half_lengths(box);
  const Axes position = components(position_mm);
  validate_position(position);
  const Axes ray = unit_direction(direction);

  double entry = -std::numeric_limits<double>::infinity();
  double exit = std::numeric_limits<double>::infinity();

  for (std::size_t axis = 0; axis < 3; ++axis) {
    if (ray[axis] == 0.0) {
      if (position[axis] < -half[axis] ||
          position[axis] > half[axis]) {
        return std::numeric_limits<double>::infinity();
      }
      continue;
    }

    const double a = (-half[axis] - position[axis]) / ray[axis];
    const double b = (half[axis] - position[axis]) / ray[axis];
    entry = std::max(entry, std::min(a, b));
    exit = std::min(exit, std::max(a, b));
    if (entry > exit) {
      return std::numeric_limits<double>::infinity();
    }
  }

  if (exit < 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  return std::max(0.0, entry);
}

double box_distance_to_out_mm(
    const AxisAlignedBox& box,
    const BoxVector3 position_mm,
    const BoxVector3 direction) {
  const Axes half = validate_half_lengths(box);
  const Axes position = components(position_mm);
  validate_position(position);
  const Axes ray = unit_direction(direction);

  for (std::size_t axis = 0; axis < 3; ++axis) {
    if (std::fabs(position[axis]) > half[axis]) {
      throw std::invalid_argument(
          "box distance to out requires an inside or surface point");
    }
  }

  double exit = std::numeric_limits<double>::infinity();
  for (std::size_t axis = 0; axis < 3; ++axis) {
    if (ray[axis] > 0.0) {
      exit = std::min(
          exit, (half[axis] - position[axis]) / ray[axis]);
    } else if (ray[axis] < 0.0) {
      exit = std::min(
          exit, (-half[axis] - position[axis]) / ray[axis]);
    }
  }

  return std::max(0.0, exit);
}

double box_safety_to_in_mm(
    const AxisAlignedBox& box,
    const BoxVector3 position_mm) {
  const Axes half = validate_half_lengths(box);
  const Axes position = components(position_mm);
  validate_position(position);
  return std::max({
      0.0,
      std::fabs(position[0]) - half[0],
      std::fabs(position[1]) - half[1],
      std::fabs(position[2]) - half[2]});
}

double box_safety_to_out_mm(
    const AxisAlignedBox& box,
    const BoxVector3 position_mm) {
  const Axes half = validate_half_lengths(box);
  const Axes position = components(position_mm);
  validate_position(position);
  return std::max(
      0.0,
      std::min({
          half[0] - std::fabs(position[0]),
          half[1] - std::fabs(position[1]),
          half[2] - std::fabs(position[2])}));
}

}  // namespace g4wgpu
