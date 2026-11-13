#include "g4wgpu/FlatBoxNavigation.hh"
#include "g4wgpu/geant4/GeometryBoundaryDistance.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "G4Box.hh"
#include "G4DynamicParticle.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4VUserDetectorConstruction.hh"

namespace {

using V = g4wgpu::BoxVector3;
using Kind = g4wgpu::FlatBoundaryKind;

class FlatBoxDetector final : public G4VUserDetectorConstruction {
 public:
  G4VPhysicalVolume* Construct() override {
    auto* vacuum = G4NistManager::Instance()->FindOrBuildMaterial(
        "G4_Galactic");
    auto* world_solid = new G4Box(
        "flat_world", 100 * CLHEP::mm,
        100 * CLHEP::mm, 100 * CLHEP::mm);
    auto* world_logical = new G4LogicalVolume(
        world_solid, vacuum, "flat_world_logical");
    auto* world = new G4PVPlacement(
        nullptr, G4ThreeVector(), world_logical,
        "flat_world_phys", nullptr, false, 0, false);

    auto* first = new G4LogicalVolume(
        new G4Box("first_box", 8 * CLHEP::mm,
                  10 * CLHEP::mm, 12 * CLHEP::mm),
        vacuum, "first_box_logical");
    new G4PVPlacement(
        nullptr, G4ThreeVector(-35 * CLHEP::mm, 0, 0),
        first, "first_box_phys", world_logical,
        false, 11, false);

    auto* second = new G4LogicalVolume(
        new G4Box("second_box", 10 * CLHEP::mm,
                  8 * CLHEP::mm, 14 * CLHEP::mm),
        vacuum, "second_box_logical");
    new G4PVPlacement(
        nullptr, G4ThreeVector(25 * CLHEP::mm, 0, 0),
        second, "second_box_phys", world_logical,
        false, 22, false);
    return world;
  }
};

struct Case {
  V position;
  V direction;
  Kind expected_kind;
  std::uint32_t expected_id;
};

}  // namespace

int main() {
  auto* run_manager = new G4RunManager();
  run_manager->SetUserInitialization(new FlatBoxDetector());
  run_manager->InitializeGeometry();

  g4wgpu::FlatBoxScene scene;
  scene.world = {{100.0, 100.0, 100.0}};
  scene.daughters = {
      {11u, {{8.0, 10.0, 12.0}}, {-35.0, 0.0, 0.0}},
      {22u, {{10.0, 8.0, 14.0}}, {25.0, 0.0, 0.0}},
  };

  // Exclude surface origins and ties: Geant4 tolerance handling at
  // coincident boundaries is a separate validation task.
  const std::vector<Case> cases{
      {{0,0,0}, {1,0,0}, Kind::daughter_entry, 22u},
      {{0,0,0}, {-1,0,0}, Kind::daughter_entry, 11u},
      {{0,0,0}, {0,1,0}, Kind::world_exit, 0u},
      {{0,0,0}, {0,0,1}, Kind::world_exit, 0u},
      {{-35,0,0}, {1,0,0}, Kind::daughter_exit, 11u},
      {{25,0,0}, {-1,0,0}, Kind::daughter_exit, 22u},
      {{60,0,0}, {-1,0,0}, Kind::daughter_entry, 22u},
      {{-50,30,0}, {1,0,0}, Kind::world_exit, 0u},
      {{0,0,30}, {0,0,1}, Kind::world_exit, 0u},
      {{0,30,0}, {0,-1,0}, Kind::world_exit, 0u},
      {{0,0,0}, {1,0.1,0}, Kind::daughter_entry, 22u},
      {{0,0,0}, {-1,0.1,0}, Kind::daughter_entry, 11u},
      // Oblique rays that miss both daughters must reach the world;
      // orthogonal rays from inside a daughter exit its side faces.
      {{0,0,0}, {1,1,0}, Kind::world_exit, 0u},
      {{0,0,0}, {1,0,1}, Kind::world_exit, 0u},
      {{25,0,0}, {0,1,0}, Kind::daughter_exit, 22u},
      {{-35,0,0}, {0,0,-1}, Kind::daughter_exit, 11u},
      // Offset rays probe daughter face extents independently of x distance.
      // The first and last enter daughter 22; the middle two miss both.
      {{0,5,0}, {1,0,0}, Kind::daughter_entry, 22u},
      {{0,9,0}, {1,0,0}, Kind::world_exit, 0u},
      {{0,0,13}, {-1,0,0}, Kind::world_exit, 0u},
      {{0,0,13}, {1,0,0}, Kind::daughter_entry, 22u},
      // Diagonal exits from daughter interiors: the nearest side face
      // wins even when another face is also approached.
      {{25,0,0}, {1,1,0}, Kind::daughter_exit, 22u},
      {{25,0,0}, {-1,-1,0}, Kind::daughter_exit, 22u},
      {{-35,0,0}, {1,0,1}, Kind::daughter_exit, 11u},
      {{-35,0,0}, {-1,0,-1}, Kind::daughter_exit, 11u},
      // 1 micron of nonzero clearance from the next face. These rays
      // exercise the independent Geant4 navigator oracle, not just the
      // portable CPU implementation (surface starts remain excluded).
      {{14.999,0,0}, {1,0,0}, Kind::daughter_entry, 22u},
      {{34.999,0,0}, {1,0,0}, Kind::daughter_exit, 22u},
      {{-42.999,0,0}, {-1,0,0}, Kind::daughter_exit, 11u},
      {{0,0,99.999}, {0,0,1}, Kind::world_exit, 0u},
      // Near-face clearances along independent axes also use the Geant4
      // oracle; avoid exact surface origins and tolerance ambiguity.
      {{25,7.999,0}, {0,1,0}, Kind::daughter_exit, 22u},
      {{-35,0,11.999}, {0,0,1}, Kind::daughter_exit, 11u},
      {{99.999,0,0}, {1,0,0}, Kind::world_exit, 0u},
      {{0,-99.999,0}, {0,-1,0}, Kind::world_exit, 0u},
  };

  // Geant4 normalizes track directions. Verify the portable reference
  // preserves the same physical boundary for positive ray rescaling.
  // Keep the independent Geant4 oracle in every comparison.
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const auto& item = cases[i];
    for (const double scale : {0.5, 1.0, 3.0}) {
      const V scaled_direction{
          item.direction.x * scale,
          item.direction.y * scale,
          item.direction.z * scale};
      const auto portable = g4wgpu::flat_box_next_boundary_mm(
          scene, item.position, scaled_direction);
      const auto p = item.position;
      const G4ThreeVector position(
          p.x * CLHEP::mm, p.y * CLHEP::mm, p.z * CLHEP::mm);
      const G4ThreeVector direction = G4ThreeVector(
          scaled_direction.x, scaled_direction.y, scaled_direction.z).unit();
      G4Track track(new G4DynamicParticle(
                        G4Gamma::GammaDefinition(), direction,
                        1.0 * CLHEP::MeV),
                    0.0, position);
      const double geant4 = g4wgpu::geant4_boundary_distance_mm(track);
      if (portable.kind != item.expected_kind ||
          portable.daughter_id != item.expected_id ||
          !std::isfinite(geant4) ||
          std::fabs(portable.distance_mm - geant4) > 1.0e-6) {
        std::cerr << "flat Box vs Geant4 next-boundary mismatch at "
                  << i << " direction scale=" << scale
                  << ": CPU=" << portable.distance_mm
                  << " Geant4=" << geant4 << '\n';
        delete run_manager;
        return EXIT_FAILURE;
      }
    }
  }

  delete run_manager;
  return EXIT_SUCCESS;
}
