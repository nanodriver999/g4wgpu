#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main() {
  g4wgpu::WebGpuBackend backend;
  const g4wgpu::WebGpuAdapterInfo& info = backend.adapter_info();

  // These are attributes of the actual adapter chosen by wgpu-native.
  std::cout << "adapter_device=" << info.device << '\n'
            << "adapter_description=" << info.description << '\n'
            << "adapter_backend=" << info.backend << '\n'
            << "adapter_type=" << info.adapter_type << '\n';
  if (info.device.empty() || info.backend.empty() ||
      info.adapter_type.empty()) {
    std::cerr << "selected WebGPU adapter information is missing\n";
    return EXIT_FAILURE;
  }

  const char* expected = std::getenv("G4WGPU_EXPECT_SOFTWARE_VULKAN");
  if (expected != nullptr && std::string(expected) == "1" &&
      (info.backend != "vulkan" || info.adapter_type != "cpu")) {
    std::cerr << "expected software Vulkan/CPU, but WebGPU selected "
              << info.backend << "/" << info.adapter_type << '\n';
    return EXIT_FAILURE;
  }

  // Confirm WGSL really executes on the selected adapter, rather
  // than passing a system Vulkan inventory-only check.
  const std::vector<double> x{1.0, 3.0};
  std::vector<double> y{2.0, 4.0};
  backend.axpy(2.0, x, y);
  if (y.size() != 2u || std::fabs(y[0] - 4.0) > 1e-5 ||
      std::fabs(y[1] - 10.0) > 1e-5) {
    std::cerr << "WebGPU adapter WGSL smoke result mismatch\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
