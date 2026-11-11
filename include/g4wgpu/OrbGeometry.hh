#pragma once

namespace g4wgpu {

// Full sphere centered at the origin in local coordinates, using millimeters.
// POD fields are independent of Geant4 object pointers and WebGPU types.
struct OrbVector3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct PortableOrb {
  double radius_mm = 0.0;
};

// Physical ray distances (mm); non-unit directions are normalized internally.
// External rays that miss, point away, or only graze return +infinity.
// A ray starting on/inside the sphere has zero distance to enter.
double orb_distance_to_in_mm(
    const PortableOrb& orb, OrbVector3 position_mm,
    OrbVector3 direction);

// The ray origin must be on or within the sphere.
double orb_distance_to_out_mm(
    const PortableOrb& orb, OrbVector3 position_mm,
    OrbVector3 direction);

// Shortest (radial) distance to the nearest surface, clamped at zero.
double orb_safety_to_in_mm(
    const PortableOrb& orb, OrbVector3 position_mm);
double orb_safety_to_out_mm(
    const PortableOrb& orb, OrbVector3 position_mm);

}  // namespace g4wgpu
