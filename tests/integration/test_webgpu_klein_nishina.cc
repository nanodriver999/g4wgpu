#include "g4wgpu/CpuBackend.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

bool physical_sample(
    const g4wgpu::KleinNishinaSample& sample,
    const double incident) {
  if (!sample.accepted) {
    return false;
  }

  if (!(sample.scattered_gamma_energy_mev > 0.0 &&
        sample.scattered_gamma_energy_mev <= incident)) {
    return false;
  }

  if (!(sample.recoil_electron_energy_mev >= 0.0 &&
        sample.recoil_electron_energy_mev < incident)) {
    return false;
  }

  if (std::fabs(
          sample.scattered_gamma_energy_mev +
              sample.recoil_electron_energy_mev -
              incident) > 2.0e-5) {
    return false;
  }

  if (!(sample.cos_theta >= -1.00001 &&
        sample.cos_theta <= 1.00001 &&
        sample.sin_theta >= 0.0 &&
        sample.sin_theta <= 1.00001)) {
    return false;
  }

  if (std::fabs(
          sample.cos_theta * sample.cos_theta +
              sample.sin_theta * sample.sin_theta -
              1.0) > 5.0e-5) {
    return false;
  }

  return sample.phi >= 0.0 &&
         sample.phi < 6.28319;
}

}  // namespace

int main() {
  constexpr std::uint32_t sample_count = 4096u;

  std::vector<double> energies(sample_count, 1.0);
  std::vector<g4wgpu::RngAddress> cpu_rng(sample_count);
  std::vector<g4wgpu::RngAddress> gpu_rng(sample_count);

  for (std::uint32_t i = 0; i < sample_count; ++i) {
    g4wgpu::RngAddress address{
        i + 1u,
        0x677075u,
        0u,
        0u};
    cpu_rng[i] = address;
    gpu_rng[i] = address;
  }

  g4wgpu::CpuBackend cpu;
  g4wgpu::WebGpuBackend gpu;

  const auto cpu_samples =
      cpu.sample_klein_nishina_batch(
          energies, cpu_rng);
  const auto gpu_samples =
      gpu.sample_klein_nishina_batch(
          energies, gpu_rng);

  if (cpu_samples.size() != sample_count ||
      gpu_samples.size() != sample_count) {
    std::cerr << "unexpected Klein-Nishina batch size\n";
    return EXIT_FAILURE;
  }

  double cpu_mean_energy_fraction = 0.0;
  double gpu_mean_energy_fraction = 0.0;
  double cpu_mean_mu = 0.0;
  double gpu_mean_mu = 0.0;

  for (std::uint32_t i = 0; i < sample_count; ++i) {
    if (!physical_sample(cpu_samples[i], energies[i])) {
      std::cerr << "invalid CPU sample at " << i << '\n';
      return EXIT_FAILURE;
    }
    if (!physical_sample(gpu_samples[i], energies[i])) {
      std::cerr << "invalid GPU sample at " << i << '\n';
      return EXIT_FAILURE;
    }

    cpu_mean_energy_fraction +=
        cpu_samples[i].scattered_gamma_energy_mev;
    gpu_mean_energy_fraction +=
        gpu_samples[i].scattered_gamma_energy_mev;

    cpu_mean_mu += cpu_samples[i].cos_theta;
    gpu_mean_mu += gpu_samples[i].cos_theta;
  }

  cpu_mean_energy_fraction /= sample_count;
  gpu_mean_energy_fraction /= sample_count;
  cpu_mean_mu /= sample_count;
  gpu_mean_mu /= sample_count;

  if (std::fabs(
          cpu_mean_energy_fraction -
          gpu_mean_energy_fraction) > 7.5e-3) {
    std::cerr
        << "CPU/GPU mean energy fraction differs: "
        << cpu_mean_energy_fraction << " vs "
        << gpu_mean_energy_fraction << '\n';
    return EXIT_FAILURE;
  }

  if (std::fabs(cpu_mean_mu - gpu_mean_mu) > 7.5e-3) {
    std::cerr
        << "CPU/GPU mean cos(theta) differs: "
        << cpu_mean_mu << " vs "
        << gpu_mean_mu << '\n';
    return EXIT_FAILURE;
  }

  // The exact counter value may differ for a small number of samples if
  // f32 and double rejection branches diverge, but the vast majority should
  // consume the same number of random values.
  std::size_t matching_counters = 0;
  for (std::uint32_t i = 0; i < sample_count; ++i) {
    if (cpu_rng[i].counter_lo == gpu_rng[i].counter_lo &&
        cpu_rng[i].counter_hi == gpu_rng[i].counter_hi) {
      ++matching_counters;
    }
  }

  if (matching_counters <
      static_cast<std::size_t>(sample_count * 0.98)) {
    std::cerr
        << "too many CPU/GPU rejection-path divergences: "
        << matching_counters << "/" << sample_count << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
