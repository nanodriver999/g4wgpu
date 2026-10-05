#pragma once

#include <cstddef>
#include <vector>

#include "g4wgpu/TrackBatch.hh"

namespace g4wgpu {

// Returns source indices for active tracks. This preserves stable input order
// and is suitable for building a compact GPU dispatch batch.
std::vector<std::size_t> active_indices(const TrackBatch& tracks);

// Creates a compacted copy containing only active tracks.
// Track order and RNG stream/counter values are preserved.
TrackBatch compact_active(const TrackBatch& tracks);

}  // namespace g4wgpu
