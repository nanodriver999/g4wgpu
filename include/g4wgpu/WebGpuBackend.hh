#pragma once

#include <memory>
#include <string_view>

#include "g4wgpu/ComputeBackend.hh"

namespace g4wgpu {

class WebGpuBackend final : public ComputeBackend {
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

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace g4wgpu
