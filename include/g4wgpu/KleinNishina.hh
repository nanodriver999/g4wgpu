#pragma once

#include <cstdint>

#include "g4wgpu/CounterRng.hh"

namespace g4wgpu {

struct KleinNishinaSample {
  double scattered_gamma_energy_mev = 0.0;
  double recoil_electron_energy_mev = 0.0;
  double cos_theta = 1.0;
  double sin_theta = 0.0;
  double phi = 0.0;
  std::uint32_t iterations = 0;
  bool accepted = false;
};

// Samples free-electron Compton scattering using the Klein-Nishina
// distribution. The input and output energies are in MeV.
//
// This is deliberately independent from Geant4 object types so the same
// numerical contract can be implemented by CPU and WebGPU backends.
KleinNishinaSample sample_klein_nishina(
    double incident_gamma_energy_mev,
    RngAddress& rng,
    std::uint32_t max_iterations = 1000u);

}  // namespace g4wgpu
