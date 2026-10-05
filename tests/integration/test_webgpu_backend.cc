#include "g4wgpu/WebGpuBackend.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require_close(const double actual, const double expected,
                   const double tolerance = 1.0e-5) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "expected " << expected << ", got " << actual << '\n';
    std::exit(EXIT_FAILURE);
  }
}

}  // namespace

int main() {
  g4wgpu::WebGpuBackend backend;

  const std::vector<double> x{1.0, 2.0, 3.0, 4.0};
  std::vector<double> y{10.0, 20.0, 30.0, 40.0};

  backend.axpy(2.0, x, y);

  require_close(y[0], 12.0);
  require_close(y[1], 24.0);
  require_close(y[2], 36.0);
  require_close(y[3], 48.0);

  g4wgpu::TrackBatch tracks;
  tracks.resize(1);
  tracks.position_x[0] = 1.0;
  tracks.position_y[0] = 2.0;
  tracks.position_z[0] = 3.0;
  tracks.direction_x[0] = 0.5;
  tracks.direction_y[0] = 1.0;
  tracks.direction_z[0] = -1.0;

  backend.advance_positions(tracks, 2.0);

  require_close(tracks.position_x[0], 2.0);
  require_close(tracks.position_y[0], 4.0);
  require_close(tracks.position_z[0], 1.0);

  return EXIT_SUCCESS;
}
