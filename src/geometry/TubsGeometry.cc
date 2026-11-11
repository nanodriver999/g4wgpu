#include "g4wgpu/TubsGeometry.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace g4wgpu {
namespace {

void validate_tubs(const PortableSolidTubs& tubs) {
  if (!std::isfinite(tubs.outer_radius_mm) ||
      !(tubs.outer_radius_mm > 0.0) ||
      !std::isfinite(tubs.half_length_z_mm) ||
      !(tubs.half_length_z_mm > 0.0)) {
    throw std::invalid_argument(
        "solid Tubs outer radius and z half-length must be finite and positive");
  }
}

void validate_vector(const TubsVector3 v) {
  if (!std::isfinite(v.x) ||
      !std::isfinite(v.y) ||
      !std::isfinite(v.z)) {
    throw std::invalid_argument(
        "solid Tubs position and direction must be finite");
  }
}

TubsVector3 unit_direction(const TubsVector3 direction) {
  validate_vector(direction);
  const double length =
      std::hypot(direction.x, direction.y, direction.z);
  if (!(length > 0.0) || !std::isfinite(length)) {
    throw std::invalid_argument(
        "solid Tubs direction must be finite and nonzero");
  }
  return {
      direction.x / length,
      direction.y / length,
      direction.z / length};
}

double radial_norm(const TubsVector3 p) {
  return std::hypot(p.x, p.y);
}

bool inside_or_on(const PortableSolidTubs& tubs, const TubsVector3 p) {
  return radial_norm(p) <= tubs.outer_radius_mm &&
         std::fabs(p.z) <= tubs.half_length_z_mm;
}

}  // namespace

double tubs_distance_to_in_mm(
    const PortableSolidTubs& tubs,
    const TubsVector3 position_mm,
    const TubsVector3 direction) {
  validate_tubs(tubs);
  validate_vector(position_mm);
  const TubsVector3 ray = unit_direction(direction);
  if (inside_or_on(tubs, position_mm)) {
    return 0.0;
  }

  const double infinity = std::numeric_limits<double>::infinity();
  double entry = -infinity;
  double exit = infinity;

  // Intersection with the infinite cylinder x*x + y*y <= radius*radius.
  const double a = ray.x * ray.x + ray.y * ray.y;
  const double radial = radial_norm(position_mm);
  if (a == 0.0) {
    if (radial > tubs.outer_radius_mm) {
      return infinity;
    }
  } else {
    const double b = position_mm.x * ray.x + position_mm.y * ray.y;
    const double c =
        (radial - tubs.outer_radius_mm) *
        (radial + tubs.outer_radius_mm);
    const double discriminant = b * b - a * c;
    if (!(discriminant > 0.0)) {
      // Tangents touch the surface without entering its interior.
      return infinity;
    }

    const double root = std::sqrt(discriminant);
    const double q = -b - std::copysign(root, b);
    // The near-root product avoids catastrophic cancellation for rays
    // beginning far outside the cylinder.
    const double t0 = q / a;
    const double t1 = c / q;
    entry = std::min(t0, t1);
    exit = std::max(t0, t1);
  }

  // Intersect the radial interval with the slab bounded by two end caps.
  if (ray.z == 0.0) {
    if (std::fabs(position_mm.z) > tubs.half_length_z_mm) {
      return infinity;
    }
  } else {
    const double t0 =
        (-tubs.half_length_z_mm - position_mm.z) / ray.z;
    const double t1 =
        (tubs.half_length_z_mm - position_mm.z) / ray.z;
    entry = std::max(entry, std::min(t0, t1));
    exit = std::min(exit, std::max(t0, t1));
  }

  if (!(exit > std::max(0.0, entry))) {
    return infinity;
  }
  return std::max(0.0, entry);
}

double tubs_distance_to_out_mm(
    const PortableSolidTubs& tubs,
    const TubsVector3 position_mm,
    const TubsVector3 direction) {
  validate_tubs(tubs);
  validate_vector(position_mm);
  const TubsVector3 ray = unit_direction(direction);

  if (!inside_or_on(tubs, position_mm)) {
    throw std::invalid_argument(
        "solid Tubs distance to out requires an inside or surface point");
  }

  const double infinity = std::numeric_limits<double>::infinity();
  double radial_exit = infinity;
  const double a = ray.x * ray.x + ray.y * ray.y;
  if (a > 0.0) {
    const double radial = radial_norm(position_mm);
    const double b = position_mm.x * ray.x + position_mm.y * ray.y;
    const double c =
        (radial - tubs.outer_radius_mm) *
        (radial + tubs.outer_radius_mm);
    const double discriminant = std::max(0.0, b * b - a * c);
    radial_exit = (-b + std::sqrt(discriminant)) / a;
  }

  double endcap_exit = infinity;
  if (ray.z > 0.0) {
    endcap_exit =
        (tubs.half_length_z_mm - position_mm.z) / ray.z;
  } else if (ray.z < 0.0) {
    endcap_exit =
        (-tubs.half_length_z_mm - position_mm.z) / ray.z;
  }

  return std::max(0.0, std::min(radial_exit, endcap_exit));
}

double tubs_safety_to_in_mm(
    const PortableSolidTubs& tubs,
    const TubsVector3 position_mm) {
  validate_tubs(tubs);
  validate_vector(position_mm);
  return std::max({
      0.0,
      radial_norm(position_mm) - tubs.outer_radius_mm,
      std::fabs(position_mm.z) - tubs.half_length_z_mm});
}

double tubs_safety_to_out_mm(
    const PortableSolidTubs& tubs,
    const TubsVector3 position_mm) {
  validate_tubs(tubs);
  validate_vector(position_mm);
  return std::max(
      0.0,
      std::min(
          tubs.outer_radius_mm - radial_norm(position_mm),
          tubs.half_length_z_mm - std::fabs(position_mm.z)));
}

}  // namespace g4wgpu
