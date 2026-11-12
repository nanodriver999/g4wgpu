#include "g4wgpu/FlatBoxNavigation.hh"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace g4wgpu {
namespace {

void validate_half_lengths(const AxisAlignedBox& box) {
  const auto h = box.half_lengths_mm;
  if (!std::isfinite(h.x) || !(h.x > 0.0) ||
      !std::isfinite(h.y) || !(h.y > 0.0) ||
      !std::isfinite(h.z) || !(h.z > 0.0)) {
    throw std::invalid_argument(
        "flat Box half lengths must be finite and positive");
  }
}

void validate_scene(const FlatBoxScene& scene) {
  validate_half_lengths(scene.world);
  const auto world = scene.world.half_lengths_mm;
  std::unordered_set<std::uint32_t> ids;

  for (const FlatBoxPlacement& daughter : scene.daughters) {
    validate_half_lengths(daughter.box);
    const auto h = daughter.box.half_lengths_mm;
    const auto c = daughter.center_mm;
    if (daughter.id == 0 || !ids.insert(daughter.id).second) {
      throw std::invalid_argument(
          "flat Box daughter IDs must be unique and nonzero");
    }
    if (!std::isfinite(c.x) || !std::isfinite(c.y) ||
        !std::isfinite(c.z) ||
        !(h.x < world.x && std::fabs(c.x) < world.x - h.x) ||
        !(h.y < world.y && std::fabs(c.y) < world.y - h.y) ||
        !(h.z < world.z && std::fabs(c.z) < world.z - h.z)) {
      throw std::invalid_argument(
          "flat Box daughter must fit strictly inside world");
    }
  }

  // A flat one-level scene has no nested daughters; touching or
  // overlapping volumes are intentionally rejected as ambiguous.
  for (std::size_t i = 0; i < scene.daughters.size(); ++i) {
    const auto& a = scene.daughters[i];
    for (std::size_t j = i + 1; j < scene.daughters.size(); ++j) {
      const auto& b = scene.daughters[j];
      const auto ac = a.center_mm;
      const auto bc = b.center_mm;
      const auto ah = a.box.half_lengths_mm;
      const auto bh = b.box.half_lengths_mm;
      const bool separated =
          std::fabs(ac.x - bc.x) > ah.x + bh.x ||
          std::fabs(ac.y - bc.y) > ah.y + bh.y ||
          std::fabs(ac.z - bc.z) > ah.z + bh.z;
      if (!separated) {
        throw std::invalid_argument(
            "flat Box daughters must be separated");
      }
    }
  }
}

bool inside_or_on(const AxisAlignedBox& box, BoxVector3 p) {
  const auto h = box.half_lengths_mm;
  return std::isfinite(p.x) && std::isfinite(p.y) &&
         std::isfinite(p.z) &&
         std::fabs(p.x) <= h.x && std::fabs(p.y) <= h.y &&
         std::fabs(p.z) <= h.z;
}

BoxVector3 to_local(BoxVector3 global, BoxVector3 center) {
  return {
      global.x - center.x,
      global.y - center.y,
      global.z - center.z};
}

}  // namespace

FlatBoundary flat_box_next_boundary_mm(
    const FlatBoxScene& scene,
    const BoxVector3 position_mm,
    const BoxVector3 direction) {
  validate_scene(scene);
  if (!inside_or_on(scene.world, position_mm)) {
    throw std::invalid_argument(
        "flat Box ray must start in/on world");
  }

  // This validates direction (including NaNs and zero length), and
  // returns a physical, direction-scale invariant distance.
  const double world_distance =
      box_distance_to_out_mm(scene.world, position_mm, direction);

  for (const auto& daughter : scene.daughters) {
    const auto local = to_local(position_mm, daughter.center_mm);
    if (inside_or_on(daughter.box, local)) {
      return FlatBoundary{
          FlatBoundaryKind::daughter_exit,
          box_distance_to_out_mm(daughter.box, local, direction),
          daughter.id};
    }
  }

  FlatBoundary nearest{
      FlatBoundaryKind::world_exit, world_distance, 0};
  for (const auto& daughter : scene.daughters) {
    const auto local = to_local(position_mm, daughter.center_mm);
    const double distance =
        box_distance_to_in_mm(daughter.box, local, direction);
    if (distance < nearest.distance_mm) {
      nearest = FlatBoundary{
          FlatBoundaryKind::daughter_entry, distance, daughter.id};
    }
  }
  return nearest;
}

}  // namespace g4wgpu
