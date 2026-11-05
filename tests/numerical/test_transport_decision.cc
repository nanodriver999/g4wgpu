#include "g4wgpu/TransportDecision.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

int main() {
  {
    const auto d =
        g4wgpu::choose_transport_limiter(3.0, 10.0);
    if (d.limiter !=
            g4wgpu::TransportLimiter::interaction ||
        d.step_distance_mm != 3.0) {
      std::cerr << "interaction should limit the step\n";
      return EXIT_FAILURE;
    }
  }

  {
    const auto d =
        g4wgpu::choose_transport_limiter(10.0, 3.0);
    if (d.limiter !=
            g4wgpu::TransportLimiter::geometry_boundary ||
        d.step_distance_mm != 3.0) {
      std::cerr << "geometry should limit the step\n";
      return EXIT_FAILURE;
    }
  }

  {
    const auto d =
        g4wgpu::choose_transport_limiter(3.0, 3.0);
    if (d.limiter !=
        g4wgpu::TransportLimiter::geometry_boundary) {
      std::cerr << "ties must prefer geometry\n";
      return EXIT_FAILURE;
    }
  }

  {
    const double inf =
        std::numeric_limits<double>::infinity();
    const auto d =
        g4wgpu::choose_transport_limiter(inf, inf);
    if (d.limiter != g4wgpu::TransportLimiter::none ||
        !std::isinf(d.step_distance_mm)) {
      std::cerr << "double-infinity case should have no limiter\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
