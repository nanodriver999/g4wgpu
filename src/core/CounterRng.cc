#include "g4wgpu/CounterRng.hh"

namespace g4wgpu {
namespace {

constexpr std::uint32_t rotl32(const std::uint32_t x,
                               const unsigned int r) noexcept {
  return (x << r) | (x >> (32u - r));
}

constexpr std::uint32_t mix32(std::uint32_t x) noexcept {
  x ^= x >> 16u;
  x *= 0x7feb352du;
  x ^= x >> 15u;
  x *= 0x846ca68bu;
  x ^= x >> 16u;
  return x;
}

}  // namespace

std::uint32_t CounterRng::sample_u32(const RngAddress& address,
                                     const std::uint32_t lane) noexcept {
  std::uint32_t x = address.counter_lo;
  x ^= rotl32(address.counter_hi, 13u);
  x ^= rotl32(address.stream_lo, 7u);
  x ^= rotl32(address.stream_hi, 19u);
  x ^= 0x9e3779b9u * (lane + 1u);

  x = mix32(x);
  x ^= mix32(address.stream_lo + 0x85ebca6bu);
  x = mix32(x ^ address.stream_hi);
  return x;
}

float CounterRng::uniform_f32(const RngAddress& address,
                              const std::uint32_t lane) noexcept {
  const std::uint32_t bits = sample_u32(address, lane) >> 8u;
  return static_cast<float>(bits) * (1.0f / 16777216.0f);
}

void CounterRng::increment(RngAddress& address) noexcept {
  ++address.counter_lo;
  if (address.counter_lo == 0u) {
    ++address.counter_hi;
  }
}

}  // namespace g4wgpu
