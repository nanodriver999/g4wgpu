#include "g4wgpu/TrackingPolicy.hh"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
  {
    g4wgpu::TrackingPolicy policy;
    policy.validate();

    if (!policy.should_buffer_gamma(0.0) ||
        !policy.should_buffer_gamma(10.0)) {
      std::cerr << "default tracking policy should accept gamma energies\n";
      return EXIT_FAILURE;
    }
  }

  {
    g4wgpu::TrackingPolicy policy;
    policy.min_gamma_energy_mev = 1.0;
    policy.batch_capacity = 32;
    policy.validate();

    if (policy.should_buffer_gamma(0.999) ||
        !policy.should_buffer_gamma(1.0) ||
        !policy.should_buffer_gamma(10.0)) {
      std::cerr << "energy threshold policy is incorrect\n";
      return EXIT_FAILURE;
    }
  }

  {
    g4wgpu::TrackingPolicy policy;
    policy.batch_capacity = 0;

    bool threw = false;
    try {
      policy.validate();
    } catch (const std::invalid_argument&) {
      threw = true;
    }

    if (!threw) {
      std::cerr << "zero batch capacity should be rejected\n";
      return EXIT_FAILURE;
    }
  }

  {
    g4wgpu::TrackingPolicy policy;
    policy.min_gamma_energy_mev = -1.0;

    bool threw = false;
    try {
      policy.validate();
    } catch (const std::invalid_argument&) {
      threw = true;
    }

    if (!threw) {
      std::cerr << "negative energy threshold should be rejected\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
