#include "g4wgpu/GammaProcessCompetition.hh"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace g4wgpu {

void GammaProcessCrossSections::validate() const {
  const double values[] = {
      compton_per_mm,
      photoelectric_per_mm,
      pair_production_per_mm,
  };

  for (const double value : values) {
    if (!(value >= 0.0) || !std::isfinite(value)) {
      throw std::invalid_argument(
          "gamma process cross sections must be finite and non-negative");
    }
  }
}

double GammaProcessCrossSections::total_per_mm() const noexcept {
  return compton_per_mm +
         photoelectric_per_mm +
         pair_production_per_mm;
}

GammaInteractionSample sample_gamma_process_competition(
    const GammaProcessCrossSections& cross_sections,
    RngAddress& rng) {
  cross_sections.validate();

  const double total = cross_sections.total_per_mm();
  if (total == 0.0) {
    CounterRng::increment(rng);
    CounterRng::increment(rng);
    return GammaInteractionSample{
        GammaProcess::none,
        std::numeric_limits<double>::infinity(),
        0.0};
  }

  const double u_distance =
      static_cast<double>(CounterRng::uniform_f32(rng));
  CounterRng::increment(rng);

  constexpr double min_u = 1.0 / 16777216.0;
  const double clamped_u =
      u_distance > 0.0 ? u_distance : min_u;
  const double distance =
      -std::log(clamped_u) / total;

  const double u_process =
      static_cast<double>(CounterRng::uniform_f32(rng));
  CounterRng::increment(rng);

  const double threshold =
      u_process * total;

  GammaProcess process = GammaProcess::pair_production;

  if (threshold < cross_sections.compton_per_mm) {
    process = GammaProcess::compton;
  } else if (
      threshold <
      cross_sections.compton_per_mm +
          cross_sections.photoelectric_per_mm) {
    process = GammaProcess::photoelectric;
  }

  return GammaInteractionSample{
      process,
      distance,
      total};
}

}  // namespace g4wgpu
