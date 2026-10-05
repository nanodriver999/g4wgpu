// This WGSL source is a library fragment. Concatenate
// shaders/rng/counter_rng.wgsl before this source.

struct RngState {
  stream_lo: u32,
  stream_hi: u32,
  counter_lo: u32,
  counter_hi: u32,
}

struct KleinNishinaSample {
  scattered_gamma_energy_mev: f32,
  recoil_electron_energy_mev: f32,
  cos_theta: f32,
  sin_theta: f32,
  phi: f32,
  iterations: u32,
  accepted: u32,
}

fn rng_increment(state: ptr<function, RngState>) {
  (*state).counter_lo = (*state).counter_lo + 1u;
  if ((*state).counter_lo == 0u) {
    (*state).counter_hi = (*state).counter_hi + 1u;
  }
}

fn rng_next_f32(state: ptr<function, RngState>) -> f32 {
  let value = counter_rng_uniform_f32(
    (*state).stream_lo,
    (*state).stream_hi,
    (*state).counter_lo,
    (*state).counter_hi,
    0u,
  );
  rng_increment(state);
  return value;
}

fn sample_klein_nishina_f32(
  incident_gamma_energy_mev: f32,
  state: ptr<function, RngState>,
  max_iterations: u32,
) -> KleinNishinaSample {
  let electron_mass_mev = 0.51099895;
  let two_pi = 6.283185307179586;

  let energy_over_mass =
    incident_gamma_energy_mev / electron_mass_mev;

  let epsilon_min =
    1.0 / (1.0 + 2.0 * energy_over_mass);
  let epsilon_min_sq = epsilon_min * epsilon_min;

  let alpha1 = -log(epsilon_min);
  let alpha2 =
    alpha1 + 0.5 * (1.0 - epsilon_min_sq);

  var result: KleinNishinaSample;
  result.scattered_gamma_energy_mev = 0.0;
  result.recoil_electron_energy_mev = 0.0;
  result.cos_theta = 1.0;
  result.sin_theta = 0.0;
  result.phi = 0.0;
  result.iterations = max_iterations;
  result.accepted = 0u;

  var iteration = 1u;
  loop {
    if (iteration > max_iterations) {
      break;
    }

    let r0 = rng_next_f32(state);
    let r1 = rng_next_f32(state);
    let r2 = rng_next_f32(state);

    var epsilon = 0.0;
    var epsilon_sq = 0.0;

    if (alpha1 > alpha2 * r0) {
      epsilon = exp(-alpha1 * r1);
      epsilon_sq = epsilon * epsilon;
    } else {
      epsilon_sq =
        epsilon_min_sq +
        (1.0 - epsilon_min_sq) * r1;
      epsilon = sqrt(epsilon_sq);
    }

    let one_minus_cos =
      (1.0 - epsilon) /
      (epsilon * energy_over_mass);

    var sin_theta_sq =
      one_minus_cos * (2.0 - one_minus_cos);

    let rejection =
      1.0 -
      epsilon * sin_theta_sq /
        (1.0 + epsilon_sq);

    if (rejection >= r2) {
      sin_theta_sq = max(0.0, sin_theta_sq);

      result.scattered_gamma_energy_mev =
        epsilon * incident_gamma_energy_mev;
      result.recoil_electron_energy_mev =
        incident_gamma_energy_mev -
        result.scattered_gamma_energy_mev;
      result.cos_theta = 1.0 - one_minus_cos;
      result.sin_theta = sqrt(sin_theta_sq);
      result.phi = two_pi * rng_next_f32(state);
      result.iterations = iteration;
      result.accepted = 1u;
      break;
    }

    iteration = iteration + 1u;
  }

  return result;
}
