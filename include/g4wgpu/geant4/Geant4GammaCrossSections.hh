#pragma once

#include "g4wgpu/GammaProcessCompetition.hh"

class G4Material;

namespace g4wgpu {

// Returns Geant4's currently configured gamma EM macroscopic cross sections
// in mm^-1 for the supplied material and kinetic energy.
//
// Requires Geant4 EM physics to be initialized so process tables exist.
GammaProcessCrossSections geant4_gamma_process_cross_sections(
    const G4Material& material,
    double incident_gamma_energy_mev);

}  // namespace g4wgpu
