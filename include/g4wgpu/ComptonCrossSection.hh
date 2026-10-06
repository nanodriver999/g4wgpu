#pragma once

namespace g4wgpu {

// Geant4-compatible Klein-Nishina Compton cross-section parameterization.
//
// Inputs:
//   incident_gamma_energy_mev : gamma kinetic energy in MeV
//   atomic_number_z           : target atomic number
//
// Return value:
//   cross section per atom in barns
//
// This function mirrors the numerical parameterization used by
// G4KleinNishinaCompton::ComputeCrossSectionPerAtom, but is kept independent
// of Geant4 types so it can be reused by CPU/WebGPU interaction-selection
// code.
double klein_nishina_cross_section_per_atom_barn(
    double incident_gamma_energy_mev,
    double atomic_number_z);

}  // namespace g4wgpu
