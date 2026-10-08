#include "g4wgpu/CpuBackend.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
  constexpr std::uint32_t sample_count = 4096u;

  std::vector<g4wgpu::GammaProcessCrossSections> xs(sample_count);
  std::vector<g4wgpu::RngAddress> cpu_rng(sample_count);
  std::vector<g4wgpu::RngAddress> gpu_rng(sample_count);

  for (std::uint32_t i = 0; i < sample_count; ++i) {
    xs[i].compton_per_mm = 0.2;
    xs[i].photoelectric_per_mm = 0.3;
    xs[i].pair_production_per_mm = 0.5;

    const g4wgpu::RngAddress address{
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
      cpu.sample_gamma_process_competition_batch(
          xs, cpu_rng);
  const auto gpu_samples =
      gpu.sample_gamma_process_competition_batch(
          xs, gpu_rng);

  if (cpu_samples.size() != sample_count ||
      gpu_samples.size() != sample_count) {
    std::cerr << "unexpected gamma competition batch size\n";
    return EXIT_FAILURE;
  }

  std::size_t matching_processes = 0;
  double cpu_mean_distance = 0.0;
  double gpu_mean_distance = 0.0;

  for (std::uint32_t i = 0; i < sample_count; ++i) {
    if (cpu_samples[i].process == gpu_samples[i].process) {
      ++matching_processes;
    }

    if (!(cpu_samples[i].distance_mm > 0.0) ||
        !(gpu_samples[i].distance_mm > 0.0) ||
        !std::isfinite(cpu_samples[i].distance_mm) ||
        !std::isfinite(gpu_samples[i].distance_mm)) {
      std::cerr << "invalid sampled distance at " << i << '\n';
      return EXIT_FAILURE;
    }

    if (cpu_rng[i].counter_lo != gpu_rng[i].counter_lo ||
        cpu_rng[i].counter_hi != gpu_rng[i].counter_hi) {
      std::cerr << "CPU/GPU RNG counter mismatch at " << i << '\n';
      return EXIT_FAILURE;
    }

    cpu_mean_distance += cpu_samples[i].distance_mm;
    gpu_mean_distance += gpu_samples[i].distance_mm;
  }

  cpu_mean_distance /= sample_count;
  gpu_mean_distance /= sample_count;

  if (matching_processes <
      static_cast<std::size_t>(sample_count * 0.999)) {
    std::cerr
        << "too many CPU/GPU process-selection divergences: "
        << matching_processes << "/" << sample_count << '\n';
    return EXIT_FAILURE;
  }

  if (std::fabs(cpu_mean_distance - gpu_mean_distance) > 2.0e-5) {
    std::cerr
        << "CPU/GPU mean interaction distance differs: "
        << cpu_mean_distance << " vs "
        << gpu_mean_distance << '\n';
    return EXIT_FAILURE;
  }

  std::vector<g4wgpu::GammaProcessCrossSections> zero_xs(1);
  std::vector<g4wgpu::RngAddress> zero_rng_cpu(1);
  std::vector<g4wgpu::RngAddress> zero_rng_gpu(1);
  zero_rng_cpu[0] = {1u, 2u, 3u, 4u};
  zero_rng_gpu[0] = zero_rng_cpu[0];

  const auto zero_cpu =
      cpu.sample_gamma_process_competition_batch(
          zero_xs, zero_rng_cpu);
  const auto zero_gpu =
      gpu.sample_gamma_process_competition_batch(
          zero_xs, zero_rng_gpu);

  if (zero_cpu[0].process != g4wgpu::GammaProcess::none ||
      zero_gpu[0].process != g4wgpu::GammaProcess::none ||
      !std::isinf(zero_cpu[0].distance_mm) ||
      !std::isinf(zero_gpu[0].distance_mm) ||
      zero_rng_cpu[0].counter_lo != zero_rng_gpu[0].counter_lo ||
      zero_rng_cpu[0].counter_hi != zero_rng_gpu[0].counter_hi) {
    std::cerr << "zero-cross-section behavior differs between CPU and GPU\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
