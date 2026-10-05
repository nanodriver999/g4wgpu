#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace g4wgpu {

struct TrackBatch {
  std::vector<float> position_x;
  std::vector<float> position_y;
  std::vector<float> position_z;

  std::vector<float> direction_x;
  std::vector<float> direction_y;
  std::vector<float> direction_z;

  std::vector<float> kinetic_energy;

  std::vector<std::uint32_t> particle_id;
  std::vector<std::uint32_t> material_id;
  std::vector<std::uint32_t> rng_counter_lo;
  std::vector<std::uint32_t> rng_counter_hi;
  std::vector<std::uint32_t> status;

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  void resize(std::size_t count);

  // Throws std::invalid_argument when one of the SoA arrays has a
  // different length. Keeping validation explicit makes CPU/GPU boundary
  // bugs fail before a dispatch.
  void validate() const;
};

}  // namespace g4wgpu
