#include "g4wgpu/FlatBoxNavigationWebGpu.hh"
#include "g4wgpu/WebGpuBackend.hh"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

// Opt-in end-to-end wall-time experiment, NOT a performance regression gate.
// Includes GPU setup, transfers, dispatch, readback and f64 validation.
int main() {
  g4wgpu::FlatBoxScene scene;
  scene.world = {{100.0, 100.0, 100.0}};
  scene.daughters = {
      {11u, {{8.0, 10.0, 12.0}}, {-35.0, 0.0, 0.0}},
      {22u, {{10.0, 8.0, 14.0}}, {25.0, 0.0, 0.0}},
      {33u, {{5.0, 6.0, 7.0}}, {0.0, 0.0, 30.0}}};
  g4wgpu::WebGpuBackend gpu;
  volatile double checksum = 0.0;
  std::cout << "batch,method,repeat,wall_ms,cpu_substitutions,dispatches\n";
  for (const std::size_t count : {1u, 32u, 256u, 1024u}) {
    std::vector<g4wgpu::BoxRay> rays;
    rays.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const double y = (static_cast<int>(i % 21) - 10) * 0.3;
      rays.push_back({{{0.0, y, 0.0}}, {{1.0, 0.0, 0.0}}});
    }
    // Warm-up is deliberately outside measured intervals.
    (void)g4wgpu::flat_box_next_boundary_webgpu_fused_batch(gpu, scene, rays);
    for (int repeat = 0; repeat < 3; ++repeat) {
      const auto cpu_start = std::chrono::steady_clock::now();
      std::vector<g4wgpu::FlatBoundary> cpu;
      cpu.reserve(count);
      for (const auto& ray : rays) {
        cpu.push_back(g4wgpu::flat_box_next_boundary_mm(
            scene, ray.position_mm, ray.direction));
      }
      const auto cpu_end = std::chrono::steady_clock::now();
      g4wgpu::FlatBoxGpuBatchStats stats;
      const auto gpu_start = std::chrono::steady_clock::now();
      const auto checked = g4wgpu::flat_box_next_boundary_webgpu_fused_batch(
          gpu, scene, rays, &stats);
      const auto gpu_end = std::chrono::steady_clock::now();
      if (checked.size() != cpu.size()) return EXIT_FAILURE;
      for (std::size_t i = 0; i < count; ++i) {
        if (checked[i].kind != cpu[i].kind ||
            checked[i].daughter_id != cpu[i].daughter_id ||
            !std::isfinite(checked[i].distance_mm) ||
            std::fabs(checked[i].distance_mm - cpu[i].distance_mm) >
                6e-4 + 5e-5 * cpu[i].distance_mm) {
          std::cerr << "CPU/GPU disagreement at " << i << '\n';
          return EXIT_FAILURE;
        }
        checksum += checked[i].distance_mm;
      }
      const auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
      };
      std::cout << count << ",cpu," << repeat << ","
                << ms(cpu_start, cpu_end) << ",0,0\n";
      std::cout << count << ",fused_validated," << repeat << ","
                << ms(gpu_start, gpu_end) << ","
                << stats.cpu_reference_fallbacks << ","
                << stats.gpu_fused_dispatches << '\n';
    }
  }
  if (!std::isfinite(checksum)) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
