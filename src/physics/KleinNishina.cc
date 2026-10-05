#include "g4wgpu/KleinNishina.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace g4wgpu {
namespace {

constexpr double kElectronMassMeV = 0.51099895000;
constexpr double kTwoPi = 6.283185307179586476925286766559;

double next_uniform(RngAddress& rng) noexcept {
  const float value = CounterRng::uniform_f32(rng);
  CounterRng::increment(rng);
  return static_cast<double>(value);
}

}  // namespace

KleinNishinaSample sample_klein_nishina(
    const double incident_gamma_energy_mev,
    RngAddress& rng,
    const std::uint32_t max_iterations) {
  if (!(incident_gamma_energy_mev > 0.0)) {
    throw std::invalid_argument(
        "incident gamma energy must be positive");
  }
  if (max_iterations == 0u) {
    throw std::invalid_argument(
        "max_iterations must be greater than zero");
  }

  const double energy_over_mass =
      incident_gamma_energy_mev / kElectronMassMeV;

  const double epsilon_min =
      1.0 / (1.0 + 2.0 * energy_over_mass);
  const double epsilon_min_sq = epsilon_min * epsilon_min;

  const double alpha1 = -std::log(epsilon_min);
  const double alpha2 =
      alpha1 + 0.5 * (1.0 - epsilon_min_sq);

  for (std::uint32_t iteration = 1u;
       iteration <= max_iterations;
       ++iteration) {
    const double r0 = next_uniform(rng);
    const double r1 = next_uniform(rng);
    const double r2 = next_uniform(rng);

    double epsilon = 0.0;
    double epsilon_sq = 0.0;

    if (alpha1 > alpha2 * r0) {
      epsilon = std::exp(-alpha1 * r1);
      epsilon_sq = epsilon * epsilon;
    } else {
      epsilon_sq =
          epsilon_min_sq +
          (1.0 - epsilon_min_sq) * r1;
      epsilon = std::sqrt(epsilon_sq);
    }

    const double one_minus_cos =
        (1.0 - epsilon) /
        (epsilon * energy_over_mass);

    double sin_theta_sq =
        one_minus_cos * (2.0 - one_minus_cos);

    const double rejection =
        1.0 -
        epsilon * sin_theta_sq /
            (1.0 + epsilon_sq);

    if (rejection < r2) {
      continue;
    }

    sin_theta_sq = std::max(0.0, sin_theta_sq);

    KleinNishinaSample result;
    result.scattered_gamma_energy_mev =
        epsilon * incident_gamma_energy_mev;
    result.recoil_electron_energy_mev =
        incident_gamma_energy_mev -
        result.scattered_gamma_energy_mev;
    result.cos_theta = 1.0 - one_minus_cos;
    result.sin_theta = std::sqrt(sin_theta_sq);
    result.phi = kTwoPi * next_uniform(rng);
    result.iterations = iteration;
    result.accepted = true;
    return result;
  }

  KleinNishinaSample failed;
  failed.iterations = max_iterations;
  failed.accepted = false;
  return failed;
}

}  // namespace g4wgpu
