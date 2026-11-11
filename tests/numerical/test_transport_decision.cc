#include "g4wgpu/TransportDecision.hh"

#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>

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

  {
    // One infinite distance must not mask a finite limiting step.
    const double inf = std::numeric_limits<double>::infinity();
    const auto physics = g4wgpu::choose_transport_limiter(2.0, inf);
    const auto geometry = g4wgpu::choose_transport_limiter(inf, 4.0);
    if (physics.limiter != g4wgpu::TransportLimiter::interaction ||
        physics.step_distance_mm != 2.0 ||
        geometry.limiter != g4wgpu::TransportLimiter::geometry_boundary ||
        geometry.step_distance_mm != 4.0) {
      std::cerr << "finite distance must win over infinity\n";
      return EXIT_FAILURE;
    }
  }

  {
    // Invalid distances must fail before a step is handed to transport.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {nan, -inf, -1.0}) {
      for (const bool invalid_interaction : {false, true}) {
        bool threw = false;
        try {
          if (invalid_interaction) {
            (void)g4wgpu::choose_transport_limiter(invalid, 1.0);
          } else {
            (void)g4wgpu::choose_transport_limiter(1.0, invalid);
          }
        } catch (const std::invalid_argument&) {
          threw = true;
        }
        if (!threw) {
          std::cerr << "invalid transport distance was accepted\n";
          return EXIT_FAILURE;
        }
      }
    }
  }

  return EXIT_SUCCESS;
}
