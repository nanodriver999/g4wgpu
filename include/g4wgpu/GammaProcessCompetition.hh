#pragma once

#include <cstdint>

#include "g4wgpu/CounterRng.hh"

namespace g4wgpu {

enum class GammaProcess : std::uint32_t {
  none = 0,
  compton = 1,
  photoelectric = 2,
  pair_production = 3,
};

struct GammaProcessCrossSections {
  double compton_per_mm = 0.0;
  double photoelectric_per_mm = 0.0;
  double pair_production_per_mm = 0.0;

  void validate() const;
  [[nodiscard]] double total_per_mm() const noexcept;
};

struct GammaInteractionSample {
  GammaProcess process = GammaProcess::none;
  double distance_mm = 0.0;
  double total_cross_section_per_mm = 0.0;
};

// Samples the distance to the next gamma EM interaction from the sum of
// supplied macroscopic cross sections and then chooses the winning process
// proportional to its cross section. The RNG address is advanced twice.
GammaInteractionSample sample_gamma_process_competition(
    const GammaProcessCrossSections& cross_sections,
    RngAddress& rng);

}  // namespace g4wgpu
