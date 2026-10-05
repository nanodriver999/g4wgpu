#include "g4wgpu/geant4/TrackBatchConversion.hh"

#include <cstdint>
#include <stdexcept>

#include "G4Material.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"

#include "g4wgpu/TrackStatus.hh"

namespace g4wgpu {
namespace {

std::uint32_t to_u32(const G4int value) noexcept {
  return static_cast<std::uint32_t>(
      static_cast<std::int32_t>(value));
}

}  // namespace

TrackBatch make_track_batch_from_geant4(
    const std::vector<G4Track*>& tracks,
    const std::uint32_t event_id) {
  TrackBatch batch;
  batch.resize(tracks.size());

  for (std::size_t i = 0; i < tracks.size(); ++i) {
    const auto* track = tracks[i];
    if (track == nullptr) {
      throw std::invalid_argument(
          "cannot convert a null G4Track");
    }

    const auto& position = track->GetPosition();
    const auto& direction = track->GetMomentumDirection();

    batch.position_x[i] = position.x() / mm;
    batch.position_y[i] = position.y() / mm;
    batch.position_z[i] = position.z() / mm;

    batch.direction_x[i] = direction.x();
    batch.direction_y[i] = direction.y();
    batch.direction_z[i] = direction.z();

    batch.kinetic_energy[i] =
        track->GetKineticEnergy() / MeV;

    const auto* definition =
        track->GetParticleDefinition();
    batch.particle_id[i] =
        definition != nullptr
            ? to_u32(definition->GetPDGEncoding())
            : 0u;

    const auto* material = track->GetMaterial();
    batch.material_id[i] =
        material != nullptr
            ? static_cast<std::uint32_t>(
                  material->GetIndex())
            : 0u;

    batch.rng_stream_lo[i] =
        to_u32(track->GetTrackID());
    batch.rng_stream_hi[i] = event_id;
    batch.rng_counter_lo[i] = 0u;
    batch.rng_counter_hi[i] = 0u;
    batch.status[i] =
        static_cast<std::uint32_t>(
            TrackStatus::Active);
  }

  batch.validate();
  return batch;
}

}  // namespace g4wgpu
