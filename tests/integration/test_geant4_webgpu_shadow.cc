#include "g4wgpu/WebGpuBackend.hh"
#include "g4wgpu/geant4/G4WgpuTrackingManager.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "G4Box.hh"
#include "G4EmStandardPhysics.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

namespace {

class DetectorConstruction final : public G4VUserDetectorConstruction {
 public:
  G4VPhysicalVolume* Construct() override {
    auto* vacuum =
        G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
    auto* solid =
        new G4Box("World", 1.0 * CLHEP::m, 1.0 * CLHEP::m, 1.0 * CLHEP::m);
    auto* logical =
        new G4LogicalVolume(solid, vacuum, "World");

    return new G4PVPlacement(
        nullptr, G4ThreeVector(), logical, "World",
        nullptr, false, 0, false);
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

class PrimaryGenerator final : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGenerator() : gun_(1) {
    gun_.SetParticleDefinition(G4Gamma::GammaDefinition());
    gun_.SetParticleEnergy(1.0 * CLHEP::MeV);
    gun_.SetParticlePosition(G4ThreeVector());
    gun_.SetParticleMomentumDirection(G4ThreeVector(0.0, 0.0, 1.0));
  }

  void GeneratePrimaries(G4Event* event) override {
    gun_.GeneratePrimaryVertex(event);
  }

 private:
  G4ParticleGun gun_;
};

}  // namespace

int main() {
  auto* run_manager = new G4RunManager();
  run_manager->SetUserInitialization(new DetectorConstruction());
  run_manager->SetUserInitialization(new EmPhysicsList());
  run_manager->SetUserAction(new PrimaryGenerator());

  g4wgpu::WebGpuBackend shadow_backend;

  g4wgpu::TrackingPolicy policy;
  policy.batch_capacity = 8;
  policy.min_gamma_energy_mev = 0.0;

  g4wgpu::G4WgpuTrackingManager tracking_manager(
      policy, &shadow_backend);

  G4Gamma::GammaDefinition()->SetTrackingManager(&tracking_manager);

  run_manager->Initialize();
  run_manager->BeamOn(1);

  G4Gamma::GammaDefinition()->SetTrackingManager(nullptr);

  const auto& batch = tracking_manager.last_flushed_batch();
  const auto& shadow = tracking_manager.last_shadow_samples();
  const auto& competition =
      tracking_manager.last_shadow_process_competition();

  if (tracking_manager.pending_track_count() != 0u ||
      batch.size() != 1u ||
      shadow.size() != 1u ||
      competition.size() != 1u ||
      !shadow[0].accepted) {
    std::cerr << "Geant4 -> WebGPU shadow pipeline did not complete\n";
    delete run_manager;
    return EXIT_FAILURE;
  }

  if (!(competition[0].total_cross_section_per_mm > 0.0) ||
      !(competition[0].distance_mm > 0.0) ||
      !std::isfinite(competition[0].distance_mm)) {
    std::cerr << "Geant4 shadow process competition did not complete\n";
    delete run_manager;
    return EXIT_FAILURE;
  }

  if (!(shadow[0].scattered_gamma_energy_mev > 0.0 &&
        shadow[0].scattered_gamma_energy_mev <=
            batch.kinetic_energy[0])) {
    std::cerr << "WebGPU shadow result is outside physical energy range\n";
    delete run_manager;
    return EXIT_FAILURE;
  }

  delete run_manager;
  return EXIT_SUCCESS;
}
