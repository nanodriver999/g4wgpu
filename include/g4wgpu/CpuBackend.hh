#pragma once

#include "g4wgpu/ComputeBackend.hh"
#include "g4wgpu/PhysicsBackend.hh"

namespace g4wgpu {

class CpuBackend final : public ComputeBackend, public PhysicsBackend {
 public:
  [[nodiscard]] std::string_view name() const noexcept override;

  void axpy(double a, const std::vector<double>& x,
            std::vector<double>& y) override;

  void advance_positions(TrackBatch& tracks, double dt) override;

  std::vector<KleinNishinaSample> sample_klein_nishina_batch(
      const std::vector<double>& incident_gamma_energy_mev,
      std::vector<RngAddress>& rng,
      std::uint32_t max_iterations = 1000u) override;
};

}  // namespace g4wgpu
