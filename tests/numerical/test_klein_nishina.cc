#include "g4wgpu/KleinNishina.hh"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool close_enough(const double a, const double b,
                  const double tolerance) {
  return std::fabs(a - b) <= tolerance;
}

}  // namespace

int main() {
  constexpr std::array<double, 5> energies_mev{
      0.02, 0.1, 0.51099895, 1.0, 10.0};

  for (std::size_t energy_index = 0;
       energy_index < energies_mev.size();
       ++energy_index) {
    const double incident = energies_mev[energy_index];

    for (std::uint32_t sample_index = 0;
         sample_index < 2000u;
         ++sample_index) {
      g4wgpu::RngAddress rng{
          static_cast<std::uint32_t>(energy_index + 1u),
          0x4b4eu,
          sample_index,
          0u};

      const auto sample =
          g4wgpu::sample_klein_nishina(incident, rng);

      if (!sample.accepted) {
        std::cerr << "sampling exceeded iteration limit\n";
        return EXIT_FAILURE;
      }

      if (!(sample.scattered_gamma_energy_mev > 0.0 &&
            sample.scattered_gamma_energy_mev <= incident)) {
        std::cerr << "invalid scattered gamma energy\n";
        return EXIT_FAILURE;
      }

      if (!(sample.recoil_electron_energy_mev >= 0.0 &&
            sample.recoil_electron_energy_mev < incident)) {
        std::cerr << "invalid recoil electron energy\n";
        return EXIT_FAILURE;
      }

      if (!close_enough(
              sample.scattered_gamma_energy_mev +
                  sample.recoil_electron_energy_mev,
              incident,
              1.0e-12)) {
        std::cerr << "energy conservation failed\n";
        return EXIT_FAILURE;
      }

      if (!(sample.cos_theta >= -1.0 - 1.0e-12 &&
            sample.cos_theta <= 1.0 + 1.0e-12)) {
        std::cerr << "cos(theta) outside physical range\n";
        return EXIT_FAILURE;
      }

      if (!(sample.sin_theta >= 0.0 &&
            sample.sin_theta <= 1.0 + 1.0e-12)) {
        std::cerr << "sin(theta) outside physical range\n";
        return EXIT_FAILURE;
      }

      if (!close_enough(
              sample.cos_theta * sample.cos_theta +
                  sample.sin_theta * sample.sin_theta,
              1.0,
              1.0e-10)) {
        std::cerr << "angle normalization failed\n";
        return EXIT_FAILURE;
      }

      if (!(sample.phi >= 0.0 &&
            sample.phi < 6.2831853071795864769)) {
        std::cerr << "phi outside [0, 2pi)\n";
        return EXIT_FAILURE;
      }
    }
  }

  {
    constexpr double incident = 1.0;
    constexpr double electron_mass_mev = 0.51099895;
    constexpr std::size_t integration_bins = 20000;

    double weight_sum = 0.0;
    double weighted_epsilon = 0.0;
    double weighted_mu = 0.0;

    const double k = incident / electron_mass_mev;
    for (std::size_t i = 0; i < integration_bins; ++i) {
      const double mu =
          -1.0 +
          (static_cast<double>(i) + 0.5) *
              (2.0 / static_cast<double>(integration_bins));
      const double epsilon =
          1.0 / (1.0 + k * (1.0 - mu));
      const double sin2 = 1.0 - mu * mu;
      const double weight =
          epsilon * epsilon *
          (epsilon + 1.0 / epsilon - sin2);

      weight_sum += weight;
      weighted_epsilon += weight * epsilon;
      weighted_mu += weight * mu;
    }

    const double expected_mean_epsilon =
        weighted_epsilon / weight_sum;
    const double expected_mean_mu =
        weighted_mu / weight_sum;

    constexpr std::uint32_t sample_count = 30000u;
    double sample_epsilon_sum = 0.0;
    double sample_mu_sum = 0.0;

    for (std::uint32_t i = 0; i < sample_count; ++i) {
      g4wgpu::RngAddress rng{
          i + 1u, 0x6b6eu, 0u, 0u};
      const auto sample =
          g4wgpu::sample_klein_nishina(incident, rng);

      if (!sample.accepted) {
        std::cerr << "distribution sample was not accepted\n";
        return EXIT_FAILURE;
      }

      sample_epsilon_sum +=
          sample.scattered_gamma_energy_mev / incident;
      sample_mu_sum += sample.cos_theta;
    }

    const double sampled_mean_epsilon =
        sample_epsilon_sum /
        static_cast<double>(sample_count);
    const double sampled_mean_mu =
        sample_mu_sum /
        static_cast<double>(sample_count);

    if (!close_enough(
            sampled_mean_epsilon,
            expected_mean_epsilon,
            5.0e-3)) {
      std::cerr
          << "mean scattered-energy fraction differs from "
          << "Klein-Nishina integral: expected "
          << expected_mean_epsilon << ", got "
          << sampled_mean_epsilon << '\n';
      return EXIT_FAILURE;
    }

    if (!close_enough(
            sampled_mean_mu,
            expected_mean_mu,
            5.0e-3)) {
      std::cerr
          << "mean cos(theta) differs from "
          << "Klein-Nishina integral: expected "
          << expected_mean_mu << ", got "
          << sampled_mean_mu << '\n';
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
