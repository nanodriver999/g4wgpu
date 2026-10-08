#include "g4wgpu/PhotoelectricCrossSection.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace g4wgpu {

void PhotoelectricSandiaSegment::validate() const {
  if (!(minimum_energy_mev >= 0.0) ||
      !std::isfinite(minimum_energy_mev)) {
    throw std::invalid_argument(
        "photoelectric minimum energy must be finite and non-negative");
  }

  for (const double value : coefficients) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument(
          "photoelectric Sandia coefficients must be finite");
    }
  }
}

double photoelectric_macroscopic_cross_section_per_mm(
    const PhotoelectricSandiaSegment& segment,
    const double incident_gamma_energy_mev) {
  segment.validate();

  if (!(incident_gamma_energy_mev > 0.0) ||
      !std::isfinite(incident_gamma_energy_mev)) {
    throw std::invalid_argument(
        "gamma kinetic energy must be finite and positive");
  }

  const double energy_mev =
      std::max(
          incident_gamma_energy_mev,
          segment.minimum_energy_mev);
  const double inverse_energy = 1.0 / energy_mev;

  return inverse_energy *
         (segment.coefficients[0] +
          inverse_energy *
              (segment.coefficients[1] +
               inverse_energy *
                   (segment.coefficients[2] +
                    inverse_energy *
                        segment.coefficients[3])));
}

}  // namespace g4wgpu
