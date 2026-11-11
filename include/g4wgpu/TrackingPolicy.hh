#pragma once

#include <cstddef>

namespace g4wgpu {

struct TrackingPolicy {
  double min_gamma_energy_mev = 0.0;
  std::size_t batch_capacity = 1024;
  // Experimental opt-in: when a sampled Compton interaction wins before the
  // next geometry boundary, apply the validated final state to Geant4 and
  // create the recoil electron. Disabled by default.
  bool enable_authoritative_compton = false;

  [[nodiscard]] bool should_buffer_gamma(
      double kinetic_energy_mev) const noexcept {
    return kinetic_energy_mev >= min_gamma_energy_mev;
  }

  void validate() const;
};

}  // namespace g4wgpu
