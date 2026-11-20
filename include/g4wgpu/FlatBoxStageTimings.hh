#pragma once

namespace g4wgpu {

// Host wall-clock diagnostics for one validated fused navigation call.
// These are NOT GPU timestamp-query durations. They may include driver
// scheduling and software-Vulkan CPU execution. No timing value is a
// pass/fail threshold; validity/accuracy and dispatch counts are gated.
struct FlatBoxStageTimings {
  double cpu_reference_ms = 0.0;
  double eligibility_ms = 0.0;
  double backend_preflight_ms = 0.0;
  double staging_ms = 0.0;
  double pipeline_ms = 0.0;
  double resources_ms = 0.0;
  double upload_ms = 0.0;
  double submit_ms = 0.0;
  double readback_ms = 0.0;
  double decode_ms = 0.0;
  double reconcile_ms = 0.0;
  double gpu_call_ms = 0.0;
  double total_ms = 0.0;
};

}  // namespace g4wgpu
