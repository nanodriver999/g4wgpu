#include "g4wgpu/GammaProcessCompetition.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
  g4wgpu::GammaProcessCrossSections xs{
      0.2,
      0.3,
      0.5,
  };

  if (std::fabs(xs.total_per_mm() - 1.0) > 1.0e-15) {
    std::cerr << "unexpected total cross section\n";
    return EXIT_FAILURE;
  }

  g4wgpu::RngAddress a{7u, 11u, 13u, 17u};
  g4wgpu::RngAddress b = a;

  const auto sa =
      g4wgpu::sample_gamma_process_competition(xs, a);
  const auto sb =
      g4wgpu::sample_gamma_process_competition(xs, b);

  if (sa.process != sb.process ||
      sa.distance_mm != sb.distance_mm ||
      a.counter_lo != b.counter_lo ||
      a.counter_hi != b.counter_hi) {
    std::cerr << "gamma process competition is not deterministic\n";
    return EXIT_FAILURE;
  }

  if (!(sa.distance_mm > 0.0) ||
      !std::isfinite(sa.distance_mm) ||
      std::fabs(sa.total_cross_section_per_mm - 1.0) > 1.0e-15) {
    std::cerr << "invalid sampled interaction\n";
    return EXIT_FAILURE;
  }

  // Statistical process fractions should follow the supplied macroscopic
  // cross sections.
  constexpr std::uint32_t n = 60000u;
  std::uint32_t counts[4] = {0u, 0u, 0u, 0u};

  g4wgpu::RngAddress rng{123u, 456u, 0u, 0u};
  for (std::uint32_t i = 0; i < n; ++i) {
    const auto s =
        g4wgpu::sample_gamma_process_competition(xs, rng);
    ++counts[static_cast<std::uint32_t>(s.process)];
  }

  const double f_compton =
      static_cast<double>(counts[1]) / n;
  const double f_photoelectric =
      static_cast<double>(counts[2]) / n;
  const double f_pair =
      static_cast<double>(counts[3]) / n;

  if (std::fabs(f_compton - 0.2) > 0.01 ||
      std::fabs(f_photoelectric - 0.3) > 0.01 ||
      std::fabs(f_pair - 0.5) > 0.01) {
    std::cerr
        << "process fractions do not match supplied cross sections: "
        << f_compton << ", "
        << f_photoelectric << ", "
        << f_pair << '\n';
    return EXIT_FAILURE;
  }

  // The sampled free-path distribution should have mean 1 / Sigma = 1 mm.
  g4wgpu::RngAddress distance_rng{999u, 111u, 0u, 0u};
  double sum_distance = 0.0;
  for (std::uint32_t i = 0; i < n; ++i) {
    sum_distance +=
        g4wgpu::sample_gamma_process_competition(
            xs, distance_rng)
            .distance_mm;
  }

  const double mean_distance =
      sum_distance / n;

  if (std::fabs(mean_distance - 1.0) > 0.02) {
    std::cerr
        << "mean interaction distance mismatch: "
        << mean_distance << '\n';
    return EXIT_FAILURE;
  }

  g4wgpu::GammaProcessCrossSections zero{};
  const auto none =
      g4wgpu::sample_gamma_process_competition(
          zero, rng);

  if (none.process != g4wgpu::GammaProcess::none ||
      !std::isinf(none.distance_mm)) {
    std::cerr << "zero-cross-section case should produce no interaction\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
