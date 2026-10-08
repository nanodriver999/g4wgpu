#pragma once

#include <array>

namespace g4wgpu {

// Portable representation of one Sandia photoelectric coefficient segment.
//
// With incident gamma energy E in MeV, the macroscopic cross section in mm^-1
// is evaluated as
//
//   sigma(E) = a0/E + a1/E^2 + a2/E^3 + a3/E^4
//
// after clamping E to minimum_energy_mev.
struct PhotoelectricSandiaSegment {
  std::array<double, 4> coefficients{};
  double minimum_energy_mev = 0.0;

  void validate() const;
};

double photoelectric_macroscopic_cross_section_per_mm(
    const PhotoelectricSandiaSegment& segment,
    double incident_gamma_energy_mev);

}  // namespace g4wgpu
