#include "g4wgpu/ComptonKinematics.hh"

#include <cmath>
#include <stdexcept>

namespace g4wgpu {
namespace {

constexpr double kElectronMassMeV = 0.51099895;

double dot(const Vector3& a, const Vector3& b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 cross(const Vector3& a, const Vector3& b) noexcept {
  return Vector3{
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x};
}

Vector3 scale(const Vector3& v, const double s) noexcept {
  return Vector3{v.x * s, v.y * s, v.z * s};
}

Vector3 add(const Vector3& a, const Vector3& b) noexcept {
  return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 subtract(const Vector3& a, const Vector3& b) noexcept {
  return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

double norm(const Vector3& v) noexcept {
  return std::sqrt(dot(v, v));
}

Vector3 normalized(const Vector3& v) {
  const double magnitude = norm(v);
  if (!(magnitude > 0.0) || !std::isfinite(magnitude)) {
    throw std::invalid_argument(
        "Compton incident direction must be finite and non-zero");
  }
  return scale(v, 1.0 / magnitude);
}

bool finite_vector(const Vector3& v) noexcept {
  return std::isfinite(v.x) &&
         std::isfinite(v.y) &&
         std::isfinite(v.z);
}

}  // namespace

ComptonFinalState make_compton_final_state(
    const double incident_gamma_energy_mev,
    const Vector3 incident_direction,
    const KleinNishinaSample& sample) {
  if (!(incident_gamma_energy_mev > 0.0) ||
      !std::isfinite(incident_gamma_energy_mev)) {
    throw std::invalid_argument(
        "Compton incident gamma energy must be finite and positive");
  }
  if (!sample.accepted) {
    throw std::invalid_argument(
        "Compton final state requires an accepted Klein-Nishina sample");
  }
  if (!(sample.scattered_gamma_energy_mev > 0.0) ||
      sample.scattered_gamma_energy_mev > incident_gamma_energy_mev ||
      !(sample.recoil_electron_energy_mev >= 0.0) ||
      !std::isfinite(sample.scattered_gamma_energy_mev) ||
      !std::isfinite(sample.recoil_electron_energy_mev) ||
      !std::isfinite(sample.cos_theta) ||
      !std::isfinite(sample.sin_theta) ||
      !std::isfinite(sample.phi)) {
    throw std::invalid_argument(
        "Compton sample contains invalid kinematics");
  }

  const double energy_residual =
      std::fabs(
          sample.scattered_gamma_energy_mev +
          sample.recoil_electron_energy_mev -
          incident_gamma_energy_mev);
  if (energy_residual >
      5.0e-5 * std::max(1.0, incident_gamma_energy_mev)) {
    throw std::invalid_argument(
        "Compton sample does not conserve energy");
  }

  const Vector3 w = normalized(incident_direction);
  const Vector3 reference =
      std::fabs(w.z) < 0.999
          ? Vector3{0.0, 0.0, 1.0}
          : Vector3{0.0, 1.0, 0.0};
  const Vector3 u = normalized(cross(reference, w));
  const Vector3 v = cross(w, u);

  const double cos_phi = std::cos(sample.phi);
  const double sin_phi = std::sin(sample.phi);

  Vector3 gamma_direction = add(
      scale(w, sample.cos_theta),
      add(
          scale(u, sample.sin_theta * cos_phi),
          scale(v, sample.sin_theta * sin_phi)));
  gamma_direction = normalized(gamma_direction);

  const Vector3 incident_momentum =
      scale(w, incident_gamma_energy_mev);
  const Vector3 scattered_gamma_momentum =
      scale(
          gamma_direction,
          sample.scattered_gamma_energy_mev);
  const Vector3 recoil_momentum =
      subtract(
          incident_momentum,
          scattered_gamma_momentum);

  const double recoil_momentum_magnitude =
      norm(recoil_momentum);

  Vector3 recoil_direction = w;
  if (recoil_momentum_magnitude > 1.0e-15) {
    recoil_direction =
        scale(recoil_momentum, 1.0 / recoil_momentum_magnitude);
  }

  if (!finite_vector(gamma_direction) ||
      !finite_vector(recoil_direction)) {
    throw std::runtime_error(
        "Compton final-state direction construction failed");
  }

  // The direction comes from exact momentum subtraction. Check that the
  // magnitude is compatible with the sampled recoil kinetic energy while
  // allowing f32 WebGPU samples.
  const double expected_recoil_momentum =
      std::sqrt(
          sample.recoil_electron_energy_mev *
          (sample.recoil_electron_energy_mev +
           2.0 * kElectronMassMeV));
  if (std::fabs(
          recoil_momentum_magnitude -
          expected_recoil_momentum) >
      2.0e-4 * std::max(1.0, expected_recoil_momentum)) {
    throw std::invalid_argument(
        "Compton sample is inconsistent with momentum conservation");
  }

  return ComptonFinalState{
      sample.scattered_gamma_energy_mev,
      sample.recoil_electron_energy_mev,
      gamma_direction,
      recoil_direction};
}

}  // namespace g4wgpu
