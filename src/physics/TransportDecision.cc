#include "g4wgpu/TransportDecision.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace g4wgpu {
namespace {

void validate_distance(
    const double value,
    const char* name) {
  if (std::isnan(value) || value < 0.0) {
    throw std::invalid_argument(
        std::string(name) + " must be non-negative or infinity");
  }
}

}  // namespace

TransportDecision choose_transport_limiter(
    const double interaction_distance_mm,
    const double boundary_distance_mm) {
  validate_distance(
      interaction_distance_mm,
      "interaction distance");
  validate_distance(
      boundary_distance_mm,
      "boundary distance");

  const bool interaction_finite =
      std::isfinite(interaction_distance_mm);
  const bool boundary_finite =
      std::isfinite(boundary_distance_mm);

  if (!interaction_finite && !boundary_finite) {
    return TransportDecision{
        TransportLimiter::none,
        std::numeric_limits<double>::infinity(),
        interaction_distance_mm,
        boundary_distance_mm};
  }

  if (interaction_finite &&
      (!boundary_finite ||
       interaction_distance_mm < boundary_distance_mm)) {
    return TransportDecision{
        TransportLimiter::interaction,
        interaction_distance_mm,
        interaction_distance_mm,
        boundary_distance_mm};
  }

  return TransportDecision{
      TransportLimiter::geometry_boundary,
      boundary_distance_mm,
      interaction_distance_mm,
      boundary_distance_mm};
}

}  // namespace g4wgpu
