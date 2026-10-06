#include "g4wgpu/CpuBackend.hh"

#include <stdexcept>

#include "g4wgpu/KleinNishina.hh"

namespace g4wgpu {

std::string_view CpuBackend::name() const noexcept {
  return "cpu";
}

void CpuBackend::axpy(const double a, const std::vector<double>& x,
                      std::vector<double>& y) {
  if (x.size() != y.size()) {
    throw std::invalid_argument("axpy requires x and y to have equal lengths");
  }

  for (std::size_t i = 0; i < x.size(); ++i) {
    y[i] = a * x[i] + y[i];
  }
}

void CpuBackend::advance_positions(TrackBatch& tracks, const double dt) {
  tracks.validate();

  for (std::size_t i = 0; i < tracks.size(); ++i) {
    tracks.position_x[i] += tracks.direction_x[i] * dt;
    tracks.position_y[i] += tracks.direction_y[i] * dt;
    tracks.position_z[i] += tracks.direction_z[i] * dt;
  }
}

std::vector<KleinNishinaSample> CpuBackend::sample_klein_nishina_batch(
    const std::vector<double>& incident_gamma_energy_mev,
    std::vector<RngAddress>& rng,
    const std::uint32_t max_iterations) {
  if (incident_gamma_energy_mev.size() != rng.size()) {
    throw std::invalid_argument(
        "Klein-Nishina batch energies and RNG state sizes differ");
  }

  std::vector<KleinNishinaSample> result;
  result.reserve(incident_gamma_energy_mev.size());

  for (std::size_t i = 0; i < incident_gamma_energy_mev.size(); ++i) {
    result.push_back(sample_klein_nishina(
        incident_gamma_energy_mev[i],
        rng[i],
        max_iterations));
  }

  return result;
}

}  // namespace g4wgpu
