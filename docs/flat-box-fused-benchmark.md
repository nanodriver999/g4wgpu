# Flat Box fused navigation wall-time experiment

Build with `-DG4WGPU_ENABLE_WEBGPU=ON -DG4WGPU_BUILD_BENCHMARKS=ON`
and the same pinned wgpu-native header/library configuration as the WebGPU
integration tests. Run `g4wgpu_flat_box_fused_benchmark` from the build tree.

The program prints CSV columns `batch,method,repeat,wall_ms,cpu_substitutions,dispatches`
for batches of 1, 32, 256 and 1024 rays, with one warm-up and three
measured repeats. It compares CPU f64 reference navigation with the **safe
validated fused GPU wrapper**, verifying kind, daughter ID and distance.
The measured GPU wall time includes host validation, pipeline/buffer setup,
upload, dispatch, readback and substitutions. CPU timing includes independent
per-ray reference calls. These are different end-to-end paths, not matched
kernel-only timings. Do not infer GPU kernel speedup from these figures.

Record the adapter, driver, OS, compiler, build mode, GPU device and wgpu-native
revision alongside CSV output. Software Vulkan CI can establish correctness,
not hardware speedup; timings vary with adapter and load. The harness is
opt-in and intentionally **not a CI timing threshold**. It neither changes
Geant4 tracking nor RNG, physics or CPU fallback. Future work may measure
persistent buffers and larger scenes, but no performance claim is made here.
