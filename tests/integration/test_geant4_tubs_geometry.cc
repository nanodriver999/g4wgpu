#include "g4wgpu/TubsGeometry.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Tubs.hh"

namespace {

bool same_distance(double actual, double expected) {
  return std::fabs(actual - expected) <= 1.0e-8;
}

}  // namespace

int main() {
  using V = g4wgpu::TubsVector3;
  const g4wgpu::PortableSolidTubs portable{10.0, 20.0};
  // Start with the full-azimuth, rmin=0 subset of G4Tubs.
  G4Tubs geant4(
      "reference_solid_tubs",
      0.0 * CLHEP::mm,
      10.0 * CLHEP::mm,
      20.0 * CLHEP::mm,
      0.0,
      CLHEP::twopi);

  struct Ray {
    V position;
    V direction;
  };
  const Ray external[] = {
      {{15.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
      {{0.0, -15.0, 0.0}, {0.0, 1.0, 0.0}},
      {{0.0, 0.0, 30.0}, {0.0, 0.0, -1.0}},
      {{15.0, 0.0, 30.0}, {-1.0, 0.0, -1.0}},
      {{0.0, 0.0, -24.0}, {0.0, 0.0, 1.0}},
      {{10.001, 0.0, 0.0}, {-1.0, 0.0, 0.0}}
  };
  for (const Ray& ray : external) {
    const auto p = ray.position;
    const auto v = ray.direction;
    const G4ThreeVector gp(
        p.x * CLHEP::mm, p.y * CLHEP::mm, p.z * CLHEP::mm);
    const G4ThreeVector gv =
        G4ThreeVector(v.x, v.y, v.z).unit();

    if (!same_distance(
            g4wgpu::tubs_distance_to_in_mm(portable, p, v),
            geant4.DistanceToIn(gp, gv) / CLHEP::mm) ||
        !same_distance(
            g4wgpu::tubs_safety_to_in_mm(portable, p),
            geant4.DistanceToIn(gp) / CLHEP::mm)) {
      std::cerr << "portable Tubs entry/safety disagrees with G4Tubs\n";
      return EXIT_FAILURE;
    }
  }

  const Ray internal[] = {
      {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}},
      {{3.0, 4.0, 0.0}, {1.0, 0.0, 0.0}},
      {{0.0, 0.0, 15.0}, {0.0, 0.0, 1.0}},
      {{1.0, 2.0, 19.999}, {0.0, 0.0, 1.0}},
      {{5.0, 0.0, 10.0}, {1.0, 0.0, 1.0}}
  };
  for (const Ray& ray : internal) {
    const auto p = ray.position;
    const auto v = ray.direction;
    const G4ThreeVector gp(
        p.x * CLHEP::mm, p.y * CLHEP::mm, p.z * CLHEP::mm);
    const G4ThreeVector gv =
        G4ThreeVector(v.x, v.y, v.z).unit();
    if (!same_distance(
            g4wgpu::tubs_distance_to_out_mm(portable, p, v),
            geant4.DistanceToOut(gp, gv) / CLHEP::mm) ||
        !same_distance(
            g4wgpu::tubs_safety_to_out_mm(portable, p),
            geant4.DistanceToOut(gp) / CLHEP::mm)) {
      std::cerr << "portable Tubs exit/safety disagrees with G4Tubs\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
