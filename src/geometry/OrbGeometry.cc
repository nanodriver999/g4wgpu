#include "g4wgpu/OrbGeometry.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace g4wgpu {
namespace {

double validated_radius(const PortableOrb& orb) {
  if (!std::isfinite(orb.radius_mm) || !(orb.radius_mm > 0.0)) {
    throw std::invalid_argument(
        "orb radius must be finite and positive");
  }
  return orb.radius_mm;
}

void validate_vector(const OrbVector3 v) {
  if (!std::isfinite(v.x) ||
      !std::isfinite(v.y) ||
      !std::isfinite(v.z)) {
    throw std::invalid_argument(
        "orb vector components must be finite");
  }
}

double norm(const OrbVector3 v) {
  return std::hypot(v.x, v.y, v.z);
}

double dot(const OrbVector3 a, const OrbVector3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

OrbVector3 unit_direction(const OrbVector3 direction) {
  validate_vector(direction);
  const double magnitude = norm(direction);
  if (!(magnitude > 0.0) || !std::isfinite(magnitude)) {
    throw std::invalid_argument(
        "orb direction must be finite and nonzero");
  }
  return {
      direction.x / magnitude,
      direction.y / magnitude,
      direction.z / magnitude};
}

}  // namespace

double orb_distance_to_in_mm(
    const PortableOrb& orb,
    const OrbVector3 position_mm,
    const OrbVector3 direction) {
  const double radius = validated_radius(orb);
  validate_vector(position_mm);
  const OrbVector3 ray = unit_direction(direction);

  const double r = norm(position_mm);
  if (r <= radius) {
    return 0.0;
  }

  // The quadratic is t^2 + 2*b*t + c = 0 for a unit ray.
  // The ratio form for the near root avoids cancellation at large distances.
  const double b = dot(position_mm, ray);
  if (b >= 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  const double c = (r - radius) * (r + radius);
  const double discriminant = b * b - c;
  if (!(discriminant > 0.0)) {
    // A tangent does not cross into the sphere.
    return std::numeric_limits<double>::infinity();
  }
  return c / (-b + std::sqrt(discriminant));
}

double orb_distance_to_out_mm(
    const PortableOrb& orb,
    const OrbVector3 position_mm,
    const OrbVector3 direction) {
  const double radius = validated_radius(orb);
  validate_vector(position_mm);
  const OrbVector3 ray = unit_direction(direction);
  const double r = norm(position_mm);

  if (r > radius) {
    throw std::invalid_argument(
        "orb distance to out requires an inside or surface point");
  }

  const double b = dot(position_mm, ray);
  const double c = (r - radius) * (r + radius);
  const double discriminant = b * b - c;
  return std::max(0.0, -b + std::sqrt(discriminant));
}

double orb_safety_to_in_mm(
    const PortableOrb& orb,
    const OrbVector3 position_mm) {
  const double radius = validated_radius(orb);
  validate_vector(position_mm);
  return std::max(0.0, norm(position_mm) - radius);
}

double orb_safety_to_out_mm(
    const PortableOrb& orb,
    const OrbVector3 position_mm) {
  const double radius = validated_radius(orb);
  validate_vector(position_mm);
  return std::max(0.0, radius - norm(position_mm));
}

}  // namespace g4wgpu
