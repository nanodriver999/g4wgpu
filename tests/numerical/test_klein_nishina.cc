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

  return EXIT_SUCCESS;
}
