#include "g4wgpu/CpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require_close(const float actual, const float expected,
                   const float tolerance = 1.0e-6f) {
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
    const std::vector<double> x{1.0f, 2.0f, 3.0f};
    std::vector<double> y{4.0f, 5.0f, 6.0f};

    backend.axpy(2.0f, x, y);

    require_close(y[0], 6.0f);
    require_close(y[1], 9.0f);
    require_close(y[2], 12.0f);
  }

  {
    g4wgpu::TrackBatch tracks;
    tracks.resize(2);

    tracks.position_x = {1.0f, -1.0f};
    tracks.position_y = {2.0f, -2.0f};
    tracks.position_z = {3.0f, -3.0f};

    tracks.direction_x = {0.5f, 1.0f};
    tracks.direction_y = {1.0f, 0.0f};
    tracks.direction_z = {-1.0f, 2.0f};

    backend.advance_positions(tracks, 2.0f);

    require_close(tracks.position_x[0], 2.0f);
    require_close(tracks.position_y[0], 4.0f);
    require_close(tracks.position_z[0], 1.0f);

    require_close(tracks.position_x[1], 1.0f);
    require_close(tracks.position_y[1], -2.0f);
    require_close(tracks.position_z[1], 1.0f);
  }

  {
    g4wgpu::TrackBatch malformed;
    malformed.resize(1);
    malformed.status.clear();

    bool threw = false;
    try {
      backend.advance_positions(malformed, 1.0f);
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
