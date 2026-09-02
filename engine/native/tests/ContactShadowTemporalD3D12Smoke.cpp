#include "Aevrynd/Renderer.h"
#include "ContactShadowTemporalComputeShader.h"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace Aevrynd;

namespace {
constexpr UINT Width = 4;
constexpr UINT Height = 3;
constexpr UINT PixelBytes = sizeof(float) * 2;

void Require(HRESULT result, const char *message) {
  if (FAILED(result))
    throw std::runtime_error(message);
}

bool GpuValidationRequested() {
  char value[8]{};
  return GetEnvironmentVariableA("AEVRYND_GPU_VALIDATION", value,
                                 static_cast<DWORD>(sizeof(value))) > 0 &&
         value[0] == '1';
}

ComPtr<IDXGIAdapter1> SelectHardwareAdapter() {
  ComPtr<IDXGIFactory6> factory;
  if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
                                IID_PPV_ARGS(&factory))))
    return {};
  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND)
      return {};
    DXGI_ADAPTER_DESC1 description{};
    if (SUCCEEDED(adapter->GetDesc1(&description)) &&
        !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    __uuidof(ID3D12Device), nullptr)))
      return adapter;
  }
}

D3D12_RESOURCE_DESC TextureDescription(D3D12_RESOURCE_FLAGS flags) {
  D3D12_RESOURCE_DESC result{};
  result.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  result.Width = Width;
  result.Height = Height;
  result.DepthOrArraySize = 1;
  result.MipLevels = 1;
  result.Format = DXGI_FORMAT_R32G32_FLOAT;
  result.SampleDesc.Count = 1;
  result.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  result.Flags = flags;
  return result;
}

D3D12_RESOURCE_DESC BufferDescription(UINT64 bytes) {
  D3D12_RESOURCE_DESC result{};
  result.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  result.Width = bytes;
  result.Height = 1;
  result.DepthOrArraySize = 1;
  result.MipLevels = 1;
  result.SampleDesc.Count = 1;
  result.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  return result;
}

struct Fixture {
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  ComPtr<ID3D12Fence> fence;
  ComPtr<ID3D12InfoQueue> infoQueue;
  ComPtr<ID3D12RootSignature> rootSignature;
  ComPtr<ID3D12PipelineState> pipeline;
  ComPtr<ID3D12DescriptorHeap> descriptors;
  std::array<ComPtr<ID3D12Resource>, 3> inputs;
  std::array<ComPtr<ID3D12Resource>, 3> uploads;
  ComPtr<ID3D12Resource> output;
  HANDLE eventHandle{};
  UINT descriptorSize{};
  std::uint64_t fenceValue{};

  ~Fixture() {
    if (eventHandle)
      CloseHandle(eventHandle);
  }

  void Wait() {
    const auto value = ++fenceValue;
    Require(queue->Signal(fence.Get(), value), "Fence signal failed");
    if (fence->GetCompletedValue() < value) {
      Require(fence->SetEventOnCompletion(value, eventHandle),
              "Fence event failed");
      if (WaitForSingleObject(eventHandle, 30000) != WAIT_OBJECT_0)
        throw std::runtime_error("GPU wait timed out");
    }
  }

  void Initialize(const std::array<std::array<float, 2>, Width * Height> &current,
                  const std::array<std::array<float, 2>, Width * Height> &previous,
                  const std::array<std::array<float, 2>, Width * Height> &motion) {
    ComPtr<ID3D12Debug> debug;
    Require(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),
            "D3D12 debug layer unavailable");
    debug->EnableDebugLayer();
    if (GpuValidationRequested()) {
      ComPtr<ID3D12Debug1> debug1;
      Require(debug.As(&debug1), "GPU validation interface unavailable");
      debug1->SetEnableGPUBasedValidation(TRUE);
    }
    const auto adapter = SelectHardwareAdapter();
    if (!adapter)
      throw std::runtime_error("Hardware D3D12 adapter unavailable");
    Require(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                              IID_PPV_ARGS(&device)),
            "Device creation failed");
    Require(device.As(&infoQueue), "Info queue unavailable");
    infoQueue->ClearStoredMessages();

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Require(device->CreateCommandQueue(&queueDescription,
                                       IID_PPV_ARGS(&queue)),
            "Queue creation failed");
    Require(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(&allocator)),
            "Allocator creation failed");
    Require(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      allocator.Get(), nullptr,
                                      IID_PPV_ARGS(&list)),
            "Command list creation failed");
    Require(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                IID_PPV_ARGS(&fence)),
            "Fence creation failed");
    eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
      throw std::runtime_error("Fence event creation failed");

    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, 0};
    ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &ranges[0]};
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1, &ranges[1]};
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[2].Constants = {0, 0, sizeof(ContactShadowTemporalConstants3D) / 4};
    for (auto &parameter : parameters)
      parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rootDescription{};
    rootDescription.NumParameters = 3;
    rootDescription.pParameters = parameters;
    ComPtr<ID3DBlob> rootBlob, rootError;
    Require(D3D12SerializeRootSignature(&rootDescription,
                                        D3D_ROOT_SIGNATURE_VERSION_1,
                                        &rootBlob, &rootError),
            "Root signature serialization failed");
    Require(device->CreateRootSignature(0, rootBlob->GetBufferPointer(),
                                        rootBlob->GetBufferSize(),
                                        IID_PPV_ARGS(&rootSignature)),
            "Root signature creation failed");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDescription{};
    pipelineDescription.pRootSignature = rootSignature.Get();
    pipelineDescription.CS = {ContactShadowTemporalComputeShader,
                              sizeof(ContactShadowTemporalComputeShader)};
    Require(device->CreateComputePipelineState(&pipelineDescription,
                                                IID_PPV_ARGS(&pipeline)),
            "Compute pipeline creation failed");

    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 4;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Require(device->CreateDescriptorHeap(&heapDescription,
                                         IID_PPV_ARGS(&descriptors)),
            "Descriptor heap creation failed");
    descriptorSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const std::array<const void *, 3> source{current.data(), previous.data(),
                                             motion.data()};
    const auto textureDescription = TextureDescription(D3D12_RESOURCE_FLAG_NONE);
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    for (UINT index = 0; index < inputs.size(); ++index) {
      Require(device->CreateCommittedResource(
                  &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                  IID_PPV_ARGS(&inputs[index])),
              "Input texture creation failed");
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
      UINT rows{};
      UINT64 rowBytes{}, totalBytes{};
      device->GetCopyableFootprints(&textureDescription, 0, 1, 0, &footprint,
                                    &rows, &rowBytes, &totalBytes);
      const auto uploadDescription = BufferDescription(totalBytes);
      Require(device->CreateCommittedResource(
                  &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                  IID_PPV_ARGS(&uploads[index])),
              "Upload buffer creation failed");
      std::byte *mapped{};
      Require(uploads[index]->Map(0, nullptr,
                                  reinterpret_cast<void **>(&mapped)),
              "Upload map failed");
      for (UINT y = 0; y < Height; ++y)
        std::memcpy(mapped + footprint.Offset + y * footprint.Footprint.RowPitch,
                    static_cast<const std::byte *>(source[index]) +
                        y * Width * PixelBytes,
                    Width * PixelBytes);
      uploads[index]->Unmap(0, nullptr);
      D3D12_TEXTURE_COPY_LOCATION destination{inputs[index].Get(),
                                               D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
      D3D12_TEXTURE_COPY_LOCATION upload{uploads[index].Get(),
                                         D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
      upload.PlacedFootprint = footprint;
      list->CopyTextureRegion(&destination, 0, 0, 0, &upload, nullptr);
      D3D12_RESOURCE_BARRIER barrier{};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition = {inputs[index].Get(), 0,
                            D3D12_RESOURCE_STATE_COPY_DEST,
                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
      list->ResourceBarrier(1, &barrier);
    }
    const auto outputDescription =
        TextureDescription(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    Require(device->CreateCommittedResource(
                &defaultHeap, D3D12_HEAP_FLAG_NONE, &outputDescription,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                IID_PPV_ARGS(&output)),
            "Output texture creation failed");

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R32G32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    auto cpu = descriptors->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < inputs.size(); ++index) {
      device->CreateShaderResourceView(inputs[index].Get(), &srv, cpu);
      cpu.ptr += descriptorSize;
    }
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = DXGI_FORMAT_R32G32_FLOAT;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(output.Get(), nullptr, &uav, cpu);
    Require(list->Close(), "Upload command list close failed");
    ID3D12CommandList *lists[]{list.Get()};
    queue->ExecuteCommandLists(1, lists);
    Wait();
  }

  std::array<std::array<float, 2>, Width * Height>
  Dispatch(const ContactShadowTemporalConstants3D &constants) {
    Require(allocator->Reset(), "Allocator reset failed");
    Require(list->Reset(allocator.Get(), pipeline.Get()), "List reset failed");
    ID3D12DescriptorHeap *heaps[]{descriptors.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(rootSignature.Get());
    auto gpu = descriptors->GetGPUDescriptorHandleForHeapStart();
    list->SetComputeRootDescriptorTable(0, gpu);
    gpu.ptr += std::uint64_t(descriptorSize) * 3;
    list->SetComputeRootDescriptorTable(1, gpu);
    list->SetComputeRoot32BitConstants(2, sizeof(constants) / 4, &constants, 0);
    list->Dispatch((Width + 7) / 8, (Height + 7) / 8, 1);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = output.Get();
    list->ResourceBarrier(1, &barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {output.Get(), 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                          D3D12_RESOURCE_STATE_COPY_SOURCE};
    list->ResourceBarrier(1, &barrier);

    const auto description = output->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows{};
    UINT64 rowBytes{}, totalBytes{};
    device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, &rows,
                                  &rowBytes, &totalBytes);
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    ComPtr<ID3D12Resource> readback;
    const auto readbackDescription = BufferDescription(totalBytes);
    Require(device->CreateCommittedResource(
                &readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDescription,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&readback)),
            "Readback creation failed");
    D3D12_TEXTURE_COPY_LOCATION destination{readback.Get(),
                                             D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    destination.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION source{output.Get(),
                                        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    list->ResourceBarrier(1, &barrier);
    Require(list->Close(), "Dispatch command list close failed");
    ID3D12CommandList *lists[]{list.Get()};
    queue->ExecuteCommandLists(1, lists);
    Wait();

    std::array<std::array<float, 2>, Width * Height> result{};
    void *mappedRaw{};
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(totalBytes)};
    Require(readback->Map(0, &readRange, &mappedRaw),
            "Readback map failed");
    const auto *mapped = static_cast<const std::byte *>(mappedRaw);
    for (UINT y = 0; y < Height; ++y)
      std::memcpy(result.data() + y * Width,
                  mapped + footprint.Offset + y * footprint.Footprint.RowPitch,
                  Width * PixelBytes);
    D3D12_RANGE writtenRange{};
    readback->Unmap(0, &writtenRange);
    return result;
  }

  std::uint64_t SevereMessages() const {
    std::uint64_t severe{};
    const auto count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 index = 0; index < count; ++index) {
      SIZE_T bytes{};
      if (FAILED(infoQueue->GetMessage(index, nullptr, &bytes)) || !bytes) {
        ++severe;
        continue;
      }
      std::vector<std::byte> storage(bytes);
      auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
      if (FAILED(infoQueue->GetMessage(index, message, &bytes)) ||
          message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
          message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
          message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) {
        ++severe;
        if (message->pDescription)
          std::cerr << "D3D12 message " << message->ID << ": "
                    << message->pDescription << '\n';
      }
    }
    return severe;
  }
};

bool Near(float value, float expected) {
  return std::isfinite(value) && std::abs(value - expected) < 1e-5f;
}
} // namespace

int main() {
  try {
    const std::array<std::array<float, 2>, Width * Height> current{{
        {{.2f, .5f}}, {{.3f, .2f}}, {{.4f, .2f}}, {{.5f, .2f}},
        {{.6f, .7f}}, {{.7f, .3f}},
        {{std::numeric_limits<float>::quiet_NaN(), .4f}}, {{.9f, .6f}},
        {{std::numeric_limits<float>::infinity(), .2f}},
        {{-std::numeric_limits<float>::infinity(), .3f}}, {{.25f, .4f}},
        {{.35f, .5f}},
    }};
    const std::array<std::array<float, 2>, Width * Height> previous{{
        {{.8f, .505f}}, {{.9f, .6f}}, {{.7f, .2f}}, {{.1f, .2f}},
        {{.2f, .7f}}, {{std::numeric_limits<float>::quiet_NaN(), .3f}},
        {{.3f, .4f}}, {{.4f, .6f}},
        {{.1f, .2f}}, {{.2f, .3f}},
        {{std::numeric_limits<float>::infinity(), .4f}},
        {{-std::numeric_limits<float>::infinity(), .5f}},
    }};
    const std::array<std::array<float, 2>, Width * Height> motion{{
        {{0.f, 0.f}}, {{0.f, 0.f}}, {{5.f, 0.f}}, {{-5.f, 0.f}},
        {{0.f, 0.f}}, {{0.f, 0.f}}, {{0.f, 0.f}},
        {{std::numeric_limits<float>::quiet_NaN(), 0.f}},
        {{0.f, 0.f}}, {{0.f, 0.f}}, {{0.f, 0.f}}, {{0.f, 0.f}},
    }};
    Fixture fixture;
    fixture.Initialize(current, previous, motion);
    ContactShadowTemporalConstants3D constants{};
    constants.historyWeight = .75f;
    constants.depthThreshold = .02f;
    constants.motionThresholdPixels = 4.f;
    constants.historyValid = 1;
    constants.outputWidth = Width;
    constants.outputHeight = Height;
    constants.inverseOutputWidth = 1.f / Width;
    constants.inverseOutputHeight = 1.f / Height;
    const auto production = fixture.Dispatch(constants);
    if (!Near(production[0][0], .65f) || !Near(production[0][1], .5f) ||
        !Near(production[1][0], .3f) || !Near(production[2][0], .4f) ||
        !Near(production[3][0], .5f) || !Near(production[4][0], .3f) ||
        !Near(production[5][0], .7f) || !Near(production[6][0], 1.f) ||
        !Near(production[7][0], .9f) || !Near(production[8][0], 1.f) ||
        !Near(production[9][0], 1.f) || !Near(production[10][0], .25f) ||
        !Near(production[11][0], .35f)) {
      std::cerr << "Temporal production result mismatch\n";
      return 2;
    }
    constants.cameraCut = 1;
    const auto cameraCut = fixture.Dispatch(constants);
    for (std::size_t index = 0; index < current.size(); ++index)
      if (!Near(cameraCut[index][0], std::isfinite(current[index][0])
                                          ? current[index][0]
                                          : 1.f) ||
          !Near(cameraCut[index][1], current[index][1])) {
        std::cerr << "Camera-cut rejection mismatch\n";
        return 3;
      }
    constants.cameraCut = 0;
    constants.resized = 1;
    const auto resized = fixture.Dispatch(constants);
    for (std::size_t index = 0; index < current.size(); ++index)
      if (!Near(resized[index][0], std::isfinite(current[index][0])
                                      ? current[index][0]
                                      : 1.f)) {
        std::cerr << "Resize rejection mismatch\n";
        return 4;
      }
    const auto severe = fixture.SevereMessages();
    if (severe)
      return 5;
    std::cout << "ContactShadowTemporalD3D12 mode="
              << (GpuValidationRequested() ? "gbv" : "normal")
              << " dispatches=3 pixels=" << Width * Height
              << " warningsOrErrors=" << severe << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
