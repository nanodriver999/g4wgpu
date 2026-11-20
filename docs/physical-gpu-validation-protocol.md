# Physical GPU validation protocol (future experiment)

This protocol defines the evidence needed **before** claiming a speedup on a
physical GPU. It does not change the implemented prototype or authorize
replacing Geant4's CPU navigator.

## Scope and baseline

- Record the exact `g4wgpu` commit, pinned Geant4 and wgpu-native revisions,
  GPU model, driver, graphics backend, OS, CPU, compiler and Release build flags.
- Run the existing opt-in `g4wgpu_flat_box_fused_benchmark` with
  `G4WGPU_ENABLE_WEBGPU=ON` and `G4WGPU_BUILD_BENCHMARKS=ON`, using the
  documented native library/header paths. Save its raw CSV and environment
  metadata, not only a derived speedup number.
- Run the same revision's CPU-only tests and, where supported, WebGPU and
  Geant4 integration tests. Record the test commands, counts and failures.
  Never substitute timing measurements for correctness tests.

## Comparison procedure

1. Keep the workload, ray ordering, scene, batch sizes (1, 32, 256, 1024),
   warm-up policy and measurement boundaries identical across repeated runs.
   The built-in benchmark currently uses one warm-up and three measured
   repetitions; collect multiple independent invocations to expose run-to-run
   variability rather than modifying its CSV schema.
2. Collect CPU f64 and **validated fused** wall times separately for each
   batch. Record medians and observed ranges, as well as
   `cpu_substitutions` and `dispatches`. The fixed benchmark workload is
   expected to have zero substitutions. Any nonzero substitution invalidates
   that sample as evidence of raw GPU correctness.
3. Preserve full end-to-end costs (CPU validation, transfers, dispatch and
   readback). Do not label the measured fused path as a kernel-only GPU
   benchmark. Note that the two measured paths have different host-side work.
4. Run a separate software-Vulkan reference only as a reproducibility and
   correctness check. Do not combine its timing samples with physical GPU
   measurements or infer a hardware speedup from GitHub Actions.
5. Report results per batch, including cases where GPU is slower. A speedup
   claim requires repeatable physical-adapter measurements, a clear definition
   of the compared work and unchanged correctness/fallback checks.

## Accuracy and ownership gates

- Keep Geant4 CPU navigation authoritative; the fused Box path remains
  opt-in. Preserve CPU fallback for unsupported geometry, out-of-range
  batches, f32-unrepresentable inputs and discrepancies.
- Run boundary/near-boundary differential tests and inspect result kind,
  daughter ID and distance tolerance, including substitutions. A zero
  substitution count for the fixed benchmark is **not** proof of accuracy
  for arbitrary Geant4 scenes.
- Preserve deterministic seed/counter mapping and existing statistical
  physics tests. This geometry timing experiment does not validate gamma
  physics, RNG quality or full Geant4 transport.
- Do not add a CI timing threshold based on shared runners. A hardware
  performance conclusion remains **unverified** until the evidence above
  is collected on a real GPU.

See [fused benchmark](flat-box-fused-benchmark.md) and
[fused Box limitations](flat-box-fused-webgpu.md).


## Software Vulkan GitHub Actions 검증의 경계

GitHub Actions에서 `vulkaninfo` 장치 목록만 기록하는 것으로는 실제
`WebGpuBackend`가 선택한 adapter가 무엇인지 증명할 수 없다.
따라서 선택된 adapter 정보를 `wgpuAdapterGetInfo`로 직접 확인하고,
`WGPU_BACKEND=vulkan` 및 Mesa lavapipe ICD를 지정한다.
실제로 선택된 Vulkan CPU adapter를 출력·검증하며, 간단한 WGSL 실행까지
성공해야 `webgpu-compile-link` Job을 통과한다.
원시 adapter 정보와 ICD 요약은 CI artifact에 남긴다.

이 검증은 **소프트웨어 WGSL 계산이 실제 실행되었다는 증거**이지
물리 GPU 가속, GPU 메모리 처리량 또는 제조사별 호환성의 증거는 아니다.
