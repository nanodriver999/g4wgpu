#pragma once

class G4Track;

namespace g4wgpu {

// Computes the distance in mm from the track position to the next geometry
// boundary along its current direction. Uses a private navigator so the
// tracking navigator state is not modified.
double geant4_boundary_distance_mm(
    const G4Track& track);

}  // namespace g4wgpu
