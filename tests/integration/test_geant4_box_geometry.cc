#include "g4wgpu/BoxGeometry.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "G4Box.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"

namespace {

bool same_distance(const double actual, const double expected) {
  return (std::isinf(actual) && std::isinf(expected)) ||
         std::fabs(actual - expected) <= 1.0e-9;
}

}  // namespace

int main() {
  const g4wgpu::AxisAlignedBox portable{{2.0, 3.0, 4.0}};
  G4Box geant4("reference_box", 2.0 * CLHEP::mm,
              3.0 * CLHEP::mm, 4.0 * CLHEP::mm);
  using V = g4wgpu::BoxVector3;

  const V outside_points[] = {
      {5.0, 0.0, 0.0}, {-5.0, 0.0, 0.0},
      {0.0, 7.0, 0.0}, {0.0, 0.0, 8.0}};
  const V outside_dirs[] = {
      {-1.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
      {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}};
  for (int i = 0; i < 4; ++i) {
    const auto p = outside_points[i];
    const auto v = outside_dirs[i];
    const G4ThreeVector gp(p.x * CLHEP::mm, p.y * CLHEP::mm,
                           p.z * CLHEP::mm);
    const G4ThreeVector gv(v.x, v.y, v.z);
    const double distance = geant4.DistanceToIn(gp, gv) / CLHEP::mm;
    const double safety = geant4.DistanceToIn(gp) / CLHEP::mm;
    if (!same_distance(
            g4wgpu::box_distance_to_in_mm(portable, p, v), distance) ||
        !same_distance(
            g4wgpu::box_safety_to_in_mm(portable, p), safety)) {
      std::cerr << "portable Box entry/safety disagrees with Geant4\n";
      return EXIT_FAILURE;
    }
  }

  const V inside_points[] = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
      {0.0, -1.0, 0.0}, {0.0, 0.0, 3.0}};
  const V inside_dirs[] = {
      {1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  for (int i = 0; i < 4; ++i) {
    const auto p = inside_points[i];
    const auto v = inside_dirs[i];
    const G4ThreeVector gp(p.x * CLHEP::mm, p.y * CLHEP::mm,
                           p.z * CLHEP::mm);
    const G4ThreeVector gv(v.x, v.y, v.z);
    const double distance = geant4.DistanceToOut(gp, gv) / CLHEP::mm;
    const double safety = geant4.DistanceToOut(gp) / CLHEP::mm;
    if (!same_distance(
            g4wgpu::box_distance_to_out_mm(portable, p, v), distance) ||
        !same_distance(
            g4wgpu::box_safety_to_out_mm(portable, p), safety)) {
      std::cerr << "portable Box exit/safety disagrees with Geant4\n";
      return EXIT_FAILURE;
    }
  }

  const double miss = g4wgpu::box_distance_to_in_mm(
      portable, V{5.0, 5.0, 0.0}, V{-1.0, 0.0, 0.0});
  if (!std::isinf(miss)) {
    std::cerr << "a parallel ray outside the Box should miss\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
