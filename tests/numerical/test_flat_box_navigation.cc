#include "g4wgpu/FlatBoxNavigation.hh"

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
