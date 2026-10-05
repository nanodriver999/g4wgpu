# wgpu-native integration

PR 3 introduces the first native WebGPU runtime path.

Reference upstream state used during implementation:

- `gfx-rs/wgpu-native@250eeb67f8d091bf7dbc4fc253293d10900cff64`
- bundled `webgpu-headers` submodule:
  `673658bc2bd70ec39fc55ebe6bb0173cf6d0a603`

## Dependency boundary

The public g4wgpu C++ headers do not expose WebGPU C handles.
Only `src/webgpu/WebGpuBackend.cc` includes `wgpu.h`.

This is intentional because wgpu-native does not yet perfectly match the
stable multi-vendor `webgpu.h` API.

The current implementation therefore treats wgpu-native as an adapter-specific
runtime behind `ComputeBackend`.

## Precision boundary

The core `TrackBatch` and CPU reference backend use `double`.

The portable WGSL smoke kernel uses `f32`.

`WebGpuBackend` performs explicit staging conversion:

```text
C++ double
   ↓
explicit float staging
   ↓
WGSL f32
   ↓
explicit double conversion
```

This prevents accidental loss of precision in the CPU reference path and makes
the WebGPU precision boundary visible in code.

## Build

WebGPU is disabled by default:

```bash
cmake -S . -B build
```

To enable the backend, point CMake at a wgpu-native installation:

```bash
cmake -S . -B build-wgpu \
  -DG4WGPU_ENABLE_WEBGPU=ON \
  -DG4WGPU_WGPU_NATIVE_INCLUDE_DIR=/path/to/wgpu-native/ffi \
  -DG4WGPU_WEBGPU_HEADER_DIR=/path/to/wgpu-native/ffi/webgpu-headers \
  -DG4WGPU_WGPU_NATIVE_LIBRARY=/path/to/libwgpu_native.so
```

The two header paths are explicit because the upstream wgpu-native source
tree keeps them separately:

- `G4WGPU_WGPU_NATIVE_INCLUDE_DIR`: contains `wgpu.h`
- `G4WGPU_WEBGPU_HEADER_DIR`: contains the matching `webgpu.h`

A packaged installation may place them together; in that case both CMake
variables may point to the same directory.

The integration test is opt-in because CI machines may not have a usable GPU:

```bash
-DG4WGPU_RUN_GPU_TESTS=ON
```

## Current limitations

The PR 3 implementation intentionally prioritizes a small verified runtime path.

- AXPY buffers are allocated for each call.
- Track positions use three AXPY dispatches.
- GPU-resident persistent batches are not implemented yet.
- Only portable WGSL `f32` is used.
- wgpu-native-specific polling is isolated in the backend implementation.

Those limitations are addressed by later batch/runtime optimization work.
