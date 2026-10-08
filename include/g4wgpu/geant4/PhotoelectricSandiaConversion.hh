#pragma once

#include "g4wgpu/PhotoelectricCrossSection.hh"

class G4Material;

namespace g4wgpu {

// Extracts the Sandia coefficient segment selected by Geant4 for the given
// material and energy, normalized to the portable MeV/mm contract.
PhotoelectricSandiaSegment make_photoelectric_sandia_segment_from_geant4(
    const G4Material& material,
    double incident_gamma_energy_mev);

}  // namespace g4wgpu
