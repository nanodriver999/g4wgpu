#include "g4wgpu/ComptonCrossSection.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace g4wgpu {

double klein_nishina_cross_section_per_atom_barn(
    const double incident_gamma_energy_mev,
    const double atomic_number_z) {
  if (!(incident_gamma_energy_mev > 0.0)) {
    throw std::invalid_argument(
        "incident gamma energy must be positive");
  }
  if (!(atomic_number_z > 0.0)) {
    throw std::invalid_argument(
        "atomic number must be positive");
  }

  constexpr double electron_mass_mev = 0.51099895;

  constexpr double a = 20.0;
  constexpr double b = 230.0;
  constexpr double c = 440.0;

  constexpr double d1 = 2.7965e-1;
  constexpr double d2 = -1.8300e-1;
  constexpr double d3 = 6.7527;
  constexpr double d4 = -1.9798e+1;

  constexpr double e1 = 1.9756e-5;
  constexpr double e2 = -1.0205e-2;
  constexpr double e3 = -7.3913e-2;
  constexpr double e4 = 2.7079e-2;

  constexpr double f1 = -3.9178e-7;
  constexpr double f2 = 6.8241e-5;
  constexpr double f3 = 6.0480e-5;
  constexpr double f4 = 3.0274e-4;

  const double z = atomic_number_z;
  const double z2 = z * z;

  const double p1z = z * (d1 + e1 * z + f1 * z2);
  const double p2z = z * (d2 + e2 * z + f2 * z2);
  const double p3z = z * (d3 + e3 * z + f3 * z2);
  const double p4z = z * (d4 + e4 * z + f4 * z2);

  const double t0_mev = z < 1.5 ? 0.040 : 0.015;

  auto sigma_at = [&](const double energy_mev) {
    const double x =
        energy_mev / electron_mass_mev;
    return p1z * std::log(1.0 + 2.0 * x) / x +
           (p2z + p3z * x + p4z * x * x) /
               (1.0 + a * x + b * x * x +
                c * x * x * x);
  };

  const double clamped_energy =
      std::max(incident_gamma_energy_mev, t0_mev);
  double cross_section_barn =
      sigma_at(clamped_energy);

  if (incident_gamma_energy_mev < t0_mev) {
    constexpr double delta_t_mev = 0.001;

    const double sigma =
        sigma_at(t0_mev + delta_t_mev);

    const double c1 =
        -t0_mev * (sigma - cross_section_barn) /
        (cross_section_barn * delta_t_mev);

    const double c2 =
        z > 1.5
            ? 0.375 - 0.0556 * std::log(z)
            : 0.150;

    const double y =
        std::log(
            incident_gamma_energy_mev / t0_mev);

    cross_section_barn *=
        std::exp(-y * (c1 + c2 * y));
  }

  return cross_section_barn;
}

}  // namespace g4wgpu
