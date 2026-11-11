#include "g4wgpu/TrackingPolicy.hh"

#include <cmath>
#include <stdexcept>

namespace g4wgpu {

void TrackingPolicy::validate() const {
  if (!std::isfinite(min_gamma_energy_mev) ||
      min_gamma_energy_mev < 0.0) {
    throw std::invalid_argument(
        "min_gamma_energy_mev must be finite and non-negative");
  }
  if (batch_capacity == 0u) {
    throw std::invalid_argument(
        "batch_capacity must be greater than zero");
  }
}

}  // namespace g4wgpu
