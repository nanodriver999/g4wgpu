#pragma once

#include <cstdint>

namespace g4wgpu {

enum class TrackStatus : std::uint32_t {
  Active = 0,
  Killed = 1,
  Suspended = 2,
  CpuFallback = 3,
  Error = 4,
};

}  // namespace g4wgpu
