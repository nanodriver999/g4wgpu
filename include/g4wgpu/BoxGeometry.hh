#pragma once

namespace g4wgpu {

// Axis-aligned box centered at the origin. All lengths are in millimeters.
// POD values are deliberately independent of Geant4 and WebGPU types.
struct BoxVector3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct AxisAlignedBox {
  BoxVector3 half_lengths_mm;
};

// A local-coordinate ray. Used as input for the optional batched WebGPU
// Box-entry experiment; it does not change Geant4 navigation ownership.
struct BoxRay {
  BoxVector3 position_mm;
  BoxVector3 direction;
};

// Return +infinity if an external ray misses the box. A starting point on
// or within the box has zero entry distance. Ray directions are normalized
// internally; the output is a physical distance in mm.
double box_distance_to_in_mm(
    const AxisAlignedBox& box, BoxVector3 position_mm,
    BoxVector3 direction);

// The starting point must be on or inside the box.
double box_distance_to_out_mm(
    const AxisAlignedBox& box, BoxVector3 position_mm,
    BoxVector3 direction);

// Conservative, axis-aligned lower bounds on the distance to a box surface.
// They match the safety semantics required for geometry navigation.
double box_safety_to_in_mm(
    const AxisAlignedBox& box, BoxVector3 position_mm);
double box_safety_to_out_mm(
    const AxisAlignedBox& box, BoxVector3 position_mm);

}  // namespace g4wgpu
