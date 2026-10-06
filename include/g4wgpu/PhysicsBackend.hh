#pragma once

#include <vector>

#include "g4wgpu/CounterRng.hh"
#include "g4wgpu/KleinNishina.hh"

namespace g4wgpu {

class PhysicsBackend {
 public:
  virtual ~PhysicsBackend() = default;

  virtual std::vector<KleinNishinaSample> sample_klein_nishina_batch(
      const std::vector<double>& incident_gamma_energy_mev,
      std::vector<RngAddress>& rng,
      std::uint32_t max_iterations = 1000u) = 0;
};

}  // namespace g4wgpu
