#include "g4wgpu/CpuBackend.hh"

#include <stdexcept>

namespace g4wgpu {

std::string_view CpuBackend::name() const noexcept {
  return "cpu";
}

void CpuBackend::axpy(const float a, const std::vector<float>& x,
                      std::vector<float>& y) {
  if (x.size() != y.size()) {
    throw std::invalid_argument("axpy requires x and y to have equal lengths");
  }

  for (std::size_t i = 0; i < x.size(); ++i) {
    y[i] = a * x[i] + y[i];
  }
}

void CpuBackend::advance_positions(TrackBatch& tracks, const float dt) {
  tracks.validate();

  for (std::size_t i = 0; i < tracks.size(); ++i) {
    tracks.position_x[i] += tracks.direction_x[i] * dt;
    tracks.position_y[i] += tracks.direction_y[i] * dt;
    tracks.position_z[i] += tracks.direction_z[i] * dt;
  }
}

}  // namespace g4wgpu
