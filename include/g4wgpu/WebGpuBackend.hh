#pragma once

#include <memory>
#include <string_view>

#include "g4wgpu/BoxGeometry.hh"
#include "g4wgpu/ComputeBackend.hh"
#include "g4wgpu/PhysicsBackend.hh"

namespace g4wgpu {

class WebGpuBackend final : public ComputeBackend, public PhysicsBackend {
 public:
  WebGpuBackend();
  ~WebGpuBackend() override;

  WebGpuBackend(const WebGpuBackend&) = delete;
  WebGpuBackend& operator=(const WebGpuBackend&) = delete;
  WebGpuBackend(WebGpuBackend&&) noexcept;
  WebGpuBackend& operator=(WebGpuBackend&&) noexcept;

  [[nodiscard]] std::string_view name() const noexcept override;

  void axpy(double a, const std::vector<double>& x,
            std::vector<double>& y) override;

  void advance_positions(TrackBatch& tracks, double dt) override;

  // Experimental standalone Box ray-entry kernel: the result is advisory
  // and is never promoted into Geant4 navigation without further validation.
  std::vector<double> box_distance_to_in_batch(
      const AxisAlignedBox& box, const std::vector<BoxRay>& rays);

  std::vector<KleinNishinaSample> sample_klein_nishina_batch(
      const std::vector<double>& incident_gamma_energy_mev,
      std::vector<RngAddress>& rng,
      std::uint32_t max_iterations = 1000u) override;

  std::vector<GammaInteractionSample>
  sample_gamma_process_competition_batch(
      const std::vector<GammaProcessCrossSections>& cross_sections,
      std::vector<RngAddress>& rng) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace g4wgpu
