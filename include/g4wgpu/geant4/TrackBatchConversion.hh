#pragma once

#include <cstdint>
#include <vector>

#include "g4wgpu/TrackBatch.hh"

class G4Track;

namespace g4wgpu {

TrackBatch make_track_batch_from_geant4(
    const std::vector<G4Track*>& tracks,
    std::uint32_t event_id);

}  // namespace g4wgpu
