#include "g4wgpu/geant4/G4WgpuTrackingManager.hh"

#include <cstdint>
#include <exception>
#include <stdexcept>

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4Gamma.hh"
#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"
#include "G4StackManager.hh"
#include "G4Track.hh"
#include "G4TrackStatus.hh"
#include "G4TrackingManager.hh"
#include "G4VProcess.hh"

#include "g4wgpu/geant4/TrackBatchConversion.hh"

namespace g4wgpu {
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
    TrackingPolicy policy)
    : policy_(policy) {
  policy_.validate();
  buffered_tracks_.reserve(policy_.batch_capacity);
}

G4WgpuTrackingManager::~G4WgpuTrackingManager() {
  // Geant4 owns tracks handed to a custom tracking manager only until the
  // hand-over call. Once buffered, this manager owns them. Destruction during
  // an active event is therefore a lifecycle error rather than something that
  // should silently delete tracks.
  if (!buffered_tracks_.empty()) {
    std::terminate();
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
    last_flushed_batch_size_ = 0;
    return;
  }

  last_flushed_batch_ = make_batch(buffered_tracks_);
  last_flushed_batch_.validate();
  last_flushed_batch_size_ =
      last_flushed_batch_.size();

  // PR #6 validates the Geant4 batching/lifecycle boundary first.
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
