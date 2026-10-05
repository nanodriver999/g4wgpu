#pragma once

#include "g4wgpu/ComputeBackend.hh"

namespace g4wgpu {

class CpuBackend final : public ComputeBackend {
 public:
  [[nodiscard]] std::string_view name() const noexcept override;

  void axpy(double a, const std::vector<double>& x,
            std::vector<double>& y) override;

  void advance_positions(TrackBatch& tracks, double dt) override;
};

}  // namespace g4wgpu
