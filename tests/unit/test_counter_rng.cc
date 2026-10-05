#include "g4wgpu/BatchOps.hh"
#include "g4wgpu/CounterRng.hh"
#include "g4wgpu/TrackStatus.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
  {
    g4wgpu::RngAddress address{
        0x12345678u, 0x9abcdef0u, 0u, 0u};

    const auto a = g4wgpu::CounterRng::sample_u32(address, 0u);
    const auto b = g4wgpu::CounterRng::sample_u32(address, 0u);
    if (a != 0x409066f8u) {
      std::cerr << "counter RNG reference vector changed\n";
      return EXIT_FAILURE;
    }
    if (a != b) {
      std::cerr << "counter RNG is not deterministic\n";
      return EXIT_FAILURE;
    }

    const float u = g4wgpu::CounterRng::uniform_f32(address, 0u);
    if (!(u >= 0.0f && u < 1.0f)) {
      std::cerr << "uniform_f32 outside [0, 1)\n";
      return EXIT_FAILURE;
    }

    const auto before = g4wgpu::CounterRng::sample_u32(address, 0u);
    g4wgpu::CounterRng::increment(address);
    const auto after = g4wgpu::CounterRng::sample_u32(address, 0u);
    if (before == after) {
      std::cerr << "counter increment did not change sample\n";
      return EXIT_FAILURE;
    }
  }

  {
    g4wgpu::RngAddress address{
        1u, 2u, 0xffffffffu, 7u};
    g4wgpu::CounterRng::increment(address);
    if (address.counter_lo != 0u || address.counter_hi != 8u) {
      std::cerr << "64-bit counter carry failed\n";
      return EXIT_FAILURE;
    }
  }

  {
    g4wgpu::TrackBatch tracks;
    tracks.resize(3);

    tracks.position_x = {10.0, 20.0, 30.0};
    tracks.particle_id = {11u, 22u, 33u};
    tracks.rng_stream_lo = {101u, 202u, 303u};
    tracks.rng_stream_hi = {1u, 2u, 3u};
    tracks.rng_counter_lo = {5u, 6u, 7u};
    tracks.rng_counter_hi = {0u, 0u, 0u};

    tracks.status = {
        static_cast<std::uint32_t>(g4wgpu::TrackStatus::Active),
        static_cast<std::uint32_t>(g4wgpu::TrackStatus::Killed),
        static_cast<std::uint32_t>(g4wgpu::TrackStatus::Active),
    };

    const auto compact = g4wgpu::compact_active(tracks);
    if (compact.size() != 2u) {
      std::cerr << "active compaction size mismatch\n";
      return EXIT_FAILURE;
    }
    if (compact.particle_id[0] != 11u ||
        compact.particle_id[1] != 33u ||
        compact.rng_stream_lo[0] != 101u ||
        compact.rng_stream_lo[1] != 303u ||
        compact.rng_counter_lo[1] != 7u) {
      std::cerr << "active compaction did not preserve identity/RNG state\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
