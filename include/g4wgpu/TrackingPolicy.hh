#pragma once

#include <cstddef>

namespace g4wgpu {

struct TrackingPolicy {
  double min_gamma_energy_mev = 0.0;
  std::size_t batch_capacity = 1024;

  [[nodiscard]] bool should_buffer_gamma(
      double kinetic_energy_mev) const noexcept {
    return kinetic_energy_mev >= min_gamma_energy_mev;
  }

  void validate() const;
};

}  // namespace g4wgpu
