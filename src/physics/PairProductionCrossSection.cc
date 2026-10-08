#include "g4wgpu/PairProductionCrossSection.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace g4wgpu {
namespace {

constexpr double electron_mass_mev = 0.51099895;
constexpr double parameterization_min_mev = 1.5;
constexpr double microbarn_to_barn = 1.0e-6;
constexpr double barn_to_mm2 = 1.0e-22;

double polynomial5(
    const double x,
    const double c0,
    const double c1,
    const double c2,
    const double c3,
    const double c4,
    const double c5) {
  return c0 +
         x * (c1 +
         x * (c2 +
         x * (c3 +
         x * (c4 +
         x * c5))));
}

}  // namespace

double pair_production_cross_section_per_atom_barn(
    const double incident_gamma_energy_mev,
    const double atomic_number_z) {
  if (!(incident_gamma_energy_mev > 0.0) ||
      !std::isfinite(incident_gamma_energy_mev)) {
    throw std::invalid_argument(
        "gamma kinetic energy must be finite and positive");
  }
  if (!(atomic_number_z > 0.0) ||
      !std::isfinite(atomic_number_z)) {
    throw std::invalid_argument(
        "atomic number Z must be finite and positive");
  }

  const double threshold_mev =
      2.0 * electron_mass_mev;
  if (atomic_number_z < 0.9 ||
      incident_gamma_energy_mev <= threshold_mev) {
    return 0.0;
  }

  const double evaluation_energy_mev =
      std::max(
          incident_gamma_energy_mev,
          parameterization_min_mev);
  const double x =
      std::log(
          evaluation_energy_mev /
          electron_mass_mev);

  // Geant4 coefficients are expressed in microbarn.
  const double f1_microbarn = polynomial5(
      x,
      8.7842e2,
      -1.9625e3,
      1.2949e3,
      -2.0028e2,
      1.2575e1,
      -2.8333e-1);

  const double f2_microbarn = polynomial5(
      x,
      -1.0342e1,
      1.7692e1,
      -8.2381,
      1.3063,
      -9.0815e-2,
      2.3586e-3);

  const double f3_microbarn = polynomial5(
      x,
      -4.5263e2,
      1.1161e3,
      -8.6749e2,
      2.1773e2,
      -2.0467e1,
      6.5372e-1);

  double cross_section_microbarn =
      (atomic_number_z + 1.0) *
      (f1_microbarn * atomic_number_z +
       f2_microbarn * atomic_number_z * atomic_number_z +
       f3_microbarn);

  if (incident_gamma_energy_mev <
      parameterization_min_mev) {
    const double ratio =
        (incident_gamma_energy_mev -
         threshold_mev) /
        (parameterization_min_mev -
         threshold_mev);
    cross_section_microbarn *=
        ratio * ratio;
  }

  return std::max(
      cross_section_microbarn *
          microbarn_to_barn,
      0.0);
}

double pair_production_macroscopic_cross_section_per_mm(
    const MaterialView& material,
    const double incident_gamma_energy_mev) {
  material.validate();

  double total_per_mm = 0.0;
  for (const auto& element : material.elements) {
    total_per_mm +=
        element.number_density_per_mm3 *
        pair_production_cross_section_per_atom_barn(
            incident_gamma_energy_mev,
            element.atomic_number_z) *
        barn_to_mm2;
  }
  return total_per_mm;
}

}  // namespace g4wgpu
