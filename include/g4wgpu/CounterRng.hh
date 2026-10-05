#pragma once

#include <cstdint>

namespace g4wgpu {

struct RngAddress {
  std::uint32_t stream_lo = 0;
  std::uint32_t stream_hi = 0;
  std::uint32_t counter_lo = 0;
  std::uint32_t counter_hi = 0;
};

class CounterRng {
 public:
  static std::uint32_t sample_u32(const RngAddress& address,
                                  std::uint32_t lane = 0) noexcept;

  // Returns an exactly representable 24-bit mantissa fraction in [0, 1).
  // This is chosen so the CPU reference can be matched by portable WGSL f32.
  static float uniform_f32(const RngAddress& address,
                           std::uint32_t lane = 0) noexcept;

  static void increment(RngAddress& address) noexcept;
};

}  // namespace g4wgpu
