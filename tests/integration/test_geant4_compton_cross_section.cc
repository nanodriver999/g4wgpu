#include "g4wgpu/ComptonCrossSection.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "G4Gamma.hh"
#include "G4KleinNishinaCompton.hh"
#include "G4SystemOfUnits.hh"

namespace {

bool close_relative(
    const double a,
    const double b,
    const double tolerance = 1.0e-9) {
  const double scale =
      std::max(std::fabs(a), std::fabs(b));
  if (scale == 0.0) {
    return true;
  }
  return std::fabs(a - b) <= tolerance * scale;
}

}  // namespace

int main() {
  G4KleinNishinaCompton reference;

  struct Case {
    double energy_mev;
    double z;
  };

  constexpr Case cases[] = {
      {0.005, 1.0},
      {0.020, 1.0},
      {0.100, 1.0},
      {1.000, 1.0},
      {10.00, 1.0},
      {0.005, 6.0},
      {0.020, 6.0},
      {0.100, 6.0},
      {1.000, 6.0},
      {10.00, 6.0},
      {0.010, 13.0},
      {0.100, 13.0},
      {1.000, 13.0},
      {10.00, 13.0},
      {0.010, 26.0},
      {0.100, 26.0},
      {1.000, 26.0},
      {10.00, 26.0},
      {0.010, 82.0},
      {0.100, 82.0},
      {1.000, 82.0},
      {10.00, 82.0},
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
        g4wgpu::klein_nishina_cross_section_per_atom_barn(
            test.energy_mev,
            test.z);

    if (!close_relative(
            reference_barn,
            portable_barn)) {
      const double rel_error =
          std::fabs(reference_barn - portable_barn) /
          std::max(std::fabs(reference_barn), std::fabs(portable_barn));
      std::cerr
          << std::setprecision(17)
          << "cross-section mismatch E="
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
