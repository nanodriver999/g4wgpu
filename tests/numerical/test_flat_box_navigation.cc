#include "g4wgpu/FlatBoxNavigation.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using V = g4wgpu::BoxVector3;
using Kind = g4wgpu::FlatBoundaryKind;
using Scene = g4wgpu::FlatBoxScene;

bool matches(const g4wgpu::FlatBoundary& result,
             const Kind kind, const double expected,
             const std::uint32_t id) {
  return result.kind == kind && result.daughter_id == id &&
         std::fabs(result.distance_mm - expected) <= 1.0e-9;
}

Scene reference_scene() {
  Scene s;
  s.world = {{100.0, 100.0, 100.0}};
  s.daughters = {
      {11u, {{8.0, 10.0, 12.0}}, {-35.0, 0.0, 0.0}},
      {22u, {{10.0, 8.0, 14.0}}, {25.0, 0.0, 0.0}},
  };
  return s;
}

template<class Fn>
bool rejects(Fn&& fn) {
  try {
    fn();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace

int main() {
  const Scene scene = reference_scene();
  const auto next = [&](V p, V d) {
    return g4wgpu::flat_box_next_boundary_mm(scene, p, d);
  };

  if (!matches(next({0,0,0}, {1,0,0}),
               Kind::daughter_entry, 15.0, 22u) ||
      !matches(next({0,0,0}, {-1,0,0}),
               Kind::daughter_entry, 27.0, 11u) ||
      !matches(next({0,0,0}, {0,1,0}),
               Kind::world_exit, 100.0, 0u) ||
      !matches(next({0,0,30}, {0,0,1}),
               Kind::world_exit, 70.0, 0u) ||
      !matches(next({-35,0,0}, {1,0,0}),
               Kind::daughter_exit, 8.0, 11u) ||
      !matches(next({25,0,0}, {-1,0,0}),
               Kind::daughter_exit, 10.0, 22u) ||
      !matches(next({60,0,0}, {-1,0,0}),
               Kind::daughter_entry, 25.0, 22u) ||
      !matches(next({-50,30,0}, {1,0,0}),
               Kind::world_exit, 150.0, 0u) ||
      !matches(next({0,0,0}, {2,0,0}),
               Kind::daughter_entry, 15.0, 22u) ||
      !matches(next({-35,0,0}, {-2,0,0}),
               Kind::daughter_exit, 8.0, 11u)) {
    std::cerr << "flat Box next-boundary routing mismatch\n";
    return EXIT_FAILURE;
  }

  // Selection must depend on geometry, not daughter storage order or
  // a positive rescaling of the direction vector.
  Scene reversed = reference_scene();
  std::swap(reversed.daughters[0], reversed.daughters[1]);
  const V positions[] = {{0,0,0}, {60,0,0}, {-35,0,0},
                         {25,0,0}, {-50,30,0}, {0,0,30}};
  const V directions[] = {{1,0,0}, {-1,0,0}, {0,1,0},
                          {0,0,-1}, {1,0.25,0}, {-1,0,-0.5}};
  for (const V p : positions) {
    for (const V d : directions) {
      const auto expected = next(p, d);
      const auto reordered = g4wgpu::flat_box_next_boundary_mm(reversed, p, d);
      const auto scaled = next(p, {d.x * 7.0, d.y * 7.0, d.z * 7.0});
      if (!matches(reordered, expected.kind, expected.distance_mm,
                   expected.daughter_id) ||
          !matches(scaled, expected.kind, expected.distance_mm,
                   expected.daughter_id)) {
        std::cerr << "flat Box order/direction scale invariance mismatch\\n";
        return EXIT_FAILURE;
      }
    }
  }

  // Interior points close to a daughter face must not skip the next
  // boundary or confuse entry with exit. Keep nonzero clearance to avoid
  // Geant4's surface-start ambiguity.
  if (!matches(next({14.999,0,0}, {1,0,0}),
               Kind::daughter_entry, 0.001, 22u) ||
      !matches(next({34.999,0,0}, {1,0,0}),
               Kind::daughter_exit, 0.001, 22u) ||
      !matches(next({-42.999,0,0}, {-1,0,0}),
               Kind::daughter_exit, 0.001, 11u) ||
      !matches(next({0,0,99.999}, {0,0,1}),
               Kind::world_exit, 0.001, 0u)) {
    std::cerr << "flat Box near-boundary clearance mismatch\\n";
    return EXIT_FAILURE;
  }

  Scene world_only;
  world_only.world = scene.world;
  if (!matches(g4wgpu::flat_box_next_boundary_mm(
                   world_only, {0,0,0}, {0,0,-2}),
               Kind::world_exit, 100.0, 0u)) {
    std::cerr << "flat Box world-only exit mismatch\n";
    return EXIT_FAILURE;
  }

  Scene duplicate = reference_scene();
  duplicate.daughters[1].id = 11u;
  Scene touching = reference_scene();
  touching.daughters[1].center_mm = {-17.0, 0.0, 0.0};
  Scene outside = reference_scene();
  outside.daughters[1].center_mm = {95.0, 0.0, 0.0};
  Scene bad_world = reference_scene();
  bad_world.world.half_lengths_mm.x = 0.0;
  const double nan = std::numeric_limits<double>::quiet_NaN();

  if (!rejects([&] { (void)next({101,0,0}, {-1,0,0}); }) ||
      !rejects([&] { (void)next({0,0,0}, {0,0,0}); }) ||
      !rejects([&] { (void)next({nan,0,0}, {1,0,0}); }) ||
      !rejects([&] { (void)next({0,0,0}, {nan,0,0}); }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_mm(
            duplicate, {0,0,0}, {1,0,0});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_mm(
            touching, {0,0,0}, {1,0,0});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_mm(
            outside, {0,0,0}, {1,0,0});
      }) ||
      !rejects([&] {
        (void)g4wgpu::flat_box_next_boundary_mm(
            bad_world, {0,0,0}, {1,0,0});
      })) {
    std::cerr << "flat Box invalid geometry accepted\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
