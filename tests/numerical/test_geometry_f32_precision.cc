#include "g4wgpu/BoxGeometry.hh"
#include "g4wgpu/OrbGeometry.hh"
#include "g4wgpu/TubsGeometry.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

// This is an input-quantization gate, not a GPU numerical-equivalence claim.
// Compare f64 CPU reference with the same CPU reference after rounding each
// input to f32, before WGSL kernels are introduced.
double f32(double value) {
  return static_cast<double>(static_cast<float>(value));
}

bool close(double actual, double reference, double absolute_tolerance,
           double relative_tolerance) {
  if (std::isinf(actual) || std::isinf(reference)) {
    return std::isinf(actual) && std::isinf(reference) &&
           std::signbit(actual) == std::signbit(reference);
  }
  return std::isfinite(actual) && std::isfinite(reference) &&
         std::fabs(actual - reference) <=
             absolute_tolerance +
                 relative_tolerance *
                     std::max(std::fabs(actual), std::fabs(reference));
}

bool check(const char* shape, const char* operation,
           double reference, double quantized, unsigned case_index) {
  // 1e-4 mm absolute + 1e-5 relative is intentionally looser than f32
  // input rounding, but not a guarantee about WGSL transcendental accuracy.
  if (close(reference, quantized, 1.0e-4, 1.0e-5)) {
    return true;
  }
  std::cerr << shape << " " << operation << " case " << case_index
            << " f64=" << reference << " f32-input=" << quantized << '\\n';
  return false;
}

}  // namespace

int main() {
  const g4wgpu::AxisAlignedBox box{{10.0, 12.0, 20.0}};
  const g4wgpu::PortableOrb orb{10.0};
  const g4wgpu::PortableSolidTubs tubs{10.0, 20.0};

  // Non-tangent, non-edge rays. Boundary and tangent tolerances need
  // dedicated WGSL-vs-Geant4 tests, not an input-quantization proxy.
  struct Ray {
    double px, py, pz;
    double dx, dy, dz;
  };
  const std::array<Ray, 7> external{{
      {15.125, 0.0, 0.0, -1.0, 0.0, 0.0},
      {0.0, 16.125, 0.0, 0.0, -1.0, 0.0},
      {0.0, 0.0, 25.125, 0.0, 0.0, -1.0},
      {15.125, 0.0, 25.125, -1.0, 0.0, -1.0},
      {-15.125, 0.0, 0.0, 1.0, 0.0, 0.0},
      {0.0, 0.0, -25.125, 0.0, 0.0, 1.0},
      {15.125, 0.0, 0.0, 1.0, 0.0, 0.0},
  }};
  const std::array<Ray, 5> internal{{
      {0.0, 0.0, 0.0, 1.0, 0.0, 0.0},
      {0.0, 0.0, 0.0, 0.0, 0.0, 1.0},
      {3.125, 4.125, 0.0, 1.0, 0.0, 0.0},
      {0.0, 0.0, 5.125, 0.0, 0.0, 1.0},
      {2.125, 1.125, 4.125, 1.0, 0.0, 1.0},
  }};

  for (unsigned i = 0; i < external.size(); ++i) {
    const auto& r = external[i];
    const g4wgpu::BoxVector3 bp{r.px, r.py, r.pz};
    const g4wgpu::BoxVector3 bd{r.dx, r.dy, r.dz};
    const g4wgpu::BoxVector3 bq{f32(r.px), f32(r.py), f32(r.pz)};
    const g4wgpu::BoxVector3 bqd{f32(r.dx), f32(r.dy), f32(r.dz)};
    const g4wgpu::OrbVector3 op{r.px, r.py, r.pz};
    const g4wgpu::OrbVector3 od{r.dx, r.dy, r.dz};
    const g4wgpu::OrbVector3 oq{f32(r.px), f32(r.py), f32(r.pz)};
    const g4wgpu::OrbVector3 oqd{f32(r.dx), f32(r.dy), f32(r.dz)};
    const g4wgpu::TubsVector3 tp{r.px, r.py, r.pz};
    const g4wgpu::TubsVector3 td{r.dx, r.dy, r.dz};
    const g4wgpu::TubsVector3 tq{f32(r.px), f32(r.py), f32(r.pz)};
    const g4wgpu::TubsVector3 tqd{f32(r.dx), f32(r.dy), f32(r.dz)};
    if (!check("box", "in", g4wgpu::box_distance_to_in_mm(box, bp, bd),
               g4wgpu::box_distance_to_in_mm(box, bq, bqd), i) ||
        !check("orb", "in", g4wgpu::orb_distance_to_in_mm(orb, op, od),
               g4wgpu::orb_distance_to_in_mm(orb, oq, oqd), i) ||
        !check("tubs", "in", g4wgpu::tubs_distance_to_in_mm(tubs, tp, td),
               g4wgpu::tubs_distance_to_in_mm(tubs, tq, tqd), i) ||
        !check("box", "safety-in", g4wgpu::box_safety_to_in_mm(box, bp),
               g4wgpu::box_safety_to_in_mm(box, bq), i) ||
        !check("orb", "safety-in", g4wgpu::orb_safety_to_in_mm(orb, op),
               g4wgpu::orb_safety_to_in_mm(orb, oq), i) ||
        !check("tubs", "safety-in", g4wgpu::tubs_safety_to_in_mm(tubs, tp),
               g4wgpu::tubs_safety_to_in_mm(tubs, tq), i)) {
      return EXIT_FAILURE;
    }
  }

  for (unsigned i = 0; i < internal.size(); ++i) {
    const auto& r = internal[i];
    const g4wgpu::BoxVector3 bp{r.px, r.py, r.pz};
    const g4wgpu::BoxVector3 bd{r.dx, r.dy, r.dz};
    const g4wgpu::BoxVector3 bq{f32(r.px), f32(r.py), f32(r.pz)};
    const g4wgpu::BoxVector3 bqd{f32(r.dx), f32(r.dy), f32(r.dz)};
    const g4wgpu::OrbVector3 op{r.px, r.py, r.pz};
    const g4wgpu::OrbVector3 od{r.dx, r.dy, r.dz};
    const g4wgpu::OrbVector3 oq{f32(r.px), f32(r.py), f32(r.pz)};
    const g4wgpu::OrbVector3 oqd{f32(r.dx), f32(r.dy), f32(r.dz)};
    const g4wgpu::TubsVector3 tp{r.px, r.py, r.pz};
    const g4wgpu::TubsVector3 td{r.dx, r.dy, r.dz};
    const g4wgpu::TubsVector3 tq{f32(r.px), f32(r.py), f32(r.pz)};
    const g4wgpu::TubsVector3 tqd{f32(r.dx), f32(r.dy), f32(r.dz)};
    if (!check("box", "out", g4wgpu::box_distance_to_out_mm(box, bp, bd),
               g4wgpu::box_distance_to_out_mm(box, bq, bqd), i) ||
        !check("orb", "out", g4wgpu::orb_distance_to_out_mm(orb, op, od),
               g4wgpu::orb_distance_to_out_mm(orb, oq, oqd), i) ||
        !check("tubs", "out", g4wgpu::tubs_distance_to_out_mm(tubs, tp, td),
               g4wgpu::tubs_distance_to_out_mm(tubs, tq, tqd), i) ||
        !check("box", "safety-out", g4wgpu::box_safety_to_out_mm(box, bp),
               g4wgpu::box_safety_to_out_mm(box, bq), i) ||
        !check("orb", "safety-out", g4wgpu::orb_safety_to_out_mm(orb, op),
               g4wgpu::orb_safety_to_out_mm(orb, oq), i) ||
        !check("tubs", "safety-out", g4wgpu::tubs_safety_to_out_mm(tubs, tp),
               g4wgpu::tubs_safety_to_out_mm(tubs, tq), i)) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
