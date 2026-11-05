#include "g4wgpu/geant4/GeometryBoundaryDistance.hh"

#include <limits>
#include <stdexcept>

#include "G4Navigator.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4TransportationManager.hh"

namespace g4wgpu {

double geant4_boundary_distance_mm(
    const G4Track& track) {
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
    return std::numeric_limits<double>::infinity();
  }

  G4Navigator navigator;
  navigator.SetWorldVolume(world);

  auto direction = track.GetMomentumDirection();
  const auto position = track.GetPosition();

  if (navigator.LocateGlobalPointAndSetup(
          position,
          &direction,
          false,
          false) == nullptr) {
    return std::numeric_limits<double>::infinity();
  }

  G4double safety = 0.0;
  const G4double boundary =
      navigator.CheckNextStep(
          position,
          direction,
          std::numeric_limits<G4double>::max(),
          safety);

  if (boundary < 0.0) {
    throw std::runtime_error(
        "Geant4 navigator returned a negative boundary distance");
  }

  return boundary / CLHEP::mm;
}

}  // namespace g4wgpu
