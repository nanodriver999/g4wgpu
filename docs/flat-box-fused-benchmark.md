# Flat Box fused navigation wall-time experiment

Build with `-DG4WGPU_ENABLE_WEBGPU=ON -DG4WGPU_BUILD_BENCHMARKS=ON`
and the same pinned wgpu-native header/library configuration as the WebGPU
integration tests. Run `g4wgpu_flat_box_fused_benchmark` from the build tree.

The program prints CSV columns `batch,method,repeat,wall_ms,cpu_substitutions,dispatches`
for batches of 1, 32, 256 and 1024 rays, with one warm-up and three
measured repeats. The batch cycles through +X and -X daughter entries,
a world-only exit from the center, and a short world-only exit near the
+X boundary (the single-ray case contains only the +X daughter entry).
This mixed workload is intentionally deterministic and is not a representative
detector track distribution. It compares CPU f64 reference navigation with the **safe
validated fused GPU wrapper**, verifying kind, daughter ID and distance.
The measured GPU wall time includes host validation, pipeline/buffer setup,
upload, dispatch, readback and substitutions. CPU timing includes independent
per-ray reference calls. These are different end-to-end paths, not matched
kernel-only timings. Do not infer GPU kernel speedup from these figures.

The WebGPU CI artifact retains both `fused-box-benchmark.csv` and
`fused-box-environment.txt` for 14 days. The metadata captures the runner OS,
architecture, kernel, compiler, pinned wgpu-native revision, build mode and
Vulkan device/driver summary. The Vulkan probe is diagnostic and does not gate
correctness if device enumeration fails. For local hardware benchmarks, also
record the adapter, driver, OS, compiler, build mode, GPU device and wgpu-native
revision alongside CSV output. Software Vulkan CI can establish correctness,
not hardware speedup; timings vary with adapter and load. The harness is
opt-in and intentionally **not a CI timing threshold**. The fixed rays are
well-conditioned and expected to have zero CPU substitutions; the CSV contract
rejects substituted fused results so a passing benchmark cannot hide CPU
fallback as GPU correctness. This assertion is limited to this benchmark,
not a removal of the production CPU fallback. It neither changes
Geant4 tracking nor RNG, physics or CPU fallback. Future work may measure
persistent buffers and larger scenes, but no performance claim is made here.

## Pipeline reuse experiment (November 13, 2026)

The initial CI software-Vulkan baseline from [workflow 43405871332](https://github.com/nanodriver999/g4wgpu/actions/runs/43405871332) was recorded before retaining the fused compute pipeline. The table reports **median measured wall times** in milliseconds (3 repeats each, warmed up, release build); the adapter is software Vulkan, not a dedicated GPU:

| Rays | CPU f64 | Fused GPU, validated |
|---:|---:|---:|
| 1 | 0.001614 | 6.56646 |
| 32 | 0.010199 | 6.55306 |
| 256 | 0.071523 | 6.60351 |
| 1024 | 0.270002 | 7.11770 |

The persistent **shader module + compute pipeline** introduced in the next
increment is reused across nonempty fused batches on the same backend.
The per-call ray/geometry buffers, bind group, encoder, transfer and readback
are still recreated. Re-running the exact benchmark in CI records the new
warmed-up end-to-end cost without changing the CSV schema or setting
flaky numerical performance thresholds. Pipeline creation is counted and
asserted to occur only once per WebGPU backend lifetime. Compare separate
runs cautiously: shared CI load and software-renderer variance can dominate.
