#include "g4wgpu/WebGpuBackend.hh"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "wgpu.h"

namespace g4wgpu {
namespace {

WGPUStringView pc_string_view(const char* value) {
  WGPUStringView view = WGPU_STRING_VIEW_INIT;
  view.data = value;
  view.length = WGPU_STRLEN;
  return view;
}

std::string pc_copy_message(const WGPUStringView message) {
  if (message.data == nullptr) {
    return {};
  }
  if (message.length == WGPU_STRLEN) {
    return std::string(message.data);
  }
  return std::string(message.data, message.length);
}

struct AdapterState {
  bool done = false;
  WGPUAdapter adapter = nullptr;
  std::string error;
};

void on_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                WGPUStringView message, void* userdata1,
                void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<AdapterState*>(userdata1);
  if (status == WGPURequestAdapterStatus_Success) {
    state->adapter = adapter;
  } else {
    state->error = pc_copy_message(message);
  }
  state->done = true;
}

struct DeviceState {
  bool done = false;
  WGPUDevice device = nullptr;
  std::string error;
};

void on_device(WGPURequestDeviceStatus status, WGPUDevice device,
               WGPUStringView message, void* userdata1,
               void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<DeviceState*>(userdata1);
  if (status == WGPURequestDeviceStatus_Success) {
    state->device = device;
  } else {
    state->error = pc_copy_message(message);
  }
  state->done = true;
}

struct MapState {
  bool done = false;
  bool success = false;
  std::string error;
};

void on_map(WGPUMapAsyncStatus status, WGPUStringView message,
            void* userdata1, void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<MapState*>(userdata1);
  state->success = status == WGPUMapAsyncStatus_Success;
  if (!state->success) {
    state->error = pc_copy_message(message);
  }
  state->done = true;
}

template <typename State>
void wait_for_callback(WGPUInstance instance, const State& state,
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

WGPUBuffer create_buffer(WGPUDevice device, const char* label,
                         std::uint64_t size, WGPUBufferUsage usage) {
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.label = pc_string_view(label);
  descriptor.size = size;
  descriptor.usage = usage;
  descriptor.mappedAtCreation = WGPU_FALSE;
  return wgpuDeviceCreateBuffer(device, &descriptor);
}

struct Runtime {
  WGPUInstance instance = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;

  Runtime() {
    instance = wgpuCreateInstance(nullptr);
    if (instance == nullptr) {
      throw std::runtime_error(
          "gamma competition WebGPU instance creation failed");
    }

    try {
      AdapterState adapter_state;
      WGPURequestAdapterCallbackInfo adapter_callback =
          WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
      adapter_callback.mode = WGPUCallbackMode_AllowProcessEvents;
      adapter_callback.callback = on_adapter;
      adapter_callback.userdata1 = &adapter_state;
      wgpuInstanceRequestAdapter(instance, nullptr, adapter_callback);
      wait_for_callback(instance, adapter_state, "request adapter");

      if (adapter_state.adapter == nullptr) {
        throw std::runtime_error(
            "gamma competition adapter request failed: " +
            adapter_state.error);
      }
      adapter = adapter_state.adapter;

      DeviceState device_state;
      WGPURequestDeviceCallbackInfo device_callback =
          WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
      device_callback.mode = WGPUCallbackMode_AllowProcessEvents;
      device_callback.callback = on_device;
      device_callback.userdata1 = &device_state;
      wgpuAdapterRequestDevice(adapter, nullptr, device_callback);
      wait_for_callback(instance, device_state, "request device");

      if (device_state.device == nullptr) {
        throw std::runtime_error(
            "gamma competition device request failed: " +
            device_state.error);
      }
      device = device_state.device;
      queue = wgpuDeviceGetQueue(device);
      if (queue == nullptr) {
        throw std::runtime_error(
            "gamma competition queue acquisition failed");
      }
    } catch (...) {
      release();
      throw;
    }
  }

  ~Runtime() {
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

struct GpuInput {
  float compton_per_mm;
  float photoelectric_per_mm;
  float pair_production_per_mm;
  float pad0;
  std::uint32_t stream_lo;
  std::uint32_t stream_hi;
  std::uint32_t counter_lo;
  std::uint32_t counter_hi;
};

static_assert(sizeof(GpuInput) == 32);

struct GpuOutput {
  std::uint32_t process;
  float distance_mm;
  float total_cross_section_per_mm;
  std::uint32_t counter_lo;
  std::uint32_t counter_hi;
  std::uint32_t pad0;
  std::uint32_t pad1;
  std::uint32_t pad2;
};

static_assert(sizeof(GpuOutput) == 32);

struct Params {
  std::uint32_t count;
  std::uint32_t pad0;
  std::uint32_t pad1;
  std::uint32_t pad2;
};

static_assert(sizeof(Params) == 16);

struct Resources {
  WGPUShaderModule shader = nullptr;
  WGPUComputePipeline pipeline = nullptr;
  WGPUBuffer input = nullptr;
  WGPUBuffer output = nullptr;
  WGPUBuffer params = nullptr;
  WGPUBuffer readback = nullptr;
  WGPUBindGroupLayout layout = nullptr;
  WGPUBindGroup bind_group = nullptr;
  WGPUCommandEncoder encoder = nullptr;
  WGPUCommandBuffer command = nullptr;

  ~Resources() {
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

constexpr const char* kShader = R"WGSL(
struct Input {
  compton_per_mm: f32,
  photoelectric_per_mm: f32,
  pair_production_per_mm: f32,
  pad0: f32,
  stream_lo: u32,
  stream_hi: u32,
  counter_lo: u32,
  counter_hi: u32,
}

struct Output {
  process: u32,
  distance_mm: f32,
  total_cross_section_per_mm: f32,
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
var<storage, read> inputs: array<Input>;

@group(0) @binding(1)
var<storage, read_write> outputs: array<Output>;

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

  let total =
      input.compton_per_mm +
      input.photoelectric_per_mm +
      input.pair_production_per_mm;

  var out: Output;
  out.process = 0u;
  out.distance_mm = bitcast<f32>(0x7f800000u);
  out.total_cross_section_per_mm = total;
  out.counter_lo = state.counter_lo;
  out.counter_hi = state.counter_hi;
  out.pad0 = 0u;
  out.pad1 = 0u;
  out.pad2 = 0u;

  if (total <= 0.0) {
    rng_increment(&state);
    rng_increment(&state);
    out.counter_lo = state.counter_lo;
    out.counter_hi = state.counter_hi;
    outputs[index] = out;
    return;
  }

  let u_distance = rng_next(&state);
  let min_u = 1.0 / 16777216.0;
  let clamped_u = max(u_distance, min_u);
  out.distance_mm = -log(clamped_u) / total;

  let u_process = rng_next(&state);
  let threshold = u_process * total;

  if (threshold < input.compton_per_mm) {
    out.process = 1u;
  } else if (
      threshold <
      input.compton_per_mm +
          input.photoelectric_per_mm) {
    out.process = 2u;
  } else {
    out.process = 3u;
  }

  out.counter_lo = state.counter_lo;
  out.counter_hi = state.counter_hi;
  outputs[index] = out;
}
)WGSL";

}  // namespace

std::vector<GammaInteractionSample>
WebGpuBackend::sample_gamma_process_competition_batch(
    const std::vector<GammaProcessCrossSections>& cross_sections,
    std::vector<RngAddress>& rng) {
  if (cross_sections.size() != rng.size()) {
    throw std::invalid_argument(
        "gamma process competition batch and RNG state sizes differ");
  }
  if (cross_sections.empty()) {
    return {};
  }
  if (cross_sections.size() >
      static_cast<std::size_t>(UINT32_MAX)) {
    throw std::overflow_error(
        "gamma process competition batch exceeds u32 dispatch range");
  }

  std::vector<GpuInput> gpu_input(cross_sections.size());
  for (std::size_t i = 0; i < cross_sections.size(); ++i) {
    cross_sections[i].validate();
    gpu_input[i] = GpuInput{
        static_cast<float>(cross_sections[i].compton_per_mm),
        static_cast<float>(cross_sections[i].photoelectric_per_mm),
        static_cast<float>(cross_sections[i].pair_production_per_mm),
        0.0f,
        rng[i].stream_lo,
        rng[i].stream_hi,
        rng[i].counter_lo,
        rng[i].counter_hi};
  }

  const std::uint64_t input_bytes =
      static_cast<std::uint64_t>(
          gpu_input.size() * sizeof(GpuInput));
  const std::uint64_t output_bytes =
      static_cast<std::uint64_t>(
          gpu_input.size() * sizeof(GpuOutput));

  Runtime runtime;
  Resources resources;

  WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgsl.code = pc_string_view(kShader);

  WGPUShaderModuleDescriptor shader_descriptor =
      WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  shader_descriptor.label =
      pc_string_view("g4wgpu_gamma_competition_shader");
  shader_descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&wgsl);

  resources.shader =
      wgpuDeviceCreateShaderModule(
          runtime.device, &shader_descriptor);
  if (resources.shader == nullptr) {
    throw std::runtime_error(
        "failed to create gamma competition shader module");
  }

  WGPUComputeState compute = WGPU_COMPUTE_STATE_INIT;
  compute.module = resources.shader;
  compute.entryPoint = pc_string_view("main");

  WGPUComputePipelineDescriptor pipeline_descriptor =
      WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
  pipeline_descriptor.label =
      pc_string_view("g4wgpu_gamma_competition_pipeline");
  pipeline_descriptor.compute = compute;

  resources.pipeline =
      wgpuDeviceCreateComputePipeline(
          runtime.device, &pipeline_descriptor);
  if (resources.pipeline == nullptr) {
    throw std::runtime_error(
        "failed to create gamma competition pipeline");
  }

  resources.input = create_buffer(
      runtime.device,
      "g4wgpu_gamma_competition_input",
      input_bytes,
      WGPUBufferUsage_Storage |
          WGPUBufferUsage_CopyDst);
  resources.output = create_buffer(
      runtime.device,
      "g4wgpu_gamma_competition_output",
      output_bytes,
      WGPUBufferUsage_Storage |
          WGPUBufferUsage_CopySrc);
  resources.params = create_buffer(
      runtime.device,
      "g4wgpu_gamma_competition_params",
      sizeof(Params),
      WGPUBufferUsage_Uniform |
          WGPUBufferUsage_CopyDst);
  resources.readback = create_buffer(
      runtime.device,
      "g4wgpu_gamma_competition_readback",
      output_bytes,
      WGPUBufferUsage_MapRead |
          WGPUBufferUsage_CopyDst);

  if (resources.input == nullptr ||
      resources.output == nullptr ||
      resources.params == nullptr ||
      resources.readback == nullptr) {
    throw std::runtime_error(
        "failed to create gamma competition buffers");
  }

  resources.layout =
      wgpuComputePipelineGetBindGroupLayout(
          resources.pipeline, 0);
  if (resources.layout == nullptr) {
    throw std::runtime_error(
        "failed to get gamma competition bind group layout");
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
  entries[2].size = sizeof(Params);

  WGPUBindGroupDescriptor bind_group_descriptor =
      WGPU_BIND_GROUP_DESCRIPTOR_INIT;
  bind_group_descriptor.label =
      pc_string_view("g4wgpu_gamma_competition_bind_group");
  bind_group_descriptor.layout = resources.layout;
  bind_group_descriptor.entryCount = 3;
  bind_group_descriptor.entries = entries;

  resources.bind_group =
      wgpuDeviceCreateBindGroup(
          runtime.device, &bind_group_descriptor);
  if (resources.bind_group == nullptr) {
    throw std::runtime_error(
        "failed to create gamma competition bind group");
  }

  const Params params{
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
      pc_string_view("g4wgpu_gamma_competition_encoder");

  resources.encoder =
      wgpuDeviceCreateCommandEncoder(
          runtime.device, &encoder_descriptor);
  if (resources.encoder == nullptr) {
    throw std::runtime_error(
        "failed to create gamma competition command encoder");
  }

  WGPUComputePassDescriptor pass_descriptor =
      WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
  pass_descriptor.label =
      pc_string_view("g4wgpu_gamma_competition_pass");

  WGPUComputePassEncoder pass =
      wgpuCommandEncoderBeginComputePass(
          resources.encoder, &pass_descriptor);
  if (pass == nullptr) {
    throw std::runtime_error(
        "failed to begin gamma competition pass");
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
      pc_string_view("g4wgpu_gamma_competition_command");
  resources.command =
      wgpuCommandEncoderFinish(
          resources.encoder, &command_descriptor);
  if (resources.command == nullptr) {
    throw std::runtime_error(
        "failed to finish gamma competition command buffer");
  }

  wgpuQueueSubmit(
      runtime.queue, 1, &resources.command);

  MapState map_state;
  WGPUBufferMapCallbackInfo map_callback =
      WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
  map_callback.mode = WGPUCallbackMode_AllowProcessEvents;
  map_callback.callback = on_map;
  map_callback.userdata1 = &map_state;

  wgpuBufferMapAsync(
      resources.readback,
      WGPUMapMode_Read,
      0,
      output_bytes,
      map_callback);
  wgpuDevicePoll(runtime.device, WGPU_TRUE, nullptr);
  wait_for_callback(
      runtime.instance,
      map_state,
      "gamma competition readback");

  if (!map_state.success) {
    throw std::runtime_error(
        "gamma competition readback failed: " +
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
        "gamma competition mapped range is null");
  }

  std::vector<GpuOutput> gpu_output(
      gpu_input.size());
  std::memcpy(
      gpu_output.data(),
      mapped,
      static_cast<std::size_t>(output_bytes));
  wgpuBufferUnmap(resources.readback);

  std::vector<GammaInteractionSample> result(
      gpu_output.size());

  for (std::size_t i = 0; i < gpu_output.size(); ++i) {
    const auto& source = gpu_output[i];
    auto& target = result[i];

    if (source.process >
        static_cast<std::uint32_t>(
            GammaProcess::pair_production)) {
      throw std::runtime_error(
          "gamma competition shader returned invalid process id");
    }

    target.process =
        static_cast<GammaProcess>(source.process);
    target.distance_mm =
        static_cast<double>(source.distance_mm);
    target.total_cross_section_per_mm =
        static_cast<double>(
            source.total_cross_section_per_mm);

    rng[i].counter_lo = source.counter_lo;
    rng[i].counter_hi = source.counter_hi;
  }

  return result;
}

}  // namespace g4wgpu
