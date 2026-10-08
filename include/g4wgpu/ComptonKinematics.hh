#pragma once

#include "g4wgpu/KleinNishina.hh"

namespace g4wgpu {

struct Vector3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct ComptonFinalState {
  double scattered_gamma_energy_mev = 0.0;
  double recoil_electron_kinetic_energy_mev = 0.0;
  Vector3 scattered_gamma_direction;
  Vector3 recoil_electron_direction;
};

// Converts a sampled Klein-Nishina scattering angle/energy into lab-frame
// outgoing gamma and recoil-electron directions. The incident direction may
// be non-unit; it is normalized internally.
ComptonFinalState make_compton_final_state(
    double incident_gamma_energy_mev,
    Vector3 incident_direction,
    const KleinNishinaSample& sample);

}  // namespace g4wgpu
