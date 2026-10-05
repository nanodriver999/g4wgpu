#include "g4wgpu/TrackingPolicy.hh"

#include <stdexcept>

namespace g4wgpu {

void TrackingPolicy::validate() const {
  if (min_gamma_energy_mev < 0.0) {
    throw std::invalid_argument(
        "min_gamma_energy_mev must be non-negative");
  }
  if (batch_capacity == 0u) {
    throw std::invalid_argument(
        "batch_capacity must be greater than zero");
  }
}

}  // namespace g4wgpu
