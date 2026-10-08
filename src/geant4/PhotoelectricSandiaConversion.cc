#include "g4wgpu/geant4/PhotoelectricSandiaConversion.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "G4Material.hh"
#include "G4SandiaTable.hh"
#include "G4SystemOfUnits.hh"

namespace g4wgpu {

PhotoelectricSandiaSegment make_photoelectric_sandia_segment_from_geant4(
    const G4Material& material,
    const double incident_gamma_energy_mev) {
  if (!(incident_gamma_energy_mev > 0.0) ||
      !std::isfinite(incident_gamma_energy_mev)) {
    throw std::invalid_argument(
        "gamma kinetic energy must be finite and positive");
  }

  const auto* table = material.GetSandiaTable();
  if (table == nullptr ||
      table->GetMatNbOfIntervals() <= 0) {
    PhotoelectricSandiaSegment empty;
    empty.minimum_energy_mev = 0.0;
    empty.validate();
    return empty;
  }

  const double minimum_energy_internal =
      table->GetSandiaCofForMaterial(0, 0);
  const double requested_energy_internal =
      incident_gamma_energy_mev * CLHEP::MeV;
  const double selected_energy_internal =
      std::max(
          requested_energy_internal,
          minimum_energy_internal);

  const double* raw =
      table->GetSandiaCofForMaterial(
          selected_energy_internal);
  if (raw == nullptr) {
    throw std::runtime_error(
        "Geant4 Sandia table returned no coefficients");
  }

  PhotoelectricSandiaSegment result;
  result.minimum_energy_mev =
      minimum_energy_internal / CLHEP::MeV;

  // Convert the dimensional Geant4 coefficients so that the portable
  // polynomial consumes E in MeV and returns sigma in mm^-1.
  double energy_scale = CLHEP::MeV;
  for (std::size_t i = 0; i < result.coefficients.size(); ++i) {
    result.coefficients[i] =
        raw[i] * CLHEP::mm / energy_scale;
    energy_scale *= CLHEP::MeV;
  }

  result.validate();
  return result;
}

}  // namespace g4wgpu
