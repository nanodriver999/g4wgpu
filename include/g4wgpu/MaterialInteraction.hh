#pragma once

#include <cstdint>
#include <vector>

#include "g4wgpu/CounterRng.hh"

namespace g4wgpu {

struct MaterialElement {
  double atomic_number_z = 0.0;
  double number_density_per_mm3 = 0.0;
};

struct MaterialView {
  std::uint32_t material_id = 0;
  std::vector<MaterialElement> elements;

  void validate() const;
};

// Returns the Compton macroscopic cross section in mm^-1.
double compton_macroscopic_cross_section_per_mm(
    const MaterialView& material,
    double incident_gamma_energy_mev);

// Returns +infinity when the macroscopic cross section is zero.
double compton_mean_free_path_mm(
    const MaterialView& material,
    double incident_gamma_energy_mev);

// Samples an exponential interaction distance using the stable counter RNG.
// The supplied address is incremented exactly once.
double sample_compton_interaction_distance_mm(
    const MaterialView& material,
    double incident_gamma_energy_mev,
    RngAddress& rng);

}  // namespace g4wgpu
