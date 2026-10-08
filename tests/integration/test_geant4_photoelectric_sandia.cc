#include "g4wgpu/PhotoelectricCrossSection.hh"
#include "g4wgpu/geant4/PhotoelectricSandiaConversion.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "G4Box.hh"
#include "G4EmStandardPhysics.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PEEffectFluoModel.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

namespace {

class DetectorConstruction final
    : public G4VUserDetectorConstruction {
 public:
  G4VPhysicalVolume* Construct() override {
    auto* water =
        G4NistManager::Instance()
            ->FindOrBuildMaterial("G4_WATER");
    auto* solid =
        new G4Box(
            "World",
            1.0 * CLHEP::m,
            1.0 * CLHEP::m,
            1.0 * CLHEP::m);
    auto* logical =
        new G4LogicalVolume(
            solid, water, "World");
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

class EmPhysicsList final : public G4VModularPhysicsList {
 public:
  EmPhysicsList() {
    RegisterPhysics(new G4EmStandardPhysics());
  }

  void SetCuts() override {
    SetCutsWithDefault();
  }
};

class PrimaryGenerator final
    : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGenerator() : gun_(1) {
    gun_.SetParticleDefinition(
        G4Gamma::GammaDefinition());
    gun_.SetParticleEnergy(2.0 * CLHEP::MeV);
    gun_.SetParticlePosition(G4ThreeVector());
    gun_.SetParticleMomentumDirection(
        G4ThreeVector(0.0, 0.0, 1.0));
  }

  void GeneratePrimaries(G4Event* event) override {
    gun_.GeneratePrimaryVertex(event);
  }

 private:
  G4ParticleGun gun_;
};

bool close_relative(
    const double a,
    const double b,
    const double tolerance = 1.0e-12) {
  const double scale =
      std::max(std::fabs(a), std::fabs(b));
  if (scale == 0.0) {
    return true;
  }
  return std::fabs(a - b) <= tolerance * scale;
}

}  // namespace

int main() {
  auto* run_manager = new G4RunManager();
  run_manager->SetUserInitialization(
      new DetectorConstruction());
  run_manager->SetUserInitialization(
      new EmPhysicsList());
  run_manager->SetUserAction(
      new PrimaryGenerator());
  run_manager->Initialize();
  run_manager->BeamOn(1);

  const char* material_names[] = {
      "G4_WATER",
      "G4_Al",
      "G4_Fe",
  };
  constexpr double energies_mev[] = {
      0.2,
      1.0,
      10.0,
  };

  auto* nist = G4NistManager::Instance();
  for (const char* name : material_names) {
    if (nist->FindOrBuildMaterial(name) == nullptr) {
      std::cerr << "failed to prebuild material " << name << '\n';
      delete run_manager;
      return EXIT_FAILURE;
    }
  }

  G4PEEffectFluoModel reference_model;
  G4DataVector cuts;
  reference_model.Initialise(
      G4Gamma::GammaDefinition(), cuts);

  for (const char* name : material_names) {
    const auto* material =
        nist->FindOrBuildMaterial(name);
    if (material == nullptr) {
      std::cerr << "failed to build material " << name << '\n';
      delete run_manager;
      return EXIT_FAILURE;
    }

    for (const double energy_mev : energies_mev) {
      const auto segment =
          g4wgpu::make_photoelectric_sandia_segment_from_geant4(
              *material, energy_mev);
      const double portable =
          g4wgpu::photoelectric_macroscopic_cross_section_per_mm(
              segment, energy_mev);
      const double geant4 =
          reference_model.CrossSectionPerVolume(
              material,
              G4Gamma::GammaDefinition(),
              energy_mev * CLHEP::MeV,
              0.0,
              energy_mev * CLHEP::MeV) *
          CLHEP::mm;

      if (!close_relative(portable, geant4)) {
        std::cerr
            << "photoelectric Sandia mismatch material="
            << name
            << " E=" << energy_mev
            << " MeV portable=" << portable
            << " 1/mm Geant4=" << geant4
            << " 1/mm\n";
        delete run_manager;
        return EXIT_FAILURE;
      }
    }
  }

  delete run_manager;
  return EXIT_SUCCESS;
}
