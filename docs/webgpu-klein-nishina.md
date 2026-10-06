# WebGPU Klein-Nishina batch execution

This stage executes the first physics sampler through the native WebGPU
backend rather than only compiling WGSL source.

## Contract

The CPU and WebGPU implementations receive the same:

- incident gamma energy
- RNG stream ID
- RNG counter
- maximum rejection-loop count

The CPU implementation uses double precision for the numerical model.
The portable WGSL implementation uses f32.

The GPU returns:

- scattered gamma energy
- recoil electron energy
- cos(theta)
- sin(theta)
- phi
- rejection-loop iteration count
- accepted flag
- updated RNG counter

## Validation

The runtime integration test executes a batch on the Mesa Vulkan software
adapter in GitHub Actions and checks:

1. all CPU and GPU samples satisfy physical invariants,
2. CPU/GPU mean scattered-energy fraction agrees within tolerance,
3. CPU/GPU mean cos(theta) agrees within tolerance,
4. at least 98% of samples consume the same RNG counter path.

This is deliberately a distribution-level comparison because f32 and double
transcendental calculations can make a small number of rejection decisions
differ even when the initial random stream is identical.

## What this does not prove

Software Vulkan validates the WebGPU/WGSL execution path but is not a hardware
performance benchmark.

Performance benchmarking on NVIDIA/AMD/Intel/Apple hardware is a separate
milestone.
