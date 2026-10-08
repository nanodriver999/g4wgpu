#include "g4wgpu/PairProductionCrossSection.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
  constexpr double threshold_mev =
      2.0 * 0.51099895;

  if (g4wgpu::pair_production_cross_section_per_atom_barn(
          threshold_mev, 13.0) != 0.0) {
    std::cerr << "pair production must vanish at threshold\n";
    return EXIT_FAILURE;
  }

  const double just_above =
      g4wgpu::pair_production_cross_section_per_atom_barn(
          1.1, 13.0);
  const double at_1p5 =
      g4wgpu::pair_production_cross_section_per_atom_barn(
          1.5, 13.0);
  const double high =
      g4wgpu::pair_production_cross_section_per_atom_barn(
          10.0, 13.0);

  if (!(just_above > 0.0) ||
      !(at_1p5 > just_above) ||
      !(high > at_1p5)) {
    std::cerr << "unexpected pair-production energy dependence\n";
    return EXIT_FAILURE;
  }

  const double hydrogen =
      g4wgpu::pair_production_cross_section_per_atom_barn(
          10.0, 1.0);
  const double lead =
      g4wgpu::pair_production_cross_section_per_atom_barn(
          10.0, 82.0);

  if (!(lead > hydrogen)) {
    std::cerr << "pair-production cross section should increase strongly with Z\n";
    return EXIT_FAILURE;
  }

  g4wgpu::MaterialView material;
  material.material_id = 7u;
  material.elements.push_back({13.0, 5.0e19});

  const double macro =
      g4wgpu::pair_production_macroscopic_cross_section_per_mm(
          material, 10.0);

  if (!(macro > 0.0) || !std::isfinite(macro)) {
    std::cerr << "invalid material pair-production cross section\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
