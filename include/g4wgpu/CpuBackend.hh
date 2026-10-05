#pragma once

#include "g4wgpu/ComputeBackend.hh"

namespace g4wgpu {

class CpuBackend final : public ComputeBackend {
 public:
  [[nodiscard]] std::string_view name() const noexcept override;

  void axpy(float a, const std::vector<float>& x,
            std::vector<float>& y) override;

  void advance_positions(TrackBatch& tracks, double dt) override;
};

}  // namespace g4wgpu
