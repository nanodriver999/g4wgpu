#include "g4wgpu/CpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require_close(const double actual, const double expected,
                   const double tolerance = 1.0e-12) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "expected " << expected << ", got " << actual << '\n';
    std::exit(EXIT_FAILURE);
  }
}

}  // namespace

int main() {
  g4wgpu::CpuBackend backend;

  if (backend.name() != "cpu") {
    return EXIT_FAILURE;
  }

  {
    const std::vector<double> x{1.0, 2.0, 3.0};
    std::vector<double> y{4.0, 5.0, 6.0};

    backend.axpy(2.0, x, y);

    require_close(y[0], 6.0);
    require_close(y[1], 9.0);
    require_close(y[2], 12.0);
  }

  {
    g4wgpu::TrackBatch tracks;
    tracks.resize(2);

    tracks.position_x = {1.0, -1.0};
    tracks.position_y = {2.0, -2.0};
    tracks.position_z = {3.0, -3.0};

    tracks.direction_x = {0.5, 1.0};
    tracks.direction_y = {1.0, 0.0};
    tracks.direction_z = {-1.0, 2.0};

    backend.advance_positions(tracks, 2.0);

    require_close(tracks.position_x[0], 2.0);
    require_close(tracks.position_y[0], 4.0);
    require_close(tracks.position_z[0], 1.0);

    require_close(tracks.position_x[1], 1.0);
    require_close(tracks.position_y[1], -2.0);
    require_close(tracks.position_z[1], 1.0);
  }

  {
    g4wgpu::TrackBatch malformed;
    malformed.resize(1);
    malformed.status.clear();

    bool threw = false;
    try {
      backend.advance_positions(malformed, 1.0);
    } catch (const std::invalid_argument&) {
      threw = true;
    }

    if (!threw) {
      std::cerr << "malformed TrackBatch should fail validation\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
