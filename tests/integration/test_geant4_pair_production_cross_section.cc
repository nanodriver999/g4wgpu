#include "g4wgpu/PairProductionCrossSection.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "G4DataVector.hh"
#include "G4Gamma.hh"
#include "G4PairProductionRelModel.hh"
#include "G4SystemOfUnits.hh"

namespace {

// Near the 2*m_e*c^2 threshold the quadratic correction amplifies tiny
// differences between the portable electron-mass constant and CLHEP's value.
bool close_relative(
    const double a,
    const double b,
    const double tolerance = 1.0e-5) {
  const double scale =
      std::max(std::fabs(a), std::fabs(b));
  if (scale == 0.0) {
    return true;
  }
  return std::fabs(a - b) <=
         tolerance * scale;
}

}  // namespace

int main() {
  G4PairProductionRelModel reference;
  G4DataVector cuts;
  reference.Initialise(
      G4Gamma::GammaDefinition(), cuts);

  struct Case {
    double energy_mev;
    double z;
  };

  constexpr Case cases[] = {
      {1.05, 1.0},
      {1.10, 6.0},
      {1.20, 13.0},
      {1.50, 26.0},
      {2.00, 82.0},
      {10.0, 1.0},
      {10.0, 6.0},
      {10.0, 13.0},
      {10.0, 26.0},
      {10.0, 82.0},
      {100.0, 13.0},
      {1000.0, 82.0},
  };

  for (const auto& test : cases) {
    const double reference_internal =
        reference.ComputeCrossSectionPerAtom(
            G4Gamma::GammaDefinition(),
            test.energy_mev * CLHEP::MeV,
            test.z,
            0.0,
            0.0,
            0.0);
    const double reference_barn =
        reference_internal / CLHEP::barn;

    const double portable_barn =
        g4wgpu::pair_production_cross_section_per_atom_barn(
            test.energy_mev,
            test.z);

    if (!close_relative(
            reference_barn,
            portable_barn)) {
      const double scale =
          std::max(
              std::fabs(reference_barn),
              std::fabs(portable_barn));
      const double rel_error =
          scale == 0.0
              ? 0.0
              : std::fabs(
                    reference_barn -
                    portable_barn) /
                    scale;

      std::cerr
          << std::setprecision(17)
          << "pair-production mismatch E="
          << test.energy_mev
          << " MeV Z=" << test.z
          << " Geant4=" << reference_barn
          << " barn portable=" << portable_barn
          << " barn rel_error=" << rel_error
          << '\n';
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
