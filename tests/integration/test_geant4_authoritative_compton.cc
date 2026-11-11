#include "g4wgpu/PhysicsBackend.hh"
#include "g4wgpu/geant4/G4WgpuTrackingManager.hh"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "G4Box.hh"
#include "G4EmStandardPhysics.hh"
#include "G4Electron.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4UserTrackingAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

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
    gun_.SetParticleDefinition(
        G4Gamma::GammaDefinition());
    gun_.SetParticleEnergy(
        1.0 * CLHEP::MeV);
    gun_.SetParticlePosition(
        G4ThreeVector());
    gun_.SetParticleMomentumDirection(
        G4ThreeVector(0.0, 0.0, 1.0));
  }

  void GeneratePrimaries(
      G4Event* event) override {
    gun_.GeneratePrimaryVertex(event);
  }

 private:
  G4ParticleGun gun_;
};

class TrackingObserver final : public G4UserTrackingAction {
 public:
  TrackingObserver(std::size_t& gamma_count, std::size_t& electron_count)
      : gamma_count_(gamma_count), electron_count_(electron_count) {}

  void PreUserTrackingAction(const G4Track* track) override {
    const double energy_mev = track->GetKineticEnergy() / CLHEP::MeV;
    const double z_mm = track->GetPosition().z() / CLHEP::mm;
    if (std::fabs(energy_mev - 0.5) > 1.0e-9 ||
        std::fabs(z_mm - 10.0) > 1.0e-9) {
      return;
    }

    if (track->GetParticleDefinition() == G4Gamma::GammaDefinition()) {
      ++gamma_count_;
    } else if (
        track->GetParticleDefinition() == G4Electron::ElectronDefinition() &&
        track->GetParentID() > 0) {
      ++electron_count_;
    }
  }

 private:
  std::size_t& gamma_count_;
  std::size_t& electron_count_;
};

class DeterministicBackend final
    : public g4wgpu::PhysicsBackend {
 public:
  enum class Scenario { compton, beyond_boundary, non_compton };
  Scenario scenario = Scenario::compton;

  std::vector<g4wgpu::GammaInteractionSample>
  sample_gamma_process_competition_batch(
      const std::vector<g4wgpu::GammaProcessCrossSections>& cross_sections,
      std::vector<g4wgpu::RngAddress>& rng) override {
    std::vector<g4wgpu::GammaInteractionSample> result;
    result.reserve(cross_sections.size());

    for (std::size_t i = 0; i < cross_sections.size(); ++i) {
      g4wgpu::CounterRng::increment(rng[i]);
      g4wgpu::CounterRng::increment(rng[i]);
      result.push_back(
          g4wgpu::GammaInteractionSample{
              scenario == Scenario::non_compton
                  ? g4wgpu::GammaProcess::photoelectric
                  : g4wgpu::GammaProcess::compton,
              scenario == Scenario::beyond_boundary ? 2000.0 : 10.0,
              1.0});
    }

    return result;
  }

  std::vector<g4wgpu::KleinNishinaSample>
  sample_klein_nishina_batch(
      const std::vector<double>& incident_gamma_energy_mev,
      std::vector<g4wgpu::RngAddress>& rng,
      std::uint32_t) override {
    std::vector<g4wgpu::KleinNishinaSample> result;
    result.reserve(incident_gamma_energy_mev.size());

    for (std::size_t i = 0;
         i < incident_gamma_energy_mev.size();
         ++i) {
      if (std::fabs(
              incident_gamma_energy_mev[i] -
              1.0) > 1.0e-12) {
        std::cerr
            << "unexpected authoritative test input energy\n";
        std::exit(EXIT_FAILURE);
      }

      g4wgpu::CounterRng::increment(rng[i]);

      const double cos_theta = 0.48900105;
      result.push_back(
          g4wgpu::KleinNishinaSample{
              0.5,
              0.5,
              cos_theta,
              std::sqrt(
                  1.0 - cos_theta * cos_theta),
              0.0,
              1u,
              true});
    }

    return result;
  }
};

}  // namespace

int main() {
  auto* run_manager =
      new G4RunManager();

  run_manager->SetUserInitialization(
      new DetectorConstruction());
  run_manager->SetUserInitialization(
      new EmPhysicsList());
  run_manager->SetUserAction(
      new PrimaryGenerator());

  DeterministicBackend backend;
  std::size_t resumed_gamma_count = 0;
  std::size_t recoil_electron_count = 0;
  run_manager->SetUserAction(
      new TrackingObserver(resumed_gamma_count, recoil_electron_count));

  g4wgpu::TrackingPolicy policy;
  policy.batch_capacity = 8;
  policy.min_gamma_energy_mev = 0.9;
  policy.enable_authoritative_compton = true;

  g4wgpu::G4WgpuTrackingManager tracking_manager(
      policy,
      &backend);

  G4Gamma::GammaDefinition()->SetTrackingManager(
      &tracking_manager);

  run_manager->Initialize();
  run_manager->BeamOn(1);

  // The promoted gamma must re-enter Geant4 CPU tracking at the 10 mm
  // interaction point with 0.5 MeV, alongside the 0.5 MeV recoil electron.
  if (resumed_gamma_count != 1u || recoil_electron_count != 1u) {
    std::cerr
        << "promoted gamma/electron were not both tracked at 10 mm: "
        << resumed_gamma_count << " / " << recoil_electron_count << '\n';
    G4Gamma::GammaDefinition()->SetTrackingManager(nullptr);
    delete run_manager;
    return EXIT_FAILURE;
  }

  const auto expect_cpu_fallback = [&](DeterministicBackend::Scenario scenario,
                                       const char* label) {
    backend.scenario = scenario;
    run_manager->BeamOn(1);
    if (tracking_manager.authoritative_compton_promotions() != 1u ||
        resumed_gamma_count != 1u ||
        recoil_electron_count != 1u) {
      std::cerr << "unexpected promotion for " << label << '\n';
      return false;
    }
    return true;
  };

  // A Compton interaction beyond the geometry boundary must not be promoted.
  if (!expect_cpu_fallback(
          DeterministicBackend::Scenario::beyond_boundary,
          "geometry-limited step") ||
      // Unsupported final states continue with the Geant4 CPU tracker.
      !expect_cpu_fallback(
          DeterministicBackend::Scenario::non_compton,
          "non-Compton process")) {
    G4Gamma::GammaDefinition()->SetTrackingManager(nullptr);
    delete run_manager;
    return EXIT_FAILURE;
  }

  G4Gamma::GammaDefinition()->SetTrackingManager(
      nullptr);

  if (tracking_manager.authoritative_compton_promotions() != 1u) {
    std::cerr
        << "expected exactly one authoritative Compton promotion, got "
        << tracking_manager.authoritative_compton_promotions()
        << '\n';
    delete run_manager;
    return EXIT_FAILURE;
  }

  if (tracking_manager.pending_track_count() != 0u) {
    std::cerr
        << "authoritative promotion left buffered tracks\n";
    delete run_manager;
    return EXIT_FAILURE;
  }

  delete run_manager;
  return EXIT_SUCCESS;
}
