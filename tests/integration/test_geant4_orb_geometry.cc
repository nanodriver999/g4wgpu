#include "g4wgpu/OrbGeometry.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "G4Orb.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"

namespace {

bool same_distance(double actual, double expected) {
  return std::fabs(actual - expected) <= 1.0e-8;
}

}  // namespace

int main() {
  using V = g4wgpu::OrbVector3;
  const g4wgpu::PortableOrb portable{10.0};
  G4Orb geant4("reference_orb", 10.0 * CLHEP::mm);

  struct Ray {
    V position;
    V direction;
  };
  const Ray external[] = {
      {{15.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{0.0, -20.0, 0.0}, {0.0, 1.0, 0.0}},
      {{12.0, 5.0, 0.0}, {-12.0, -5.0, 0.0}},
      {{0.0, 0.0, 40.0}, {0.0, 0.0, -1.0}},
      {{10.001, 0.0, 0.0}, {-1.0, 0.0, 0.0}}
  };
  for (const auto& ray : external) {
    const auto p = ray.position;
    const auto v = ray.direction;
    const G4ThreeVector gp(
        p.x * CLHEP::mm, p.y * CLHEP::mm, p.z * CLHEP::mm);
    const G4ThreeVector gv =
        G4ThreeVector(v.x, v.y, v.z).unit();
    if (!same_distance(
            g4wgpu::orb_distance_to_in_mm(portable, p, v),
            geant4.DistanceToIn(gp, gv) / CLHEP::mm) ||
        !same_distance(
            g4wgpu::orb_safety_to_in_mm(portable, p),
            geant4.DistanceToIn(gp) / CLHEP::mm)) {
      std::cerr << "portable Orb entry differs from G4Orb\n";
      return EXIT_FAILURE;
    }
  }

  const Ray internal[] = {
      {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, 5.0}, {0.0, 0.0, -1.0}},
      {{4.0, 3.0, 0.0}, {-1.0, 2.0, 0.0}},
      {{-2.0, 3.0, 1.0}, {0.0, 0.0, 1.0}},
      {{0.0, 0.0, 9.999}, {0.0, 0.0, 1.0}}
  };
  for (const auto& ray : internal) {
    const auto p = ray.position;
    const auto v = ray.direction;
    const G4ThreeVector gp(
        p.x * CLHEP::mm, p.y * CLHEP::mm, p.z * CLHEP::mm);
    const G4ThreeVector gv =
        G4ThreeVector(v.x, v.y, v.z).unit();
    if (!same_distance(
            g4wgpu::orb_distance_to_out_mm(portable, p, v),
            geant4.DistanceToOut(gp, gv) / CLHEP::mm) ||
        !same_distance(
            g4wgpu::orb_safety_to_out_mm(portable, p),
            geant4.DistanceToOut(gp) / CLHEP::mm)) {
      std::cerr << "portable Orb exit differs from G4Orb\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
