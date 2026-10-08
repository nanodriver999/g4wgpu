#include "g4wgpu/GammaProcessCompetition.hh"
#include "g4wgpu/MaterialInteraction.hh"
#include "g4wgpu/geant4/Geant4GammaCrossSections.hh"
#include "g4wgpu/geant4/MaterialConversion.hh"

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
    const double tolerance = 3.0e-6) {
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

  // Complete one normal event so Geant4 builds the material-cuts couples and
  // EM physics tables used by G4EmCalculator::GetCrossSectionPerVolume().
  run_manager->BeamOn(1);

  const auto* water =
      G4NistManager::Instance()
          ->FindOrBuildMaterial("G4_WATER");
  if (water == nullptr) {
    std::cerr << "failed to construct G4_WATER\n";
    delete run_manager;
    return EXIT_FAILURE;
  }

  const auto material =
      g4wgpu::make_material_view_from_geant4(*water);

  constexpr double energies_mev[] = {
      0.1,
      1.0,
      2.0,
      10.0,
  };

  for (const double energy_mev : energies_mev) {
    const auto xs =
        g4wgpu::geant4_gamma_process_cross_sections(
            *water, energy_mev);

    if (!(xs.total_per_mm() > 0.0) ||
        !(xs.compton_per_mm > 0.0) ||
        !(xs.photoelectric_per_mm >= 0.0) ||
        !(xs.pair_production_per_mm >= 0.0)) {
      std::cerr
          << "invalid Geant4 gamma cross sections at "
          << energy_mev << " MeV\n";
      delete run_manager;
      return EXIT_FAILURE;
    }

    const double portable_compton =
        g4wgpu::compton_macroscopic_cross_section_per_mm(
            material, energy_mev);

    if (!close_relative(
            xs.compton_per_mm,
            portable_compton)) {
      std::cerr
          << "Compton cross-section mismatch at "
          << energy_mev
          << " MeV: Geant4="
          << xs.compton_per_mm
          << " 1/mm portable="
          << portable_compton
          << " 1/mm\n";
      delete run_manager;
      return EXIT_FAILURE;
    }

    if (energy_mev < 1.022 &&
        xs.pair_production_per_mm != 0.0) {
      std::cerr
          << "pair production should be zero below threshold\n";
      delete run_manager;
      return EXIT_FAILURE;
    }

    g4wgpu::RngAddress rng{
        123u,
        static_cast<std::uint32_t>(
            energy_mev * 1000.0),
        0u,
        0u};

    const auto sample =
        g4wgpu::sample_gamma_process_competition(
            xs, rng);

    if (sample.process ==
            g4wgpu::GammaProcess::none ||
        !(sample.distance_mm > 0.0) ||
        !std::isfinite(sample.distance_mm)) {
      std::cerr
          << "Geant4 cross sections did not feed competition correctly\n";
      delete run_manager;
      return EXIT_FAILURE;
    }
  }

  delete run_manager;
  return EXIT_SUCCESS;
}
