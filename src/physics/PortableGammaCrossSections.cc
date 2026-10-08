#include "g4wgpu/PortableGammaCrossSections.hh"

#include "g4wgpu/PairProductionCrossSection.hh"

namespace g4wgpu {

GammaProcessCrossSections portable_gamma_process_cross_sections(
    const MaterialView& material,
    const PhotoelectricSandiaSegment& photoelectric_segment,
    const double incident_gamma_energy_mev) {
  GammaProcessCrossSections result;
  result.compton_per_mm =
      compton_macroscopic_cross_section_per_mm(
          material, incident_gamma_energy_mev);
  result.photoelectric_per_mm =
      photoelectric_macroscopic_cross_section_per_mm(
          photoelectric_segment,
          incident_gamma_energy_mev);
  result.pair_production_per_mm =
      pair_production_macroscopic_cross_section_per_mm(
          material, incident_gamma_energy_mev);

  result.validate();
  return result;
}

}  // namespace g4wgpu
