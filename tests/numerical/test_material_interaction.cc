#include "g4wgpu/MaterialInteraction.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
  g4wgpu::MaterialView carbon;
  carbon.material_id = 1;
  carbon.elements.push_back({6.0, 1.0e20});

  const double sigma =
      g4wgpu::compton_macroscopic_cross_section_per_mm(
          carbon, 1.0);
  const double mfp =
      g4wgpu::compton_mean_free_path_mm(
          carbon, 1.0);

  if (!(sigma > 0.0) || !std::isfinite(sigma) ||
      !(mfp > 0.0) || !std::isfinite(mfp)) {
    std::cerr << "invalid material Compton transport quantities\n";
    return EXIT_FAILURE;
  }

  const double reciprocal_error =
      std::fabs(sigma * mfp - 1.0);
  if (reciprocal_error > 1.0e-12) {
    std::cerr << "mean free path is not reciprocal of macroscopic sigma\n";
    return EXIT_FAILURE;
  }

  g4wgpu::MaterialView mixture;
  mixture.material_id = 2;
  mixture.elements.push_back({1.0, 2.0e19});
  mixture.elements.push_back({8.0, 1.0e19});

  if (!(g4wgpu::compton_macroscopic_cross_section_per_mm(
            mixture, 1.0) > 0.0)) {
    std::cerr << "mixture cross section should be positive\n";
    return EXIT_FAILURE;
  }

  g4wgpu::RngAddress a{1u, 2u, 3u, 4u};
  g4wgpu::RngAddress b = a;

  const double da =
      g4wgpu::sample_compton_interaction_distance_mm(
          mixture, 1.0, a);
  const double db =
      g4wgpu::sample_compton_interaction_distance_mm(
          mixture, 1.0, b);

  if (!(da > 0.0) || da != db ||
      a.counter_lo != b.counter_lo ||
      a.counter_hi != b.counter_hi) {
    std::cerr << "interaction distance RNG is not deterministic\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
