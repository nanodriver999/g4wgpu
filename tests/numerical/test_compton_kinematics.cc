#include "g4wgpu/ComptonKinematics.hh"
#include "g4wgpu/KleinNishina.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

double dot(
    const g4wgpu::Vector3& a,
    const g4wgpu::Vector3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

double norm(const g4wgpu::Vector3& v) {
  return std::sqrt(dot(v, v));
}

g4wgpu::Vector3 normalized(g4wgpu::Vector3 v) {
  const double n = norm(v);
  v.x /= n;
  v.y /= n;
  v.z /= n;
  return v;
}

}  // namespace

int main() {
  constexpr double electron_mass_mev = 0.51099895;
  constexpr double incident_energy_mev = 1.0;

  const g4wgpu::Vector3 incident =
      normalized({1.0, 2.0, 3.0});

  g4wgpu::RngAddress rng{
      0x12345678u,
      0x9abcdef0u,
      0u,
      0u};

  for (int i = 0; i < 10000; ++i) {
    const auto sample =
        g4wgpu::sample_klein_nishina(
            incident_energy_mev,
            rng);

    const auto final_state =
        g4wgpu::make_compton_final_state(
            incident_energy_mev,
            incident,
            sample);

    if (std::fabs(
            norm(final_state.scattered_gamma_direction) -
            1.0) > 1.0e-12 ||
        std::fabs(
            norm(final_state.recoil_electron_direction) -
            1.0) > 1.0e-12) {
      std::cerr << "Compton final-state direction is not unit length\n";
      return EXIT_FAILURE;
    }

    const double sampled_mu =
        dot(
            incident,
            final_state.scattered_gamma_direction);
    if (std::fabs(sampled_mu - sample.cos_theta) > 1.0e-12) {
      std::cerr << "lab-frame gamma direction does not preserve theta\n";
      return EXIT_FAILURE;
    }

    if (std::fabs(
            final_state.scattered_gamma_energy_mev +
            final_state.recoil_electron_kinetic_energy_mev -
            incident_energy_mev) > 1.0e-12) {
      std::cerr << "Compton final state does not conserve energy\n";
      return EXIT_FAILURE;
    }

    const double electron_momentum =
        std::sqrt(
            final_state.recoil_electron_kinetic_energy_mev *
            (final_state.recoil_electron_kinetic_energy_mev +
             2.0 * electron_mass_mev));

    const g4wgpu::Vector3 residual{
        incident_energy_mev * incident.x -
            final_state.scattered_gamma_energy_mev *
                final_state.scattered_gamma_direction.x -
            electron_momentum *
                final_state.recoil_electron_direction.x,
        incident_energy_mev * incident.y -
            final_state.scattered_gamma_energy_mev *
                final_state.scattered_gamma_direction.y -
            electron_momentum *
                final_state.recoil_electron_direction.y,
        incident_energy_mev * incident.z -
            final_state.scattered_gamma_energy_mev *
                final_state.scattered_gamma_direction.z -
            electron_momentum *
                final_state.recoil_electron_direction.z};

    if (norm(residual) > 2.0e-10) {
      std::cerr << "Compton final state does not conserve momentum\n";
      return EXIT_FAILURE;
    }
  }

  bool rejected_invalid_sample = false;
  try {
    g4wgpu::KleinNishinaSample invalid;
    (void)g4wgpu::make_compton_final_state(
        incident_energy_mev,
        incident,
        invalid);
  } catch (const std::invalid_argument&) {
    rejected_invalid_sample = true;
  }

  if (!rejected_invalid_sample) {
    std::cerr << "unaccepted Klein-Nishina sample was not rejected\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
