#pragma once

#include <cstdint>
#include <vector>

#include "g4wgpu/BoxGeometry.hh"

namespace g4wgpu {

// First PR 8 flattened-navigation experiment: an axis-aligned Box world
// with non-overlapping, unrotated, translated Box daughters. All coordinates
// and distances are millimeters. ID 0 is reserved for the world.
struct FlatBoxPlacement {
  std::uint32_t id = 0;
  AxisAlignedBox box{};
  BoxVector3 center_mm{};
};

struct FlatBoxScene {
  AxisAlignedBox world{};
  std::vector<FlatBoxPlacement> daughters;
};

enum class FlatBoundaryKind : std::uint32_t {
  world_exit = 0,
  daughter_entry = 1,
  daughter_exit = 2,
};

struct FlatBoundary {
  FlatBoundaryKind kind = FlatBoundaryKind::world_exit;
  double distance_mm = 0.0;
  std::uint32_t daughter_id = 0;  // 0 if kind == world_exit
};

// Return the nearest geometry boundary along the ray. Ray origins must be
// inside/on the world. Daughters must have positive dimensions, be strictly
// contained in the world, have distinct nonzero IDs, and be disjoint.
//
// A ray in a daughter exits that daughter; otherwise its next boundary is
// the nearest daughter entry or the world exit. Exact ties favor the world.
// The reference performs full scene validation on every call; it is not a
// performance-optimized navigation or a replacement for G4Navigator.
//
// Surface starts can be ambiguous in Geant4; correctness comparisons use
// interior points with nonzero clearance from all boundaries.
FlatBoundary flat_box_next_boundary_mm(
    const FlatBoxScene& scene,
    BoxVector3 position_mm,
    BoxVector3 direction);

}  // namespace g4wgpu
