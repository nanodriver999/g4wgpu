#include "g4wgpu/WebGpuBackend.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "wgpu.h"

namespace g4wgpu {
namespace {

WGPUStringView string_view(const char* value) {
  WGPUStringView view = WGPU_STRING_VIEW_INIT;
  view.data = value;
  view.length = WGPU_STRLEN;
  return view;
}

std::string copy_message(const WGPUStringView message) {
  if (message.data == nullptr) {
    return {};
  }

  if (message.length == WGPU_STRLEN) {
    return std::string(message.data);
  }

  return std::string(message.data, message.length);
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
          std::string(operation) + " timed out waiting for WebGPU callback");
    }

    std::this_thread::yield();
  }
}

struct AdapterRequestState {
  bool done = false;
  WGPUAdapter adapter = nullptr;
  std::string error;
};

void on_adapter_request(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                        WGPUStringView message, void* userdata1,
                        void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<AdapterRequestState*>(userdata1);
  state->done = true;

  if (status == WGPURequestAdapterStatus_Success) {
    state->adapter = adapter;
  } else {
    state->error = copy_message(message);
  }
}

struct DeviceRequestState {
  bool done = false;
  WGPUDevice device = nullptr;
  std::string error;
};

void on_device_request(WGPURequestDeviceStatus status, WGPUDevice device,
                       WGPUStringView message, void* userdata1,
                       void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<DeviceRequestState*>(userdata1);
  state->done = true;

  if (status == WGPURequestDeviceStatus_Success) {
    state->device = device;
  } else {
    state->error = copy_message(message);
  }
}

struct BufferMapState {
  bool done = false;
  bool success = false;
  std::string error;
};

void on_buffer_map(WGPUMapAsyncStatus status, WGPUStringView message,
                   void* userdata1, void* userdata2) {
  (void)userdata2;
  auto* state = static_cast<BufferMapState*>(userdata1);
  state->success = status == WGPUMapAsyncStatus_Success;

  if (!state->success) {
    state->error = copy_message(message);
  }

  state->done = true;
}

WGPUShaderModule create_shader_module(
    WGPUDevice device, const char* source,
    const char* label = "g4wgpu_axpy_shader") {
  WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgsl.code = string_view(source);

  WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  descriptor.label = string_view(label);
  descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl);

  return wgpuDeviceCreateShaderModule(device, &descriptor);
}

WGPUBuffer create_buffer(WGPUDevice device, const char* label,
                         std::uint64_t size, WGPUBufferUsage usage) {
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.label = string_view(label);
  descriptor.size = size;
  descriptor.usage = usage;
  descriptor.mappedAtCreation = WGPU_FALSE;

  return wgpuDeviceCreateBuffer(device, &descriptor);
}

struct AxpyParams {
  float a;
  std::uint32_t count;
  std::uint32_t pad0;
  std::uint32_t pad1;
};

static_assert(sizeof(AxpyParams) == 16);

constexpr const char* kAxpyShader = R"WGSL(
struct Params {
  a: f32,
  count: u32,
  pad0: u32,
  pad1: u32,
}

@group(0) @binding(0)
var<storage, read> x: array<f32>;

@group(0) @binding(1)
var<storage, read_write> y: array<f32>;

@group(0) @binding(2)
var<uniform> params: Params;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let i = id.x;
  if (i >= params.count) {
    return;
  }

  y[i] = params.a * x[i] + y[i];
}
)WGSL";


struct GpuBoxRay {
  float px;
  float py;
  float pz;
  float pad0;
  float dx;
  float dy;
  float dz;
  float pad1;
};
static_assert(sizeof(GpuBoxRay) == 32);

struct GpuBoxParams {
  float half_x;
  float half_y;
  float half_z;
  std::uint32_t count;
};
static_assert(sizeof(GpuBoxParams) == 16);

// A read-only standalone Box DistanceToIn experiment. Geant4 navigation
// remains authoritative; invalid f32 inputs are rejected in the C++ adapter.
constexpr const char* kBoxEntryShader = R"WGSL(
struct Ray {
  px: f32,
  py: f32,
  pz: f32,
  pad0: f32,
  dx: f32,
  dy: f32,
  dz: f32,
  pad1: f32,
}

struct Params {
  half_x: f32,
  half_y: f32,
  half_z: f32,
  count: u32,
}

@group(0) @binding(0)
var<storage, read> rays: array<Ray>;

@group(0) @binding(1)
var<storage, read_write> distances: array<f32>;

@group(0) @binding(2)
var<uniform> params: Params;

@compute @workgroup_size(128)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let index = id.x;
  if (index >= params.count) {
    return;
  }

  let ray = rays[index];
  let p = vec3<f32>(ray.px, ray.py, ray.pz);
  let d = vec3<f32>(ray.dx, ray.dy, ray.dz);
  let half = vec3<f32>(
      params.half_x, params.half_y, params.half_z);
  let infinity = bitcast<f32>(0x7f800000u);

  var entry = -infinity;
  var exit = infinity;
  for (var axis = 0u; axis < 3u; axis = axis + 1u) {
    let origin = p[axis];
    let direction = d[axis];
    let extent = half[axis];
    if (direction == 0.0) {
      if (origin < -extent || origin > extent) {
        distances[index] = infinity;
        return;
      }
    } else {
      let a = (-extent - origin) / direction;
      let b = (extent - origin) / direction;
      entry = max(entry, min(a, b));
      exit = min(exit, max(a, b));
      if (entry > exit) {
        distances[index] = infinity;
        return;
      }
    }
  }

  if (exit < 0.0) {
    distances[index] = infinity;
  } else {
    distances[index] = max(0.0, entry);
  }
}
)WGSL";

struct DispatchResources {
  WGPUShaderModule shader = nullptr;
  WGPUComputePipeline pipeline = nullptr;
  WGPUBuffer x_buffer = nullptr;
  WGPUBuffer y_buffer = nullptr;
  WGPUBuffer params_buffer = nullptr;
  WGPUBuffer readback_buffer = nullptr;
  WGPUBindGroupLayout layout = nullptr;
  WGPUBindGroup bind_group = nullptr;
  WGPUCommandEncoder encoder = nullptr;
  WGPUCommandBuffer command_buffer = nullptr;

  ~DispatchResources() {
    if (command_buffer != nullptr) {
      wgpuCommandBufferRelease(command_buffer);
    }
    if (encoder != nullptr) {
      wgpuCommandEncoderRelease(encoder);
    }
    if (bind_group != nullptr) {
      wgpuBindGroupRelease(bind_group);
    }
    if (layout != nullptr) {
      wgpuBindGroupLayoutRelease(layout);
    }
    if (readback_buffer != nullptr) {
      wgpuBufferRelease(readback_buffer);
    }
    if (params_buffer != nullptr) {
      wgpuBufferRelease(params_buffer);
    }
    if (y_buffer != nullptr) {
      wgpuBufferRelease(y_buffer);
    }
    if (x_buffer != nullptr) {
      wgpuBufferRelease(x_buffer);
    }
    if (pipeline != nullptr) {
      wgpuComputePipelineRelease(pipeline);
    }
    if (shader != nullptr) {
      wgpuShaderModuleRelease(shader);
    }
  }
};

}  // namespace

class WebGpuBackend::Impl {
 public:
  Impl() {
    try {
      initialize();
    } catch (...) {
      release();
      throw;
    }
  }

  ~Impl() {
    release();
  }

  void initialize() {
    instance = wgpuCreateInstance(nullptr);
    if (instance == nullptr) {
      throw std::runtime_error("wgpuCreateInstance failed");
    }

    AdapterRequestState adapter_state;
    WGPURequestAdapterCallbackInfo adapter_callback =
        WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapter_callback.mode = WGPUCallbackMode_AllowProcessEvents;
    adapter_callback.callback = on_adapter_request;
    adapter_callback.userdata1 = &adapter_state;

    wgpuInstanceRequestAdapter(instance, nullptr, adapter_callback);
    wait_for_callback(instance, adapter_state, "wgpuInstanceRequestAdapter");

    if (adapter_state.adapter == nullptr) {
      throw std::runtime_error(
          "wgpuInstanceRequestAdapter failed: " + adapter_state.error);
    }
    adapter = adapter_state.adapter;

    DeviceRequestState device_state;
    WGPURequestDeviceCallbackInfo device_callback =
        WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    device_callback.mode = WGPUCallbackMode_AllowProcessEvents;
    device_callback.callback = on_device_request;
    device_callback.userdata1 = &device_state;

    wgpuAdapterRequestDevice(adapter, nullptr, device_callback);
    wait_for_callback(instance, device_state, "wgpuAdapterRequestDevice");

    if (device_state.device == nullptr) {
      throw std::runtime_error(
          "wgpuAdapterRequestDevice failed: " + device_state.error);
    }
    device = device_state.device;

    queue = wgpuDeviceGetQueue(device);
    if (queue == nullptr) {
      throw std::runtime_error("wgpuDeviceGetQueue failed");
    }

    shader = create_shader_module(device, kAxpyShader);
    if (shader == nullptr) {
      throw std::runtime_error("wgpuDeviceCreateShaderModule failed");
    }

    WGPUComputeState compute = WGPU_COMPUTE_STATE_INIT;
    compute.module = shader;
    compute.entryPoint = string_view("main");

    WGPUComputePipelineDescriptor pipeline_descriptor =
        WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pipeline_descriptor.label = string_view("g4wgpu_axpy_pipeline");
    pipeline_descriptor.compute = compute;

    pipeline =
        wgpuDeviceCreateComputePipeline(device, &pipeline_descriptor);
    if (pipeline == nullptr) {
      throw std::runtime_error("wgpuDeviceCreateComputePipeline failed");
    }
  }

  void release() noexcept {
    if (pipeline != nullptr) {
      wgpuComputePipelineRelease(pipeline);
      pipeline = nullptr;
    }
    if (shader != nullptr) {
      wgpuShaderModuleRelease(shader);
      shader = nullptr;
    }
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

  void axpy(const float a, const std::vector<float>& x,
            std::vector<float>& y) {
    if (x.size() != y.size()) {
      throw std::invalid_argument(
          "WebGpuBackend::axpy requires equal vector lengths");
    }
    if (x.empty()) {
      return;
    }
    if (x.size() > static_cast<std::size_t>(UINT32_MAX)) {
      throw std::overflow_error(
          "WebGpuBackend::axpy input exceeds u32 shader index range");
    }

    const std::uint64_t bytes =
        static_cast<std::uint64_t>(x.size() * sizeof(float));

    DispatchResources resources;
    resources.x_buffer = create_buffer(
        device, "g4wgpu_axpy_x", bytes,
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);
    resources.y_buffer = create_buffer(
        device, "g4wgpu_axpy_y", bytes,
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
            WGPUBufferUsage_CopySrc);
    resources.params_buffer = create_buffer(
        device, "g4wgpu_axpy_params", sizeof(AxpyParams),
        WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    resources.readback_buffer = create_buffer(
        device, "g4wgpu_axpy_readback", bytes,
        WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);

    if (resources.x_buffer == nullptr || resources.y_buffer == nullptr ||
        resources.params_buffer == nullptr ||
        resources.readback_buffer == nullptr) {
      throw std::runtime_error("failed to create WebGPU buffers");
    }

    resources.layout =
        wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
    if (resources.layout == nullptr) {
      throw std::runtime_error(
          "wgpuComputePipelineGetBindGroupLayout failed");
    }

    WGPUBindGroupEntry entries[3] = {
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT,
    };

    entries[0].binding = 0;
    entries[0].buffer = resources.x_buffer;
    entries[0].size = bytes;

    entries[1].binding = 1;
    entries[1].buffer = resources.y_buffer;
    entries[1].size = bytes;

    entries[2].binding = 2;
    entries[2].buffer = resources.params_buffer;
    entries[2].size = sizeof(AxpyParams);

    WGPUBindGroupDescriptor bind_group_descriptor =
        WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bind_group_descriptor.label = string_view("g4wgpu_axpy_bind_group");
    bind_group_descriptor.layout = resources.layout;
    bind_group_descriptor.entryCount = 3;
    bind_group_descriptor.entries = entries;

    resources.bind_group =
        wgpuDeviceCreateBindGroup(device, &bind_group_descriptor);
    if (resources.bind_group == nullptr) {
      throw std::runtime_error("wgpuDeviceCreateBindGroup failed");
    }

    WGPUCommandEncoderDescriptor encoder_descriptor =
        WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    encoder_descriptor.label = string_view("g4wgpu_axpy_encoder");
    resources.encoder =
        wgpuDeviceCreateCommandEncoder(device, &encoder_descriptor);
    if (resources.encoder == nullptr) {
      throw std::runtime_error("wgpuDeviceCreateCommandEncoder failed");
    }

    WGPUComputePassDescriptor pass_descriptor =
        WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    pass_descriptor.label = string_view("g4wgpu_axpy_pass");
    WGPUComputePassEncoder pass =
        wgpuCommandEncoderBeginComputePass(
            resources.encoder, &pass_descriptor);
    if (pass == nullptr) {
      throw std::runtime_error(
          "wgpuCommandEncoderBeginComputePass failed");
    }

    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(
        pass, 0, resources.bind_group, 0, nullptr);

    const std::uint32_t workgroups =
        (static_cast<std::uint32_t>(x.size()) + 255u) / 256u;
    wgpuComputePassEncoderDispatchWorkgroups(pass, workgroups, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    wgpuCommandEncoderCopyBufferToBuffer(
        resources.encoder, resources.y_buffer, 0,
        resources.readback_buffer, 0, bytes);

    WGPUCommandBufferDescriptor command_buffer_descriptor =
        WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    command_buffer_descriptor.label =
        string_view("g4wgpu_axpy_command_buffer");
    resources.command_buffer =
        wgpuCommandEncoderFinish(
            resources.encoder, &command_buffer_descriptor);
    if (resources.command_buffer == nullptr) {
      throw std::runtime_error("wgpuCommandEncoderFinish failed");
    }

    const AxpyParams params{
        a, static_cast<std::uint32_t>(x.size()), 0u, 0u};

    wgpuQueueWriteBuffer(
        queue, resources.x_buffer, 0, x.data(), bytes);
    wgpuQueueWriteBuffer(
        queue, resources.y_buffer, 0, y.data(), bytes);
    wgpuQueueWriteBuffer(
        queue, resources.params_buffer, 0, &params, sizeof(params));
    wgpuQueueSubmit(queue, 1, &resources.command_buffer);

    BufferMapState map_state;
    WGPUBufferMapCallbackInfo map_callback =
        WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    map_callback.mode = WGPUCallbackMode_AllowProcessEvents;
    map_callback.callback = on_buffer_map;
    map_callback.userdata1 = &map_state;

    wgpuBufferMapAsync(
        resources.readback_buffer, WGPUMapMode_Read, 0, bytes,
        map_callback);
    wgpuDevicePoll(device, WGPU_TRUE, nullptr);
    wait_for_callback(instance, map_state, "wgpuBufferMapAsync");

    if (!map_state.success) {
      throw std::runtime_error(
          "WebGPU buffer mapping failed: " + map_state.error);
    }

    const void* mapped =
        wgpuBufferGetConstMappedRange(
            resources.readback_buffer, 0, bytes);
    if (mapped == nullptr) {
      wgpuBufferUnmap(resources.readback_buffer);
      throw std::runtime_error(
          "wgpuBufferGetConstMappedRange returned null");
    }

    std::memcpy(
        y.data(), mapped, static_cast<std::size_t>(bytes));
    wgpuBufferUnmap(resources.readback_buffer);
  }


  std::vector<double> box_distance_to_in_batch(
      const AxisAlignedBox& box,
      const std::vector<BoxRay>& rays) {
    const auto finite_f32 = [](double value) -> float {
      const float converted = static_cast<float>(value);
      if (!std::isfinite(value) || !std::isfinite(converted)) {
        throw std::invalid_argument(
            "Box GPU input must be representable by finite f32");
      }
      return converted;
    };

    const GpuBoxParams parameters{
        finite_f32(box.half_lengths_mm.x),
        finite_f32(box.half_lengths_mm.y),
        finite_f32(box.half_lengths_mm.z),
        static_cast<std::uint32_t>(rays.size())};
    if (!(parameters.half_x > 0.0f) ||
        !(parameters.half_y > 0.0f) ||
        !(parameters.half_z > 0.0f)) {
      throw std::invalid_argument(
          "Box GPU half lengths must be positive in f32");
    }
    constexpr std::size_t kMaxRays = 65535u * 128u;
    if (rays.size() > kMaxRays) {
      throw std::overflow_error(
          "Box GPU batch exceeds portable workgroup dispatch limit");
    }
    if (rays.empty()) {
      return {};
    }

    std::vector<GpuBoxRay> inputs;
    inputs.reserve(rays.size());
    for (const BoxRay& ray : rays) {
      const float px = finite_f32(ray.position_mm.x);
      const float py = finite_f32(ray.position_mm.y);
      const float pz = finite_f32(ray.position_mm.z);
      const double direction_norm =
          std::hypot(
              ray.direction.x, ray.direction.y, ray.direction.z);
      if (!(direction_norm > 0.0) ||
          !std::isfinite(direction_norm)) {
        throw std::invalid_argument(
            "Box GPU direction must be finite and nonzero");
      }
      const float dx =
          finite_f32(ray.direction.x / direction_norm);
      const float dy =
          finite_f32(ray.direction.y / direction_norm);
      const float dz =
          finite_f32(ray.direction.z / direction_norm);
      inputs.push_back(GpuBoxRay{
          px, py, pz, 0.0f,
          dx, dy, dz, 0.0f});
    }

    const std::uint64_t input_bytes =
        static_cast<std::uint64_t>(
            inputs.size() * sizeof(GpuBoxRay));
    const std::uint64_t output_bytes =
        static_cast<std::uint64_t>(
            inputs.size() * sizeof(float));

    DispatchResources resources;
    resources.shader = create_shader_module(
        device, kBoxEntryShader, "g4wgpu_box_entry_shader");
    if (resources.shader == nullptr) {
      throw std::runtime_error(
          "failed to create Box entry shader module");
    }

    WGPUComputeState compute = WGPU_COMPUTE_STATE_INIT;
    compute.module = resources.shader;
    compute.entryPoint = string_view("main");
    WGPUComputePipelineDescriptor pipeline_descriptor =
        WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pipeline_descriptor.label =
        string_view("g4wgpu_box_entry_pipeline");
    pipeline_descriptor.compute = compute;
    resources.pipeline =
        wgpuDeviceCreateComputePipeline(
            device, &pipeline_descriptor);
    if (resources.pipeline == nullptr) {
      throw std::runtime_error(
          "failed to create Box entry compute pipeline");
    }

    resources.x_buffer = create_buffer(
        device, "g4wgpu_box_rays", input_bytes,
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);
    resources.y_buffer = create_buffer(
        device, "g4wgpu_box_distances", output_bytes,
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc);
    resources.params_buffer = create_buffer(
        device, "g4wgpu_box_params", sizeof(GpuBoxParams),
        WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    resources.readback_buffer = create_buffer(
        device, "g4wgpu_box_readback", output_bytes,
        WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);
    if (resources.x_buffer == nullptr ||
        resources.y_buffer == nullptr ||
        resources.params_buffer == nullptr ||
        resources.readback_buffer == nullptr) {
      throw std::runtime_error(
          "failed to create Box entry buffers");
    }

    resources.layout = wgpuComputePipelineGetBindGroupLayout(
        resources.pipeline, 0);
    if (resources.layout == nullptr) {
      throw std::runtime_error(
          "failed to get Box entry bind group layout");
    }

    WGPUBindGroupEntry entries[3] = {
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].buffer = resources.x_buffer;
    entries[0].size = input_bytes;
    entries[1].binding = 1;
    entries[1].buffer = resources.y_buffer;
    entries[1].size = output_bytes;
    entries[2].binding = 2;
    entries[2].buffer = resources.params_buffer;
    entries[2].size = sizeof(GpuBoxParams);

    WGPUBindGroupDescriptor bind_group_descriptor =
        WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bind_group_descriptor.label =
        string_view("g4wgpu_box_entry_bind_group");
    bind_group_descriptor.layout = resources.layout;
    bind_group_descriptor.entryCount = 3;
    bind_group_descriptor.entries = entries;
    resources.bind_group =
        wgpuDeviceCreateBindGroup(
            device, &bind_group_descriptor);
    if (resources.bind_group == nullptr) {
      throw std::runtime_error(
          "failed to create Box entry bind group");
    }

    wgpuQueueWriteBuffer(
        queue, resources.x_buffer, 0,
        inputs.data(), input_bytes);
    wgpuQueueWriteBuffer(
        queue, resources.params_buffer, 0,
        &parameters, sizeof(parameters));

    WGPUCommandEncoderDescriptor encoder_descriptor =
        WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    encoder_descriptor.label =
        string_view("g4wgpu_box_entry_encoder");
    resources.encoder =
        wgpuDeviceCreateCommandEncoder(
            device, &encoder_descriptor);
    if (resources.encoder == nullptr) {
      throw std::runtime_error(
          "failed to create Box entry command encoder");
    }
    WGPUComputePassDescriptor pass_descriptor =
        WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    pass_descriptor.label =
        string_view("g4wgpu_box_entry_pass");
    WGPUComputePassEncoder pass =
        wgpuCommandEncoderBeginComputePass(
            resources.encoder, &pass_descriptor);
    if (pass == nullptr) {
      throw std::runtime_error(
          "failed to start Box entry pass");
    }

    wgpuComputePassEncoderSetPipeline(
        pass, resources.pipeline);
    wgpuComputePassEncoderSetBindGroup(
        pass, 0, resources.bind_group, 0, nullptr);
    const std::uint32_t workgroups =
        static_cast<std::uint32_t>(
            (rays.size() + 127u) / 128u);
    wgpuComputePassEncoderDispatchWorkgroups(
        pass, workgroups, 1u, 1u);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    wgpuCommandEncoderCopyBufferToBuffer(
        resources.encoder, resources.y_buffer, 0,
        resources.readback_buffer, 0, output_bytes);

    WGPUCommandBufferDescriptor command_descriptor =
        WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    command_descriptor.label =
        string_view("g4wgpu_box_entry_command");
    resources.command_buffer =
        wgpuCommandEncoderFinish(
            resources.encoder, &command_descriptor);
    if (resources.command_buffer == nullptr) {
      throw std::runtime_error(
          "failed to finish Box entry command buffer");
    }
    wgpuQueueSubmit(queue, 1, &resources.command_buffer);

    BufferMapState map_state;
    WGPUBufferMapCallbackInfo callback =
        WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowProcessEvents;
    callback.callback = on_buffer_map;
    callback.userdata1 = &map_state;
    wgpuBufferMapAsync(
        resources.readback_buffer, WGPUMapMode_Read,
        0, output_bytes, callback);
    wgpuDevicePoll(device, WGPU_TRUE, nullptr);
    wait_for_callback(
        instance, map_state, "Box entry readback");

    if (!map_state.success) {
      throw std::runtime_error(
          "Box entry readback failed: " + map_state.error);
    }
    const void* mapped = wgpuBufferGetConstMappedRange(
        resources.readback_buffer, 0, output_bytes);
    if (mapped == nullptr) {
      wgpuBufferUnmap(resources.readback_buffer);
      throw std::runtime_error(
          "Box entry mapped range is null");
    }

    std::vector<float> output(rays.size());
    std::memcpy(
        output.data(), mapped,
        static_cast<std::size_t>(output_bytes));
    wgpuBufferUnmap(resources.readback_buffer);

    std::vector<double> distances;
    distances.reserve(output.size());
    for (const float value : output) {
      if (std::isnan(value) || value < 0.0f) {
        throw std::runtime_error(
            "Box GPU kernel returned invalid distance");
      }
      distances.push_back(static_cast<double>(value));
    }
    return distances;
  }

  WGPUInstance instance = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPUShaderModule shader = nullptr;
  WGPUComputePipeline pipeline = nullptr;
};

WebGpuBackend::WebGpuBackend() : impl_(std::make_unique<Impl>()) {}

WebGpuBackend::~WebGpuBackend() = default;

WebGpuBackend::WebGpuBackend(WebGpuBackend&&) noexcept = default;

WebGpuBackend& WebGpuBackend::operator=(WebGpuBackend&&) noexcept = default;

std::string_view WebGpuBackend::name() const noexcept {
  return "webgpu/wgpu-native";
}

void WebGpuBackend::axpy(const double a, const std::vector<double>& x,
                         std::vector<double>& y) {
  if (x.size() != y.size()) {
    throw std::invalid_argument(
        "WebGpuBackend::axpy requires equal vector lengths");
  }

  std::vector<float> x_staging(x.size());
  std::vector<float> y_staging(y.size());

  std::transform(
      x.begin(), x.end(), x_staging.begin(),
      [](double value) { return static_cast<float>(value); });
  std::transform(
      y.begin(), y.end(), y_staging.begin(),
      [](double value) { return static_cast<float>(value); });

  impl_->axpy(static_cast<float>(a), x_staging, y_staging);

  std::transform(
      y_staging.begin(), y_staging.end(), y.begin(),
      [](float value) { return static_cast<double>(value); });
}

std::vector<double> WebGpuBackend::box_distance_to_in_batch(
    const AxisAlignedBox& box,
    const std::vector<BoxRay>& rays) {
  return impl_->box_distance_to_in_batch(box, rays);
}

void WebGpuBackend::advance_positions(TrackBatch& tracks, const double dt) {
  tracks.validate();

  // PR 3 deliberately reuses the verified AXPY runtime path. A future
  // resident TrackBatch implementation will fuse or batch these dispatches
  // instead of performing three independent transfers.
  axpy(dt, tracks.direction_x, tracks.position_x);
  axpy(dt, tracks.direction_y, tracks.position_y);
  axpy(dt, tracks.direction_z, tracks.position_z);
}

}  // namespace g4wgpu
