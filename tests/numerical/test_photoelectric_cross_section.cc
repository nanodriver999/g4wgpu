#include "g4wgpu/PhotoelectricCrossSection.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
  g4wgpu::PhotoelectricSandiaSegment segment;
  segment.coefficients = {1.0, 2.0, 3.0, 4.0};
  segment.minimum_energy_mev = 0.1;

  const double sigma_one =
      g4wgpu::photoelectric_macroscopic_cross_section_per_mm(
          segment, 1.0);
  if (std::fabs(sigma_one - 10.0) > 1.0e-15) {
    std::cerr << "unexpected Sandia polynomial value at 1 MeV\n";
    return EXIT_FAILURE;
  }

  const double sigma_clamped =
      g4wgpu::photoelectric_macroscopic_cross_section_per_mm(
          segment, 0.05);
  const double sigma_minimum =
      g4wgpu::photoelectric_macroscopic_cross_section_per_mm(
          segment, 0.1);

  if (sigma_clamped != sigma_minimum) {
    std::cerr << "minimum-energy clamp is not deterministic\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
