#pragma once

#include <string_view>
#include <vector>

#include "g4wgpu/TrackBatch.hh"

namespace g4wgpu {

class ComputeBackend {
 public:
  virtual ~ComputeBackend() = default;

  [[nodiscard]] virtual std::string_view name() const noexcept = 0;

  // Small backend-independent smoke kernel. It exists to validate the
  // backend/runtime path before any Geant4 physics is introduced.
  virtual void axpy(double a, const std::vector<double>& x,
                    std::vector<double>& y) = 0;

  // Representative track-batch operation used to establish the data
  // boundary that future WebGPU kernels will consume.
  virtual void advance_positions(TrackBatch& tracks, double dt) = 0;
};

}  // namespace g4wgpu
