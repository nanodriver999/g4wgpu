#include "g4wgpu/ComptonCrossSection.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool nearly_equal(
    const double a,
    const double b,
    const double rel_tolerance = 1.0e-12) {
  const double scale =
      std::max(std::fabs(a), std::fabs(b));
  if (scale == 0.0) {
    return true;
  }
  return std::fabs(a - b) <= rel_tolerance * scale;
}

}  // namespace

int main() {
  struct Case {
    double energy_mev;
    double z;
  };

  constexpr Case cases[] = {
      {0.005, 1.0},
      {0.020, 1.0},
      {0.100, 1.0},
      {1.000, 1.0},
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
    const double value =
        g4wgpu::klein_nishina_cross_section_per_atom_barn(
            test.energy_mev, test.z);

    if (!(value > 0.0) ||
        !std::isfinite(value)) {
      std::cerr
          << "invalid cross section for E="
          << test.energy_mev
          << " MeV Z=" << test.z << '\n';
      return EXIT_FAILURE;
    }
  }

  // The cross section should decrease with energy in the asymptotic region.
  const double carbon_1 =
      g4wgpu::klein_nishina_cross_section_per_atom_barn(
          1.0, 6.0);
  const double carbon_10 =
      g4wgpu::klein_nishina_cross_section_per_atom_barn(
          10.0, 6.0);

  if (!(carbon_10 < carbon_1)) {
    std::cerr << "high-energy carbon cross section should decrease\n";
    return EXIT_FAILURE;
  }

  // Atomic scaling should remain ordered for representative elements.
  const double hydrogen =
      g4wgpu::klein_nishina_cross_section_per_atom_barn(
          1.0, 1.0);
  const double lead =
      g4wgpu::klein_nishina_cross_section_per_atom_barn(
          1.0, 82.0);

  if (!(lead > hydrogen)) {
    std::cerr << "lead cross section should exceed hydrogen\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
