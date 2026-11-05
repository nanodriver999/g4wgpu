#pragma once

#include <cstdint>

namespace g4wgpu {

enum class TransportLimiter : std::uint32_t {
  none = 0,
  interaction = 1,
  geometry_boundary = 2,
};

struct TransportDecision {
  TransportLimiter limiter = TransportLimiter::none;
  double step_distance_mm = 0.0;
  double interaction_distance_mm = 0.0;
  double boundary_distance_mm = 0.0;
};

// Chooses the next transport limiter from an already sampled physics
// interaction distance and the distance to the next geometry boundary.
//
// Infinite distances are allowed. If both are infinite there is no limiter.
// Exact ties are assigned to geometry to avoid placing an interaction beyond
// or ambiguously on a boundary.
TransportDecision choose_transport_limiter(
    double interaction_distance_mm,
    double boundary_distance_mm);

}  // namespace g4wgpu
