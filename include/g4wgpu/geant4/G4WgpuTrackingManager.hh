#pragma once

#include <cstddef>
#include <vector>

#include "G4VTrackingManager.hh"

#include "g4wgpu/GammaProcessCompetition.hh"
#include "g4wgpu/KleinNishina.hh"
#include "g4wgpu/PhysicsBackend.hh"
#include "g4wgpu/TrackBatch.hh"
#include "g4wgpu/TrackingPolicy.hh"

class G4ParticleDefinition;
class G4Track;

namespace g4wgpu {

class G4WgpuTrackingManager final : public G4VTrackingManager {
 public:
  explicit G4WgpuTrackingManager(
      TrackingPolicy policy = {},
      PhysicsBackend* shadow_physics_backend = nullptr);
  ~G4WgpuTrackingManager() override;

  G4WgpuTrackingManager(
      const G4WgpuTrackingManager&) = delete;
  G4WgpuTrackingManager& operator=(
      const G4WgpuTrackingManager&) = delete;

  void BuildPhysicsTable(
      const G4ParticleDefinition& particle) override;
  void PreparePhysicsTable(
      const G4ParticleDefinition& particle) override;

  void HandOverOneTrack(G4Track* track) override;
  void FlushEvent() override;

  [[nodiscard]] std::size_t pending_track_count() const noexcept {
    return buffered_tracks_.size();
  }

  [[nodiscard]] std::size_t last_flushed_batch_size() const noexcept {
    return last_flushed_batch_size_;
  }

  // Snapshot of the most recently flushed portable batch. It is intended
  // for validation/profiling while the actual GPU transport path is being
  // introduced incrementally.
  [[nodiscard]] const TrackBatch& last_flushed_batch() const noexcept {
    return last_flushed_batch_;
  }

  [[nodiscard]] const std::vector<KleinNishinaSample>&
  last_shadow_samples() const noexcept {
    return last_shadow_samples_;
  }

  [[nodiscard]] const std::vector<GammaInteractionSample>&
  last_shadow_process_competition() const noexcept {
    return last_shadow_process_competition_;
  }

  [[nodiscard]] bool shadow_physics_enabled() const noexcept {
    return shadow_physics_backend_ != nullptr;
  }

 private:
  bool is_gpu_candidate(const G4Track& track) const;
  void flush_buffer();
  void process_with_default_tracking(G4Track* track);
  TrackBatch make_batch(
      const std::vector<G4Track*>& tracks) const;

  TrackingPolicy policy_;
  PhysicsBackend* shadow_physics_backend_ = nullptr;
  std::vector<G4Track*> buffered_tracks_;
  TrackBatch last_flushed_batch_;
  std::vector<KleinNishinaSample> last_shadow_samples_;
  std::vector<GammaInteractionSample> last_shadow_process_competition_;
  std::size_t last_flushed_batch_size_ = 0;
};

}  // namespace g4wgpu
