#include "g4wgpu/TrackingPolicy.hh"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
  {
    g4wgpu::TrackingPolicy policy;
    policy.validate();

    if (policy.enable_authoritative_compton) {
      std::cerr << "authoritative Compton must be opt-in by default\n";
      return EXIT_FAILURE;
    }

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

  {
    // NaN must not silently bypass the non-negative threshold check.
    // Positive infinity is also not a meaningful tracking threshold.
    for (const double invalid_threshold : {
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()}) {
      g4wgpu::TrackingPolicy policy;
      policy.min_gamma_energy_mev = invalid_threshold;
      bool threw = false;
      try {
        policy.validate();
      } catch (const std::invalid_argument&) {
        threw = true;
      }
      if (!threw) {
        std::cerr << "non-finite energy threshold should be rejected\n";
        return EXIT_FAILURE;
      }
    }
  }

  return EXIT_SUCCESS;
}
