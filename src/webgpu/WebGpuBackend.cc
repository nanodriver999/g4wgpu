#include "g4wgpu/WebGpuBackend.hh"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
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
  state->done = true;
  state->success = status == WGPUMapAsyncStatus_Success;

  if (!state->success) {
    state->error = copy_message(message);
  }
}

WGPUShaderModule create_shader_module(WGPUDevice device, const char* source) {
  WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgsl.code = string_view(source);

  WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  descriptor.label = string_view("g4wgpu_axpy_shader");
  descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&wgsl);

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

}  // namespace

class WebGpuBackend::Impl {
 public:
  Impl() {
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
    while (!adapter_state.done) {
      wgpuInstanceProcessEvents(instance);
    }

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
    while (!device_state.done) {
      wgpuInstanceProcessEvents(instance);
    }

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

  ~Impl() {
    if (pipeline != nullptr) {
      wgpuComputePipelineRelease(pipeline);
    }
    if (shader != nullptr) {
      wgpuShaderModuleRelease(shader);
    }
    if (queue != nullptr) {
      wgpuQueueRelease(queue);
    }
    if (device != nullptr) {
      wgpuDeviceRelease(device);
    }
    if (adapter != nullptr) {
      wgpuAdapterRelease(adapter);
    }
    if (instance != nullptr) {
      wgpuInstanceRelease(instance);
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

    WGPUBuffer x_buffer = create_buffer(
        device, "g4wgpu_axpy_x", bytes,
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);
    WGPUBuffer y_buffer = create_buffer(
        device, "g4wgpu_axpy_y", bytes,
        WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
            WGPUBufferUsage_CopySrc);
    WGPUBuffer params_buffer = create_buffer(
        device, "g4wgpu_axpy_params", sizeof(AxpyParams),
        WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    WGPUBuffer readback_buffer = create_buffer(
        device, "g4wgpu_axpy_readback", bytes,
        WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);

    if (x_buffer == nullptr || y_buffer == nullptr ||
        params_buffer == nullptr || readback_buffer == nullptr) {
      if (x_buffer != nullptr) wgpuBufferRelease(x_buffer);
      if (y_buffer != nullptr) wgpuBufferRelease(y_buffer);
      if (params_buffer != nullptr) wgpuBufferRelease(params_buffer);
      if (readback_buffer != nullptr) wgpuBufferRelease(readback_buffer);
      throw std::runtime_error("failed to create WebGPU buffers");
    }

    WGPUBindGroupLayout layout =
        wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
    if (layout == nullptr) {
      wgpuBufferRelease(readback_buffer);
      wgpuBufferRelease(params_buffer);
      wgpuBufferRelease(y_buffer);
      wgpuBufferRelease(x_buffer);
      throw std::runtime_error(
          "wgpuComputePipelineGetBindGroupLayout failed");
    }

    WGPUBindGroupEntry entries[3] = {
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT,
    };

    entries[0].binding = 0;
    entries[0].buffer = x_buffer;
    entries[0].size = bytes;

    entries[1].binding = 1;
    entries[1].buffer = y_buffer;
    entries[1].size = bytes;

    entries[2].binding = 2;
    entries[2].buffer = params_buffer;
    entries[2].size = sizeof(AxpyParams);

    WGPUBindGroupDescriptor bind_group_descriptor =
        WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bind_group_descriptor.label = string_view("g4wgpu_axpy_bind_group");
    bind_group_descriptor.layout = layout;
    bind_group_descriptor.entryCount = 3;
    bind_group_descriptor.entries = entries;

    WGPUBindGroup bind_group =
        wgpuDeviceCreateBindGroup(device, &bind_group_descriptor);
    if (bind_group == nullptr) {
      wgpuBindGroupLayoutRelease(layout);
      wgpuBufferRelease(readback_buffer);
      wgpuBufferRelease(params_buffer);
      wgpuBufferRelease(y_buffer);
      wgpuBufferRelease(x_buffer);
      throw std::runtime_error("wgpuDeviceCreateBindGroup failed");
    }

    WGPUCommandEncoderDescriptor encoder_descriptor =
        WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    encoder_descriptor.label = string_view("g4wgpu_axpy_encoder");
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device, &encoder_descriptor);

    WGPUComputePassDescriptor pass_descriptor =
        WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    pass_descriptor.label = string_view("g4wgpu_axpy_pass");
    WGPUComputePassEncoder pass =
        wgpuCommandEncoderBeginComputePass(encoder, &pass_descriptor);

    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bind_group, 0, nullptr);

    const std::uint32_t workgroups =
        (static_cast<std::uint32_t>(x.size()) + 255u) / 256u;
    wgpuComputePassEncoderDispatchWorkgroups(pass, workgroups, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    wgpuCommandEncoderCopyBufferToBuffer(
        encoder, y_buffer, 0, readback_buffer, 0, bytes);

    WGPUCommandBufferDescriptor command_buffer_descriptor =
        WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    command_buffer_descriptor.label =
        string_view("g4wgpu_axpy_command_buffer");
    WGPUCommandBuffer command_buffer =
        wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);

    const AxpyParams params{
        a, static_cast<std::uint32_t>(x.size()), 0u, 0u};

    wgpuQueueWriteBuffer(queue, x_buffer, 0, x.data(), bytes);
    wgpuQueueWriteBuffer(queue, y_buffer, 0, y.data(), bytes);
    wgpuQueueWriteBuffer(
        queue, params_buffer, 0, &params, sizeof(params));
    wgpuQueueSubmit(queue, 1, &command_buffer);

    BufferMapState map_state;
    WGPUBufferMapCallbackInfo map_callback =
        WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    map_callback.mode = WGPUCallbackMode_AllowSpontaneous;
    map_callback.callback = on_buffer_map;
    map_callback.userdata1 = &map_state;

    wgpuBufferMapAsync(
        readback_buffer, WGPUMapMode_Read, 0, bytes, map_callback);
    wgpuDevicePoll(device, WGPU_TRUE, nullptr);

    if (!map_state.done || !map_state.success) {
      wgpuCommandBufferRelease(command_buffer);
      wgpuCommandEncoderRelease(encoder);
      wgpuBindGroupRelease(bind_group);
      wgpuBindGroupLayoutRelease(layout);
      wgpuBufferRelease(readback_buffer);
      wgpuBufferRelease(params_buffer);
      wgpuBufferRelease(y_buffer);
      wgpuBufferRelease(x_buffer);
      throw std::runtime_error(
          "WebGPU buffer mapping failed: " + map_state.error);
    }

    const void* mapped =
        wgpuBufferGetConstMappedRange(readback_buffer, 0, bytes);
    if (mapped == nullptr) {
      throw std::runtime_error(
          "wgpuBufferGetConstMappedRange returned null");
    }

    std::memcpy(y.data(), mapped, static_cast<std::size_t>(bytes));
    wgpuBufferUnmap(readback_buffer);

    wgpuCommandBufferRelease(command_buffer);
    wgpuCommandEncoderRelease(encoder);
    wgpuBindGroupRelease(bind_group);
    wgpuBindGroupLayoutRelease(layout);
    wgpuBufferRelease(readback_buffer);
    wgpuBufferRelease(params_buffer);
    wgpuBufferRelease(y_buffer);
    wgpuBufferRelease(x_buffer);
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
