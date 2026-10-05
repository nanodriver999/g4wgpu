#include "g4wgpu/BatchOps.hh"

#include "g4wgpu/TrackStatus.hh"

namespace g4wgpu {

std::vector<std::size_t> active_indices(const TrackBatch& tracks) {
  tracks.validate();

  std::vector<std::size_t> indices;
  indices.reserve(tracks.size());

  for (std::size_t i = 0; i < tracks.size(); ++i) {
    if (tracks.status[i] ==
        static_cast<std::uint32_t>(TrackStatus::Active)) {
      indices.push_back(i);
    }
  }

  return indices;
}

TrackBatch compact_active(const TrackBatch& tracks) {
  const auto indices = active_indices(tracks);

  TrackBatch result;
  result.resize(indices.size());

  for (std::size_t out = 0; out < indices.size(); ++out) {
    const std::size_t in = indices[out];

    result.position_x[out] = tracks.position_x[in];
    result.position_y[out] = tracks.position_y[in];
    result.position_z[out] = tracks.position_z[in];

    result.direction_x[out] = tracks.direction_x[in];
    result.direction_y[out] = tracks.direction_y[in];
    result.direction_z[out] = tracks.direction_z[in];

    result.kinetic_energy[out] = tracks.kinetic_energy[in];

    result.particle_id[out] = tracks.particle_id[in];
    result.material_id[out] = tracks.material_id[in];

    result.rng_stream_lo[out] = tracks.rng_stream_lo[in];
    result.rng_stream_hi[out] = tracks.rng_stream_hi[in];
    result.rng_counter_lo[out] = tracks.rng_counter_lo[in];
    result.rng_counter_hi[out] = tracks.rng_counter_hi[in];

    result.status[out] = tracks.status[in];
  }

  return result;
}

}  // namespace g4wgpu
