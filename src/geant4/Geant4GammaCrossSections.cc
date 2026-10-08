#include "g4wgpu/geant4/Geant4GammaCrossSections.hh"

#include <stdexcept>

#include "G4EmCalculator.hh"
#include "G4Gamma.hh"
#include "G4Material.hh"
#include "G4SystemOfUnits.hh"

namespace g4wgpu {

GammaProcessCrossSections geant4_gamma_process_cross_sections(
    const G4Material& material,
    const double incident_gamma_energy_mev) {
  if (!(incident_gamma_energy_mev > 0.0)) {
    throw std::invalid_argument(
        "gamma kinetic energy must be positive");
  }

  G4EmCalculator calculator;
  const auto* gamma = G4Gamma::GammaDefinition();
  const double energy =
      incident_gamma_energy_mev * CLHEP::MeV;

  // Geant4's cross section per volume uses inverse internal length units.
  // Multiplying by mm converts to the portable mm^-1 numerical contract.
  GammaProcessCrossSections result;
  result.compton_per_mm =
      calculator.GetCrossSectionPerVolume(
          energy, gamma, "compt", &material) *
      CLHEP::mm;
  result.photoelectric_per_mm =
      calculator.GetCrossSectionPerVolume(
          energy, gamma, "phot", &material) *
      CLHEP::mm;
  result.pair_production_per_mm =
      calculator.GetCrossSectionPerVolume(
          energy, gamma, "conv", &material) *
      CLHEP::mm;

  result.validate();
  return result;
}

}  // namespace g4wgpu
