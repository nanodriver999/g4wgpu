#include "g4wgpu/MaterialInteraction.hh"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "g4wgpu/ComptonCrossSection.hh"

namespace g4wgpu {
namespace {

constexpr double barn_to_mm2 = 1.0e-22;

}

void MaterialView::validate() const {
  if (elements.empty()) {
    throw std::invalid_argument("material must contain at least one element");
  }
  for (const auto& element : elements) {
    if (!(element.atomic_number_z > 0.0)) {
      throw std::invalid_argument("material element Z must be positive");
    }
    if (!(element.number_density_per_mm3 >= 0.0) ||
        !std::isfinite(element.number_density_per_mm3)) {
      throw std::invalid_argument(
          "material element number density must be finite and non-negative");
    }
  }
}

double compton_macroscopic_cross_section_per_mm(
    const MaterialView& material,
    const double incident_gamma_energy_mev) {
  material.validate();

  double total = 0.0;
  for (const auto& element : material.elements) {
    const double sigma_barn =
        klein_nishina_cross_section_per_atom_barn(
            incident_gamma_energy_mev,
            element.atomic_number_z);
    total += element.number_density_per_mm3 *
             sigma_barn * barn_to_mm2;
  }
  return total;
}

double compton_mean_free_path_mm(
    const MaterialView& material,
    const double incident_gamma_energy_mev) {
  const double sigma =
      compton_macroscopic_cross_section_per_mm(
          material, incident_gamma_energy_mev);
  if (sigma == 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  return 1.0 / sigma;
}

double sample_compton_interaction_distance_mm(
    const MaterialView& material,
    const double incident_gamma_energy_mev,
    RngAddress& rng) {
  const double sigma =
      compton_macroscopic_cross_section_per_mm(
          material, incident_gamma_energy_mev);
  if (sigma == 0.0) {
    CounterRng::increment(rng);
    return std::numeric_limits<double>::infinity();
  }

  // CounterRng::uniform_f32 is in [0,1). Map away from exactly zero so
  // -log(U) remains finite while retaining deterministic CPU/WGSL parity.
  const double u = static_cast<double>(
      CounterRng::uniform_f32(rng));
  CounterRng::increment(rng);

  constexpr double min_u = 1.0 / 16777216.0;  // 2^-24
  const double clamped_u = u > 0.0 ? u : min_u;
  return -std::log(clamped_u) / sigma;
}

}  // namespace g4wgpu
