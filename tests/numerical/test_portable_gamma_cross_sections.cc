#include "g4wgpu/PortableGammaCrossSections.hh"

#include <cstdlib>
#include <iostream>

int main() {
  g4wgpu::MaterialView material;
  material.material_id = 42u;
  material.elements.push_back({13.0, 5.0e19});

  g4wgpu::PhotoelectricSandiaSegment photoelectric;
  photoelectric.coefficients = {
      2.0e-4,
      1.0e-5,
      0.0,
      0.0,
  };
  photoelectric.minimum_energy_mev = 0.01;

  const auto low =
      g4wgpu::portable_gamma_process_cross_sections(
          material, photoelectric, 1.0);

  if (!(low.compton_per_mm > 0.0) ||
      !(low.photoelectric_per_mm > 0.0) ||
      low.pair_production_per_mm != 0.0 ||
      !(low.total_per_mm() > 0.0)) {
    std::cerr << "invalid portable gamma cross sections below pair threshold\n";
    return EXIT_FAILURE;
  }

  const auto high =
      g4wgpu::portable_gamma_process_cross_sections(
          material, photoelectric, 10.0);

  if (!(high.compton_per_mm > 0.0) ||
      !(high.photoelectric_per_mm > 0.0) ||
      !(high.pair_production_per_mm > 0.0) ||
      !(high.total_per_mm() > 0.0)) {
    std::cerr << "invalid portable gamma cross sections above pair threshold\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
