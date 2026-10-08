#pragma once

#include "g4wgpu/GammaProcessCompetition.hh"
#include "g4wgpu/MaterialInteraction.hh"
#include "g4wgpu/PhotoelectricCrossSection.hh"

namespace g4wgpu {

// Assemble the three portable macroscopic gamma EM cross sections used by the
// process-competition layer.
//
// The photoelectric Sandia segment is selected host-side for the material and
// energy; Compton and pair-production are evaluated from MaterialView.
GammaProcessCrossSections portable_gamma_process_cross_sections(
    const MaterialView& material,
    const PhotoelectricSandiaSegment& photoelectric_segment,
    double incident_gamma_energy_mev);

}  // namespace g4wgpu
