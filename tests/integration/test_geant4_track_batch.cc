#include "g4wgpu/geant4/TrackBatchConversion.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "G4DynamicParticle.hh"
#include "G4Gamma.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"

namespace {

bool close_enough(const double a, const double b,
                  const double tolerance = 1.0e-12) {
  return std::fabs(a - b) <= tolerance;
}

}  // namespace

int main() {
  auto* dynamic_particle = new G4DynamicParticle(
      G4Gamma::GammaDefinition(),
      G4ThreeVector(0.0, 0.6, 0.8),
      2.5 * CLHEP::MeV);

  auto* track = new G4Track(
      dynamic_particle,
      0.0,
      G4ThreeVector(
          1.25 * CLHEP::mm,
          -2.5 * CLHEP::mm,
          4.0 * CLHEP::mm));
  track->SetTrackID(42);

  std::vector<G4Track*> tracks{track};
  const auto batch =
      g4wgpu::make_track_batch_from_geant4(
          tracks, 17u);

  if (batch.size() != 1u) {
    std::cerr << "unexpected converted batch size\n";
    delete track;
    return EXIT_FAILURE;
  }

  if (!close_enough(batch.position_x[0], 1.25) ||
      !close_enough(batch.position_y[0], -2.5) ||
      !close_enough(batch.position_z[0], 4.0)) {
    std::cerr << "position unit conversion failed\n";
    delete track;
    return EXIT_FAILURE;
  }

  if (!close_enough(batch.direction_x[0], 0.0) ||
      !close_enough(batch.direction_y[0], 0.6) ||
      !close_enough(batch.direction_z[0], 0.8)) {
    std::cerr << "direction conversion failed\n";
    delete track;
    return EXIT_FAILURE;
  }

  if (!close_enough(batch.kinetic_energy[0], 2.5)) {
    std::cerr << "energy unit conversion failed\n";
    delete track;
    return EXIT_FAILURE;
  }

  if (batch.particle_id[0] != 22u ||
      batch.rng_stream_lo[0] != 42u ||
      batch.rng_stream_hi[0] != 17u ||
      batch.rng_counter_lo[0] != 0u ||
      batch.rng_counter_hi[0] != 0u) {
    std::cerr << "identity/RNG conversion failed\n";
    delete track;
    return EXIT_FAILURE;
  }

  delete track;
  return EXIT_SUCCESS;
}
