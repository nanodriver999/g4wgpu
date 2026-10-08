#pragma once

#include "g4wgpu/GammaProcessCompetition.hh"
#include "g4wgpu/MaterialInteraction.hh"
#include "g4wgpu/PhotoelectricCrossSection.hh"

namespace g4wgpu {

// Assemble the three portable macroscopic gamma EM cross sections used by
// process competition.
//
// The active photoelectric Sandia segment is selected host-side. Compton and
// pair production are evaluated directly from the portable material view.
GammaProcessCrossSections portable_gamma_process_cross_sections(
    const MaterialView& material,
    const PhotoelectricSandiaSegment& photoelectric_segment,
    double incident_gamma_energy_mev);

}  // namespace g4wgpu
