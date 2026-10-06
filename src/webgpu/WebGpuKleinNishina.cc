#include "g4wgpu/WebGpuBackend.hh"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "wgpu.h"

namespace g4wgpu {
namespace {

WGPUStringView kn_string_view(const char* value) {
  WGPUStringView view = WGPU_STRING_VIEW_INIT;
  view.data = value;
  view.length = WGPU_STRLEN;
  return view;
}

std::string kn_copy_message(const WGPUStringView message) {
  if (message.data == nullptr) {
    return {};
  }
  if (message.length == WGPU_STRLEN) {
    return std::string(message.data);
  }
  return std::string(message.data, message.length);
}

struct KnAdapterState {
  bool done = false;
  WGPUAdapter adapter = nullptr;
  std::string error;
};

void kn_on_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                   WGPUStringView message, void* userdata1,
                   void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<KnAdapterState*>(userdata1);
  if (status == WGPURequestAdapterStatus_Success) {
    state->adapter = adapter;
  } else {
    state->error = kn_copy_message(message);
  }
  state->done = true;
}

struct KnDeviceState {
  bool done = false;
  WGPUDevice device = nullptr;
  std::string error;
};

void kn_on_device(WGPURequestDeviceStatus status, WGPUDevice device,
                  WGPUStringView message, void* userdata1,
                  void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<KnDeviceState*>(userdata1);
  if (status == WGPURequestDeviceStatus_Success) {
    state->device = device;
  } else {
    state->error = kn_copy_message(message);
  }
  state->done = true;
}

struct KnMapState {
  bool done = false;
  bool success = false;
  std::string error;
};

void kn_on_map(WGPUMapAsyncStatus status, WGPUStringView message,
               void* userdata1, void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<KnMapState*>(userdata1);
  state->success = status == WGPUMapAsyncStatus_Success;
  if (!state->success) {
    state->error = kn_copy_message(message);
  }
  state->done = true;
}

template <typename State>
void kn_wait(WGPUInstance instance, const State& state,
             const char* operation) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(10);

  while (!state.done) {
    wgpuInstanceProcessEvents(instance);
    if (std::chrono::steady_clock::now() >= deadline) {
      throw std::runtime_error(
          std::string(operation) + " timed out");
    }
    std::this_thread::yield();
  }
}

WGPUBuffer kn_create_buffer(WGPUDevice device, const char* label,
                            std::uint64_t size,
                            WGPUBufferUsage usage) {
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.label = kn_string_view(label);
  descriptor.size = size;
  descriptor.usage = usage;
  descriptor.mappedAtCreation = WGPU_FALSE;
  return wgpuDeviceCreateBuffer(device, &descriptor);
}

struct KnRuntime {
  WGPUInstance instance = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;

  KnRuntime() {
    instance = wgpuCreateInstance(nullptr);
    if (instance == nullptr) {
      throw std::runtime_error(
          "Klein-Nishina WebGPU instance creation failed");
    }

    try {
      KnAdapterState adapter_state;
      WGPURequestAdapterCallbackInfo adapter_callback =
          WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
      adapter_callback.mode = WGPUCallbackMode_AllowProcessEvents;
      adapter_callback.callback = kn_on_adapter;
      adapter_callback.userdata1 = &adapter_state;
      wgpuInstanceRequestAdapter(
          instance, nullptr, adapter_callback);
      kn_wait(instance, adapter_state, "request adapter");

      if (adapter_state.adapter == nullptr) {
        throw std::runtime_error(
            "Klein-Nishina adapter request failed: " +
            adapter_state.error);
      }
      adapter = adapter_state.adapter;

      KnDeviceState device_state;
      WGPURequestDeviceCallbackInfo device_callback =
          WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
      device_callback.mode = WGPUCallbackMode_AllowProcessEvents;
      device_callback.callback = kn_on_device;
      device_callback.userdata1 = &device_state;
      wgpuAdapterRequestDevice(
          adapter, nullptr, device_callback);
      kn_wait(instance, device_state, "request device");

      if (device_state.device == nullptr) {
        throw std::runtime_error(
            "Klein-Nishina device request failed: " +
            device_state.error);
      }
      device = device_state.device;
      queue = wgpuDeviceGetQueue(device);
      if (queue == nullptr) {
        throw std::runtime_error(
            "Klein-Nishina queue acquisition failed");
      }
    } catch (...) {
      release();
      throw;
    }
  }

  ~KnRuntime() {
    release();
  }

  void release() noexcept {
    if (queue != nullptr) {
      wgpuQueueRelease(queue);
      queue = nullptr;
    }
    if (device != nullptr) {
      wgpuDeviceRelease(device);
      device = nullptr;
    }
    if (adapter != nullptr) {
      wgpuAdapterRelease(adapter);
      adapter = nullptr;
    }
    if (instance != nullptr) {
      wgpuInstanceRelease(instance);
      instance = nullptr;
    }
  }
};

struct KnResources {
  WGPUShaderModule shader = nullptr;
  WGPUComputePipeline pipeline = nullptr;
  WGPUBindGroupLayout layout = nullptr;
  WGPUBindGroup bind_group = nullptr;
  WGPUBuffer input = nullptr;
  WGPUBuffer output = nullptr;
  WGPUBuffer params = nullptr;
  WGPUBuffer readback = nullptr;
  WGPUCommandEncoder encoder = nullptr;
  WGPUCommandBuffer command = nullptr;

  ~KnResources() {
    if (command != nullptr) wgpuCommandBufferRelease(command);
    if (encoder != nullptr) wgpuCommandEncoderRelease(encoder);
    if (bind_group != nullptr) wgpuBindGroupRelease(bind_group);
    if (layout != nullptr) wgpuBindGroupLayoutRelease(layout);
    if (readback != nullptr) wgpuBufferRelease(readback);
    if (params != nullptr) wgpuBufferRelease(params);
    if (output != nullptr) wgpuBufferRelease(output);
    if (input != nullptr) wgpuBufferRelease(input);
    if (pipeline != nullptr) wgpuComputePipelineRelease(pipeline);
    if (shader != nullptr) wgpuShaderModuleRelease(shader);
  }
};

struct alignas(4) KnGpuInput {
  float incident_energy_mev;
  std::uint32_t stream_lo;
  std::uint32_t stream_hi;
  std::uint32_t counter_lo;
  std::uint32_t counter_hi;
  std::uint32_t max_iterations;
  std::uint32_t pad0;
  std::uint32_t pad1;
};

struct alignas(4) KnGpuOutput {
  float scattered_gamma_energy_mev;
  float recoil_electron_energy_mev;
  float cos_theta;
  float sin_theta;
  float phi;
  std::uint32_t iterations;
  std::uint32_t accepted;
  std::uint32_t counter_lo;
  std::uint32_t counter_hi;
  std::uint32_t pad0;
  std::uint32_t pad1;
  std::uint32_t pad2;
};

struct KnGpuParams {
  std::uint32_t count;
  std::uint32_t pad0;
  std::uint32_t pad1;
  std::uint32_t pad2;
};

static_assert(sizeof(KnGpuInput) == 32);
static_assert(offsetof(KnGpuInput, incident_energy_mev) == 0);
static_assert(offsetof(KnGpuInput, stream_lo) == 4);
static_assert(offsetof(KnGpuInput, stream_hi) == 8);
static_assert(offsetof(KnGpuInput, counter_lo) == 12);
static_assert(offsetof(KnGpuInput, counter_hi) == 16);
static_assert(offsetof(KnGpuInput, max_iterations) == 20);

static_assert(sizeof(KnGpuOutput) == 48);
static_assert(offsetof(KnGpuOutput, scattered_gamma_energy_mev) == 0);
static_assert(offsetof(KnGpuOutput, recoil_electron_energy_mev) == 4);
static_assert(offsetof(KnGpuOutput, cos_theta) == 8);
static_assert(offsetof(KnGpuOutput, sin_theta) == 12);
static_assert(offsetof(KnGpuOutput, phi) == 16);
static_assert(offsetof(KnGpuOutput, iterations) == 20);
static_assert(offsetof(KnGpuOutput, accepted) == 24);
static_assert(offsetof(KnGpuOutput, counter_lo) == 28);
static_assert(offsetof(KnGpuOutput, counter_hi) == 32);

static_assert(sizeof(KnGpuParams) == 16);
static_assert(offsetof(KnGpuParams, count) == 0);

constexpr const char* kKleinNishinaShader = R"WGSL(
struct KnInput {
  incident_energy_mev: f32,
  stream_lo: u32,
  stream_hi: u32,
  counter_lo: u32,
  counter_hi: u32,
  max_iterations: u32,
  pad0: u32,
  pad1: u32,
}

struct KnOutput {
  scattered_gamma_energy_mev: f32,
  recoil_electron_energy_mev: f32,
  cos_theta: f32,
  sin_theta: f32,
  phi: f32,
  iterations: u32,
  accepted: u32,
  counter_lo: u32,
  counter_hi: u32,
  pad0: u32,
  pad1: u32,
  pad2: u32,
}

struct Params {
  count: u32,
  pad0: u32,
  pad1: u32,
  pad2: u32,
}

struct RngState {
  stream_lo: u32,
  stream_hi: u32,
  counter_lo: u32,
  counter_hi: u32,
}

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

fn rng_u32(state: RngState) -> u32 {
  var x = state.counter_lo;
  x = x ^ rotl32(state.counter_hi, 13u);
  x = x ^ rotl32(state.stream_lo, 7u);
  x = x ^ rotl32(state.stream_hi, 19u);
  x = x ^ 0x9e3779b9u;

  x = mix32(x);
  x = x ^ mix32(state.stream_lo + 0x85ebca6bu);
  x = mix32(x ^ state.stream_hi);
  return x;
}

fn rng_increment(state: ptr<function, RngState>) {
  (*state).counter_lo = (*state).counter_lo + 1u;
  if ((*state).counter_lo == 0u) {
    (*state).counter_hi = (*state).counter_hi + 1u;
  }
}

fn rng_next(state: ptr<function, RngState>) -> f32 {
  let bits = rng_u32(*state) >> 8u;
  let result = f32(bits) * (1.0 / 16777216.0);
  rng_increment(state);
  return result;
}

@group(0) @binding(0)
var<storage, read> inputs: array<KnInput>;

@group(0) @binding(1)
var<storage, read_write> outputs: array<KnOutput>;

@group(0) @binding(2)
var<uniform> params: Params;

@compute @workgroup_size(128)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let index = id.x;
  if (index >= params.count) {
    return;
  }

  let input = inputs[index];
  var state: RngState;
  state.stream_lo = input.stream_lo;
  state.stream_hi = input.stream_hi;
  state.counter_lo = input.counter_lo;
  state.counter_hi = input.counter_hi;

  var out: KnOutput;
  out.scattered_gamma_energy_mev = 0.0;
  out.recoil_electron_energy_mev = 0.0;
  out.cos_theta = 1.0;
  out.sin_theta = 0.0;
  out.phi = 0.0;
  out.iterations = input.max_iterations;
  out.accepted = 0u;
  out.counter_lo = state.counter_lo;
  out.counter_hi = state.counter_hi;
  out.pad0 = 0u;
  out.pad1 = 0u;
  out.pad2 = 0u;

  if (input.incident_energy_mev <= 0.0 ||
      input.max_iterations == 0u) {
    outputs[index] = out;
    return;
  }

  let electron_mass_mev = 0.51099895;
  let two_pi = 6.283185307179586;
  let energy_over_mass =
      input.incident_energy_mev / electron_mass_mev;

  let epsilon_min =
      1.0 / (1.0 + 2.0 * energy_over_mass);
  let epsilon_min_sq =
      epsilon_min * epsilon_min;
  let alpha1 = -log(epsilon_min);
  let alpha2 =
      alpha1 + 0.5 * (1.0 - epsilon_min_sq);

  var iteration = 1u;
  loop {
    if (iteration > input.max_iterations) {
      break;
    }

    let r0 = rng_next(&state);
    let r1 = rng_next(&state);
    let r2 = rng_next(&state);

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
        one_minus_cos *
        (2.0 - one_minus_cos);

    let rejection =
        1.0 -
        epsilon * sin_theta_sq /
        (1.0 + epsilon_sq);

    if (rejection >= r2) {
      sin_theta_sq = max(0.0, sin_theta_sq);

      out.scattered_gamma_energy_mev =
          epsilon * input.incident_energy_mev;
      out.recoil_electron_energy_mev =
          input.incident_energy_mev -
          out.scattered_gamma_energy_mev;
      out.cos_theta = 1.0 - one_minus_cos;
      out.sin_theta = sqrt(sin_theta_sq);
      out.phi = two_pi * rng_next(&state);
      out.iterations = iteration;
      out.accepted = 1u;
      break;
    }

    iteration = iteration + 1u;
  }

  out.counter_lo = state.counter_lo;
  out.counter_hi = state.counter_hi;
  outputs[index] = out;
}
)WGSL";

}  // namespace

std::vector<KleinNishinaSample>
WebGpuBackend::sample_klein_nishina_batch(
    const std::vector<double>& incident_gamma_energy_mev,
    std::vector<RngAddress>& rng,
    const std::uint32_t max_iterations) {
  if (incident_gamma_energy_mev.size() != rng.size()) {
    throw std::invalid_argument(
        "Klein-Nishina batch energies and RNG state sizes differ");
  }
  if (max_iterations == 0u) {
    throw std::invalid_argument(
        "max_iterations must be greater than zero");
  }
  if (incident_gamma_energy_mev.empty()) {
    return {};
  }
  if (incident_gamma_energy_mev.size() >
      static_cast<std::size_t>(UINT32_MAX)) {
    throw std::overflow_error(
        "Klein-Nishina batch exceeds u32 dispatch range");
  }

  std::vector<KnGpuInput> gpu_input(
      incident_gamma_energy_mev.size());

  for (std::size_t i = 0;
       i < incident_gamma_energy_mev.size();
       ++i) {
    if (!(incident_gamma_energy_mev[i] > 0.0)) {
      throw std::invalid_argument(
          "incident gamma energy must be positive");
    }

    gpu_input[i] = KnGpuInput{
        static_cast<float>(incident_gamma_energy_mev[i]),
        rng[i].stream_lo,
        rng[i].stream_hi,
        rng[i].counter_lo,
        rng[i].counter_hi,
        max_iterations,
        0u,
        0u};
  }

  const std::uint64_t input_bytes =
      static_cast<std::uint64_t>(
          gpu_input.size() * sizeof(KnGpuInput));
  const std::uint64_t output_bytes =
      static_cast<std::uint64_t>(
          gpu_input.size() * sizeof(KnGpuOutput));

  KnRuntime runtime;
  KnResources resources;

  WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgsl.code = kn_string_view(kKleinNishinaShader);

  WGPUShaderModuleDescriptor shader_descriptor =
      WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  shader_descriptor.label =
      kn_string_view("g4wgpu_klein_nishina_shader");
  shader_descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&wgsl);

  resources.shader =
      wgpuDeviceCreateShaderModule(
          runtime.device, &shader_descriptor);
  if (resources.shader == nullptr) {
    throw std::runtime_error(
        "failed to create Klein-Nishina shader module");
  }

  WGPUComputeState compute = WGPU_COMPUTE_STATE_INIT;
  compute.module = resources.shader;
  compute.entryPoint = kn_string_view("main");

  WGPUComputePipelineDescriptor pipeline_descriptor =
      WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
  pipeline_descriptor.label =
      kn_string_view("g4wgpu_klein_nishina_pipeline");
  pipeline_descriptor.compute = compute;

  resources.pipeline =
      wgpuDeviceCreateComputePipeline(
          runtime.device, &pipeline_descriptor);
  if (resources.pipeline == nullptr) {
    throw std::runtime_error(
        "failed to create Klein-Nishina compute pipeline");
  }

  resources.input = kn_create_buffer(
      runtime.device,
      "g4wgpu_kn_input",
      input_bytes,
      WGPUBufferUsage_Storage |
          WGPUBufferUsage_CopyDst);
  resources.output = kn_create_buffer(
      runtime.device,
      "g4wgpu_kn_output",
      output_bytes,
      WGPUBufferUsage_Storage |
          WGPUBufferUsage_CopySrc);
  resources.params = kn_create_buffer(
      runtime.device,
      "g4wgpu_kn_params",
      sizeof(KnGpuParams),
      WGPUBufferUsage_Uniform |
          WGPUBufferUsage_CopyDst);
  resources.readback = kn_create_buffer(
      runtime.device,
      "g4wgpu_kn_readback",
      output_bytes,
      WGPUBufferUsage_MapRead |
          WGPUBufferUsage_CopyDst);

  if (resources.input == nullptr ||
      resources.output == nullptr ||
      resources.params == nullptr ||
      resources.readback == nullptr) {
    throw std::runtime_error(
        "failed to create Klein-Nishina buffers");
  }

  resources.layout =
      wgpuComputePipelineGetBindGroupLayout(
          resources.pipeline, 0);
  if (resources.layout == nullptr) {
    throw std::runtime_error(
        "failed to get Klein-Nishina bind group layout");
  }

  WGPUBindGroupEntry entries[3] = {
      WGPU_BIND_GROUP_ENTRY_INIT,
      WGPU_BIND_GROUP_ENTRY_INIT,
      WGPU_BIND_GROUP_ENTRY_INIT};

  entries[0].binding = 0;
  entries[0].buffer = resources.input;
  entries[0].size = input_bytes;

  entries[1].binding = 1;
  entries[1].buffer = resources.output;
  entries[1].size = output_bytes;

  entries[2].binding = 2;
  entries[2].buffer = resources.params;
  entries[2].size = sizeof(KnGpuParams);

  WGPUBindGroupDescriptor bind_group_descriptor =
      WGPU_BIND_GROUP_DESCRIPTOR_INIT;
  bind_group_descriptor.label =
      kn_string_view("g4wgpu_kn_bind_group");
  bind_group_descriptor.layout = resources.layout;
  bind_group_descriptor.entryCount = 3;
  bind_group_descriptor.entries = entries;

  resources.bind_group =
      wgpuDeviceCreateBindGroup(
          runtime.device, &bind_group_descriptor);
  if (resources.bind_group == nullptr) {
    throw std::runtime_error(
        "failed to create Klein-Nishina bind group");
  }

  const KnGpuParams params{
      static_cast<std::uint32_t>(gpu_input.size()),
      0u, 0u, 0u};

  wgpuQueueWriteBuffer(
      runtime.queue,
      resources.input,
      0,
      gpu_input.data(),
      input_bytes);
  wgpuQueueWriteBuffer(
      runtime.queue,
      resources.params,
      0,
      &params,
      sizeof(params));

  WGPUCommandEncoderDescriptor encoder_descriptor =
      WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
  encoder_descriptor.label =
      kn_string_view("g4wgpu_kn_encoder");

  resources.encoder =
      wgpuDeviceCreateCommandEncoder(
          runtime.device, &encoder_descriptor);
  if (resources.encoder == nullptr) {
    throw std::runtime_error(
        "failed to create Klein-Nishina command encoder");
  }

  WGPUComputePassDescriptor pass_descriptor =
      WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
  pass_descriptor.label =
      kn_string_view("g4wgpu_kn_pass");

  WGPUComputePassEncoder pass =
      wgpuCommandEncoderBeginComputePass(
          resources.encoder, &pass_descriptor);
  if (pass == nullptr) {
    throw std::runtime_error(
        "failed to begin Klein-Nishina compute pass");
  }

  wgpuComputePassEncoderSetPipeline(
      pass, resources.pipeline);
  wgpuComputePassEncoderSetBindGroup(
      pass, 0, resources.bind_group, 0, nullptr);

  const std::uint32_t workgroups =
      (static_cast<std::uint32_t>(
           gpu_input.size()) +
       127u) /
      128u;

  wgpuComputePassEncoderDispatchWorkgroups(
      pass, workgroups, 1, 1);
  wgpuComputePassEncoderEnd(pass);
  wgpuComputePassEncoderRelease(pass);

  wgpuCommandEncoderCopyBufferToBuffer(
      resources.encoder,
      resources.output,
      0,
      resources.readback,
      0,
      output_bytes);

  WGPUCommandBufferDescriptor command_descriptor =
      WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
  command_descriptor.label =
      kn_string_view("g4wgpu_kn_command");

  resources.command =
      wgpuCommandEncoderFinish(
          resources.encoder, &command_descriptor);
  if (resources.command == nullptr) {
    throw std::runtime_error(
        "failed to finish Klein-Nishina command buffer");
  }

  wgpuQueueSubmit(
      runtime.queue, 1, &resources.command);

  KnMapState map_state;
  WGPUBufferMapCallbackInfo map_callback =
      WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
  map_callback.mode = WGPUCallbackMode_AllowProcessEvents;
  map_callback.callback = kn_on_map;
  map_callback.userdata1 = &map_state;

  wgpuBufferMapAsync(
      resources.readback,
      WGPUMapMode_Read,
      0,
      output_bytes,
      map_callback);
  wgpuDevicePoll(runtime.device, WGPU_TRUE, nullptr);
  kn_wait(
      runtime.instance,
      map_state,
      "Klein-Nishina readback");

  if (!map_state.success) {
    throw std::runtime_error(
        "Klein-Nishina readback failed: " +
        map_state.error);
  }

  const void* mapped =
      wgpuBufferGetConstMappedRange(
          resources.readback,
          0,
          output_bytes);
  if (mapped == nullptr) {
    wgpuBufferUnmap(resources.readback);
    throw std::runtime_error(
        "Klein-Nishina mapped range is null");
  }

  std::vector<KnGpuOutput> gpu_output(
      gpu_input.size());
  std::memcpy(
      gpu_output.data(),
      mapped,
      static_cast<std::size_t>(output_bytes));
  wgpuBufferUnmap(resources.readback);

  std::vector<KleinNishinaSample> result(
      gpu_output.size());

  for (std::size_t i = 0; i < gpu_output.size(); ++i) {
    const auto& source = gpu_output[i];
    auto& target = result[i];

    target.scattered_gamma_energy_mev =
        static_cast<double>(
            source.scattered_gamma_energy_mev);
    target.recoil_electron_energy_mev =
        static_cast<double>(
            source.recoil_electron_energy_mev);
    target.cos_theta =
        static_cast<double>(source.cos_theta);
    target.sin_theta =
        static_cast<double>(source.sin_theta);
    target.phi =
        static_cast<double>(source.phi);
    target.iterations = source.iterations;
    target.accepted = source.accepted != 0u;

    rng[i].counter_lo = source.counter_lo;
    rng[i].counter_hi = source.counter_hi;
  }

  return result;
}

}  // namespace g4wgpu
