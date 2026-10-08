#include "g4wgpu/geant4/G4WgpuTrackingManager.hh"

#include <cstdint>
#include <stdexcept>

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4Exception.hh"
#include "G4Gamma.hh"
#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"
#include "G4StackManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4TrackStatus.hh"
#include "G4TrackingManager.hh"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"

#include "g4wgpu/PortableGammaCrossSections.hh"
#include "g4wgpu/geant4/MaterialConversion.hh"
#include "g4wgpu/geant4/PhotoelectricSandiaConversion.hh"
#include "g4wgpu/geant4/TrackBatchConversion.hh"

namespace g4wgpu {
namespace {

const G4Material* resolve_track_material(
    const G4Track& track) {
  if (track.GetStep() != nullptr) {
    return track.GetMaterial();
  }

  if (track.GetVolume() != nullptr &&
      track.GetVolume()->GetLogicalVolume() != nullptr) {
    return track.GetVolume()->GetLogicalVolume()->GetMaterial();
  }

  if (track.GetLogicalVolumeAtVertex() != nullptr) {
    return track.GetLogicalVolumeAtVertex()->GetMaterial();
  }

  // Custom tracking managers receive primaries before
  // G4SteppingManager::SetInitialStep() creates the touchable/step. Resolve
  // the volume with a private navigator so we do not mutate the track or the
  // tracking navigator's state.
  auto* transportation =
      G4TransportationManager::GetTransportationManager();
  auto* tracking_navigator =
      transportation != nullptr
          ? transportation->GetNavigatorForTracking()
          : nullptr;
  auto* world =
      tracking_navigator != nullptr
          ? tracking_navigator->GetWorldVolume()
          : nullptr;
  if (world == nullptr) {
    return nullptr;
  }

  G4Navigator navigator;
  navigator.SetWorldVolume(world);
  auto direction = track.GetMomentumDirection();
  auto* volume =
      navigator.LocateGlobalPointAndSetup(
          track.GetPosition(),
          &direction,
          false,
          false);
  return volume != nullptr &&
                 volume->GetLogicalVolume() != nullptr
             ? volume->GetLogicalVolume()->GetMaterial()
             : nullptr;
}

}  // namespace

namespace {

void forward_process_table_build(
    const G4ParticleDefinition& particle,
    const bool prepare) {
  auto* process_manager = particle.GetProcessManager();
  auto* master_manager = particle.GetMasterProcessManager();

  if (process_manager == nullptr) {
    return;
  }

  auto* processes = process_manager->GetProcessList();
  if (processes == nullptr) {
    return;
  }

  for (std::size_t i = 0; i < processes->size(); ++i) {
    auto* process = (*processes)[i];
    if (process == nullptr) {
      continue;
    }

    const bool is_master = process_manager == master_manager;
    if (prepare) {
      if (is_master) {
        process->PreparePhysicsTable(particle);
      } else {
        process->PrepareWorkerPhysicsTable(particle);
      }
    } else {
      if (is_master) {
        process->BuildPhysicsTable(particle);
      } else {
        process->BuildWorkerPhysicsTable(particle);
      }
    }
  }
}

std::uint32_t to_u32(const G4int value) noexcept {
  return static_cast<std::uint32_t>(
      static_cast<std::int32_t>(value));
}

}  // namespace

G4WgpuTrackingManager::G4WgpuTrackingManager(
    TrackingPolicy policy,
    PhysicsBackend* shadow_physics_backend)
    : policy_(policy),
      shadow_physics_backend_(shadow_physics_backend) {
  policy_.validate();
  buffered_tracks_.reserve(policy_.batch_capacity);
}

G4WgpuTrackingManager::~G4WgpuTrackingManager() {
  if (!buffered_tracks_.empty()) {
    G4Exception(
        "G4WgpuTrackingManager::~G4WgpuTrackingManager",
        "G4WGPU001",
        JustWarning,
        "Destroying the tracking manager with pending tracks; "
        "the owned pending tracks will be deleted.");

    for (auto* track : buffered_tracks_) {
      delete track;
    }
    buffered_tracks_.clear();
  }
}

void G4WgpuTrackingManager::BuildPhysicsTable(
    const G4ParticleDefinition& particle) {
  forward_process_table_build(particle, false);
}

void G4WgpuTrackingManager::PreparePhysicsTable(
    const G4ParticleDefinition& particle) {
  forward_process_table_build(particle, true);
}

bool G4WgpuTrackingManager::is_gpu_candidate(
    const G4Track& track) const {
  const auto* definition = track.GetParticleDefinition();
  if (definition == nullptr ||
      definition != G4Gamma::GammaDefinition()) {
    return false;
  }

  return policy_.should_buffer_gamma(
      track.GetKineticEnergy() / CLHEP::MeV);
}

void G4WgpuTrackingManager::HandOverOneTrack(
    G4Track* track) {
  if (track == nullptr) {
    throw std::invalid_argument(
        "G4WgpuTrackingManager received a null track");
  }

  if (!is_gpu_candidate(*track)) {
    process_with_default_tracking(track);
    return;
  }

  buffered_tracks_.push_back(track);

  if (buffered_tracks_.size() >= policy_.batch_capacity) {
    flush_buffer();
  }
}

void G4WgpuTrackingManager::FlushEvent() {
  flush_buffer();
}

TrackBatch G4WgpuTrackingManager::make_batch(
    const std::vector<G4Track*>& tracks) const {
  auto* event_manager = G4EventManager::GetEventManager();
  const auto* event =
      event_manager != nullptr
          ? event_manager->GetConstCurrentEvent()
          : nullptr;
  const std::uint32_t event_id =
      event != nullptr ? to_u32(event->GetEventID()) : 0u;

  return make_track_batch_from_geant4(
      tracks, event_id);
}

void G4WgpuTrackingManager::flush_buffer() {
  if (buffered_tracks_.empty()) {
    last_flushed_batch_.resize(0);
    last_shadow_samples_.clear();
    last_shadow_process_competition_.clear();
    last_shadow_compton_final_states_.clear();
    last_flushed_batch_size_ = 0;
    return;
  }

  last_flushed_batch_ = make_batch(buffered_tracks_);
  last_flushed_batch_.validate();
  last_flushed_batch_size_ =
      last_flushed_batch_.size();

  last_shadow_samples_.clear();
  last_shadow_process_competition_.clear();
  last_shadow_compton_final_states_.clear();
  if (shadow_physics_backend_ != nullptr) {
    std::vector<RngAddress> competition_rng(
        last_flushed_batch_.size());
    std::vector<GammaProcessCrossSections> cross_sections(
        last_flushed_batch_.size());

    for (std::size_t i = 0; i < last_flushed_batch_.size(); ++i) {
      const RngAddress address{
          last_flushed_batch_.rng_stream_lo[i],
          last_flushed_batch_.rng_stream_hi[i],
          last_flushed_batch_.rng_counter_lo[i],
          last_flushed_batch_.rng_counter_hi[i]};
      competition_rng[i] = address;

      const auto* track = buffered_tracks_[i];
      const double energy_mev =
          last_flushed_batch_.kinetic_energy[i];

      // G4Track::GetMaterial() is unsafe before the first G4Step exists.
      // Resolve pre-tracking primaries through a private navigator instead.
      const G4Material* material =
          resolve_track_material(*track);

      if (material != nullptr) {
        const auto material_view =
            make_material_view_from_geant4(*material);
        const auto photoelectric_segment =
            make_photoelectric_sandia_segment_from_geant4(
                *material,
                energy_mev);

        cross_sections[i] =
            portable_gamma_process_cross_sections(
                material_view,
                photoelectric_segment,
                energy_mev);
      }
    }

    last_shadow_process_competition_ =
        shadow_physics_backend_
            ->sample_gamma_process_competition_batch(
                cross_sections,
                competition_rng);

    if (last_shadow_process_competition_.size() !=
        last_flushed_batch_.size()) {
      throw std::runtime_error(
          "shadow process competition returned an unexpected sample count");
    }

    // Only tracks for which process competition selected Compton should run
    // the Klein-Nishina final-state sampler. Continue from the RNG counters
    // consumed by process competition so the two stages do not reuse draws.
    std::vector<std::size_t> compton_indices;
    std::vector<double> compton_energies;
    std::vector<RngAddress> compton_rng;
    compton_indices.reserve(last_flushed_batch_.size());
    compton_energies.reserve(last_flushed_batch_.size());
    compton_rng.reserve(last_flushed_batch_.size());

    for (std::size_t i = 0; i < last_flushed_batch_.size(); ++i) {
      if (last_shadow_process_competition_[i].process ==
          GammaProcess::compton) {
        compton_indices.push_back(i);
        compton_energies.push_back(
            last_flushed_batch_.kinetic_energy[i]);
        compton_rng.push_back(competition_rng[i]);
      }
    }

    last_shadow_samples_.assign(
        last_flushed_batch_.size(),
        KleinNishinaSample{});

    const auto compton_samples =
        shadow_physics_backend_->sample_klein_nishina_batch(
            compton_energies,
            compton_rng);

    if (compton_samples.size() != compton_indices.size()) {
      throw std::runtime_error(
          "shadow Compton sampler returned an unexpected sample count");
    }

    for (std::size_t i = 0; i < compton_indices.size(); ++i) {
      last_shadow_samples_[compton_indices[i]] =
          compton_samples[i];
    }

    last_shadow_compton_final_states_.resize(
        last_flushed_batch_.size());

    for (const std::size_t index : compton_indices) {
      const auto& sample = last_shadow_samples_[index];
      if (!sample.accepted) {
        continue;
      }

      last_shadow_compton_final_states_[index] =
          make_compton_final_state(
              last_flushed_batch_.kinetic_energy[index],
              Vector3{
                  last_flushed_batch_.direction_x[index],
                  last_flushed_batch_.direction_y[index],
                  last_flushed_batch_.direction_z[index]},
              sample);
    }
  }

  // Shadow physics is observational only. Geant4 state is still determined
  // exclusively by the existing CPU tracking path until a later PR promotes
  // GPU results into transport state changes.
  // Actual WebGPU physics ownership is introduced only after this fallback
  // path is proven equivalent.
  auto tracks = std::move(buffered_tracks_);
  buffered_tracks_.clear();
  buffered_tracks_.reserve(policy_.batch_capacity);

  for (auto* track : tracks) {
    process_with_default_tracking(track);
  }
}

void G4WgpuTrackingManager::process_with_default_tracking(
    G4Track* track) {
  auto* event_manager =
      G4EventManager::GetEventManager();
  if (event_manager == nullptr) {
    throw std::runtime_error(
        "G4EventManager is not available");
  }

  auto* tracking_manager =
      event_manager->GetTrackingManager();
  if (tracking_manager == nullptr) {
    throw std::runtime_error(
        "default G4TrackingManager is not available");
  }

  // G4EventManager normally takes ownership of trajectories created by the
  // default tracking manager. A custom G4VTrackingManager does not have
  // access to the event manager's private trajectory container, so silently
  // dropping a requested trajectory would be incorrect.
  if (tracking_manager->GetStoreTrajectory() != 0) {
    throw std::runtime_error(
        "G4WgpuTrackingManager CPU fallback does not yet support "
        "Geant4 trajectory storage");
  }

  tracking_manager->ProcessOneTrack(track);

  auto* secondaries =
      tracking_manager->GimmeSecondaries();
  auto* stack_manager =
      event_manager->GetStackManager();
  if (stack_manager == nullptr) {
    throw std::runtime_error(
        "G4StackManager is not available");
  }

  switch (track->GetTrackStatus()) {
    case fStopButAlive:
    case fSuspend:
    case fSuspendAndWait:
      stack_manager->PushOneTrack(track);
      event_manager->StackTracks(secondaries);
      return;

    case fPostponeToNextEvent:
      stack_manager->PushOneTrack(track);
      event_manager->StackTracks(secondaries);
      return;

    case fStopAndKill:
      event_manager->StackTracks(secondaries);
      delete track;
      return;

    case fKillTrackAndSecondaries:
      if (secondaries != nullptr) {
        for (auto* secondary : *secondaries) {
          delete secondary;
        }
        secondaries->clear();
      }
      delete track;
      return;

    case fAlive:
      throw std::runtime_error(
          "default G4TrackingManager returned fAlive unexpectedly");
  }

  throw std::runtime_error(
      "default G4TrackingManager returned an unknown track status");
}

}  // namespace g4wgpu
