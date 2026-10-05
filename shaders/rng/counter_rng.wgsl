fn rotl32(x: u32, r: u32) -> u32 {
  return (x << r) | (x >> (32u - r));
}

fn mix32(value: u32) -> u32 {
  var x = value;
  x = x ^ (x >> 16u);
  x = x * 0x7feb352du;
  x = x ^ (x >> 15u);
  x = x * 0x846ca68bu;
  x = x ^ (x >> 16u);
  return x;
}

fn counter_rng_u32(
  stream_lo: u32,
  stream_hi: u32,
  counter_lo: u32,
  counter_hi: u32,
  lane: u32,
) -> u32 {
  var x = counter_lo;
  x = x ^ rotl32(counter_hi, 13u);
  x = x ^ rotl32(stream_lo, 7u);
  x = x ^ rotl32(stream_hi, 19u);
  x = x ^ (0x9e3779b9u * (lane + 1u));

  x = mix32(x);
  x = x ^ mix32(stream_lo + 0x85ebca6bu);
  x = mix32(x ^ stream_hi);
  return x;
}

fn counter_rng_uniform_f32(
  stream_lo: u32,
  stream_hi: u32,
  counter_lo: u32,
  counter_hi: u32,
  lane: u32,
) -> f32 {
  let bits = counter_rng_u32(
    stream_lo, stream_hi, counter_lo, counter_hi, lane
  ) >> 8u;
  return f32(bits) * (1.0 / 16777216.0);
}
