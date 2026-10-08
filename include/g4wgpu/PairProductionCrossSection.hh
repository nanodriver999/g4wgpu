#pragma once

#include "g4wgpu/MaterialInteraction.hh"

namespace g4wgpu {

// Portable Urban/Hubbell parameterization used by Geant4's standard
// G4PairProductionRelModel below its high-energy numerical-integration regime.
//
// Returns barns per atom.
double pair_production_cross_section_per_atom_barn(
    double incident_gamma_energy_mev,
    double atomic_number_z);

// Material macroscopic cross section in mm^-1.
double pair_production_macroscopic_cross_section_per_mm(
    const MaterialView& material,
    double incident_gamma_energy_mev);

}  // namespace g4wgpu
