#include "g4wgpu/TrackBatch.hh"

#include <array>
#include <stdexcept>

namespace g4wgpu {

std::size_t TrackBatch::size() const noexcept {
  return position_x.size();
}

bool TrackBatch::empty() const noexcept {
  return size() == 0;
}

void TrackBatch::resize(const std::size_t count) {
  position_x.resize(count);
  position_y.resize(count);
  position_z.resize(count);

  direction_x.resize(count);
  direction_y.resize(count);
  direction_z.resize(count);

  kinetic_energy.resize(count);

  particle_id.resize(count);
  material_id.resize(count);
  rng_stream_lo.resize(count);
  rng_stream_hi.resize(count);
  rng_counter_lo.resize(count);
  rng_counter_hi.resize(count);
  status.resize(count);
}

void TrackBatch::validate() const {
  const auto expected = size();

  const std::array<std::size_t, 13> sizes = {
      position_y.size(),       position_z.size(),       direction_x.size(),
      direction_y.size(),      direction_z.size(),      kinetic_energy.size(),
      particle_id.size(),      material_id.size(),      rng_stream_lo.size(),
      rng_stream_hi.size(),    rng_counter_lo.size(),   rng_counter_hi.size(),
      status.size(),
  };

  for (const auto actual : sizes) {
    if (actual != expected) {
      throw std::invalid_argument(
          "TrackBatch contains SoA arrays with inconsistent lengths");
    }
  }
}

}  // namespace g4wgpu
