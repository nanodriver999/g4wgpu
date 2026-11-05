#include "g4wgpu/geant4/GeometryBoundaryDistance.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

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

class DetectorConstruction final
    : public G4VUserDetectorConstruction {
 public:
  G4VPhysicalVolume* Construct() override {
    auto* vacuum =
        G4NistManager::Instance()
            ->FindOrBuildMaterial("G4_Galactic");

    auto* solid =
        new G4Box(
            "World",
            1.0 * CLHEP::m,
            1.0 * CLHEP::m,
            1.0 * CLHEP::m);
    auto* logical =
        new G4LogicalVolume(
            solid,
            vacuum,
            "World");

    return new G4PVPlacement(
        nullptr,
        G4ThreeVector(),
        logical,
        "World",
        nullptr,
        false,
        0,
        false);
  }
};

}  // namespace

int main() {
  auto* run_manager = new G4RunManager();
  run_manager->SetUserInitialization(
      new DetectorConstruction());
  run_manager->InitializeGeometry();

  auto* particle =
      new G4DynamicParticle(
          G4Gamma::GammaDefinition(),
          G4ThreeVector(0.0, 0.0, 1.0),
          1.0 * CLHEP::MeV);

  G4Track track(
      particle,
      0.0,
      G4ThreeVector());

  const double distance =
      g4wgpu::geant4_boundary_distance_mm(track);

  if (!std::isfinite(distance) ||
      std::fabs(distance - 1000.0) > 1.0e-6) {
    std::cerr
        << "unexpected boundary distance: "
        << distance << " mm\n";
    delete run_manager;
    return EXIT_FAILURE;
  }

  delete run_manager;
  return EXIT_SUCCESS;
}
