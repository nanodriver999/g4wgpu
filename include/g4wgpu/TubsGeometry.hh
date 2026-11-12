#pragma once

namespace g4wgpu {

// Geant4 G4Tubs subset: a solid, full-azimuth cylinder centered at the local
// origin, with no inner hole or phi clipping. All lengths are in millimeters.
// POD fields remain independent of Geant4 object pointers and GPU handles.
struct TubsVector3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct TubsRay {
  TubsVector3 position_mm;
  TubsVector3 direction;
};

struct PortableSolidTubs {
  double outer_radius_mm = 0.0;
  double half_length_z_mm = 0.0;
};

// Return +infinity when an external ray does not enter the solid cylinder.
// Positions are local; direction vectors are normalized internally.
// A point on or inside the solid has zero entry distance.
double tubs_distance_to_in_mm(
    const PortableSolidTubs& tubs,
    TubsVector3 position_mm,
    TubsVector3 direction);

// Require an origin on/inside the solid; returns the first outward boundary.
double tubs_distance_to_out_mm(
    const PortableSolidTubs& tubs,
    TubsVector3 position_mm,
    TubsVector3 direction);

// Conservative radial/axial safety lower bounds, clamped to zero.
// Inward safety uses max(radial clearance, axial clearance); outward safety
// uses min(distance to curved wall, distance to an end cap).
double tubs_safety_to_in_mm(
    const PortableSolidTubs& tubs,
    TubsVector3 position_mm);
double tubs_safety_to_out_mm(
    const PortableSolidTubs& tubs,
    TubsVector3 position_mm);

}  // namespace g4wgpu
