#include "Aevrynd/VolumetricAtmospherePassD3D12.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <limits>
#include <utility>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace Aevrynd {
namespace {
constexpr DXGI_FORMAT AtmosphereFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr std::uint32_t ConstantFrameSlices = 3;
constexpr std::uint64_t ConstantSliceBytes = 512;
constexpr std::uint64_t BlueNoiseBytes = 64u * 64u;
constexpr std::uint64_t DiagnosticBytes =
    AEVRYND_INTERNAL_RENDERER_TEST_HOOKS ? ConstantFrameSlices * 32u + 16u : 0u;

float FiniteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float ClampFinite(float value, float fallback, float minimum,
                  float maximum) noexcept {
  return (std::clamp)(FiniteOr(value, fallback), minimum, maximum);
}

float NormalizeScale(float value) noexcept {
  return ClampFinite(value, .5f, .25f, 1.f);
}

std::uint32_t ScaledExtent(std::uint32_t extent, float scale) noexcept {
  if (!extent)
    return 0;
  return (std::max)(1u, std::uint32_t(std::ceil(float(extent) * scale)));
}

D3D12_CPU_DESCRIPTOR_HANDLE Offset(D3D12_CPU_DESCRIPTOR_HANDLE start,
                                   UINT increment, UINT index) noexcept {
  start.ptr += SIZE_T(increment) * index;
  return start;
}

D3D12_GPU_DESCRIPTOR_HANDLE Offset(D3D12_GPU_DESCRIPTOR_HANDLE start,
                                   UINT increment, UINT index) noexcept {
  start.ptr += UINT64(increment) * index;
  return start;
}
} // namespace

struct VolumetricAtmospherePassD3D12::Impl {
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12RootSignature> rootSignature;
  ComPtr<ID3D12PipelineState> pipeline;
  ComPtr<ID3D12DescriptorHeap> descriptorHeap;
  ComPtr<ID3D12Resource> constantBuffer;
  std::byte *constantMapped{};
  ComPtr<ID3D12Resource> blueNoiseBuffer;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  std::array<ComPtr<ID3D12Resource>, ConstantFrameSlices> temporalCounters;
  std::array<ComPtr<ID3D12Resource>, ConstantFrameSlices> temporalReadbacks;
  ComPtr<ID3D12Resource> temporalCounterZero;
  std::byte *temporalCounterZeroMapped{};
  std::array<bool, ConstantFrameSlices> temporalReadbackPending{};
  std::array<D3D12_RESOURCE_STATES, ConstantFrameSlices> temporalCounterStates{};
#endif
  std::array<ComPtr<ID3D12Resource>, 2> textures;
  std::array<ComPtr<ID3D12Resource>, 2> segmentTextures;
  VolumetricAtmospherePassStats3D stats{};
  std::uint32_t fullWidth{}, fullHeight{}, writeIndex{};
  std::uint64_t topologyFingerprint{};
  std::uint32_t temporalSequenceIndex{};
  float previousTimeSeconds{};
  bool previousTimeValid{};
  float resolutionScale{.5f};
  UINT descriptorIncrement{};

  bool CreateResources(std::uint32_t width, std::uint32_t height,
                       float scale) {
    const float normalizedScale = NormalizeScale(scale);
    const std::uint32_t scaledWidth = ScaledExtent(width, normalizedScale),
                        scaledHeight = ScaledExtent(height, normalizedScale);
    if (!device || !scaledWidth || !scaledHeight)
      return false;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = scaledWidth;
    description.Height = scaledHeight;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = AtmosphereFormat;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    std::array<ComPtr<ID3D12Resource>, 2> replacement;
    for (auto &texture : replacement)
      if (FAILED(device->CreateCommittedResource(
              &heapProperties, D3D12_HEAP_FLAG_NONE, &description,
              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
              IID_PPV_ARGS(&texture))))
        return false;
    D3D12_RESOURCE_DESC segmentDescription = description;
    segmentDescription.Format = DXGI_FORMAT_R16G16_FLOAT;
    std::array<ComPtr<ID3D12Resource>, 2> segmentReplacement;
    for (auto &texture : segmentReplacement)
      if (FAILED(device->CreateCommittedResource(
              &heapProperties, D3D12_HEAP_FLAG_NONE, &segmentDescription,
              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
              IID_PPV_ARGS(&texture))))
        return false;

    const auto cpuStart = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = AtmosphereFormat;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = AtmosphereFormat;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    for (UINT index = 0; index < 2; ++index) {
      device->CreateShaderResourceView(replacement[index].Get(), &srv,
                                       Offset(cpuStart, descriptorIncrement,
                                              index * 2));
      device->CreateUnorderedAccessView(replacement[index].Get(), nullptr, &uav,
                                        Offset(cpuStart, descriptorIncrement,
                                               index * 2 + 1));
    }
    srv.Format = DXGI_FORMAT_R16G16_FLOAT;
    uav.Format = DXGI_FORMAT_R16G16_FLOAT;
    for (UINT index = 0; index < 2; ++index) {
      device->CreateShaderResourceView(segmentReplacement[index].Get(), &srv,
                                       Offset(cpuStart, descriptorIncrement,
                                              4 + index * 2));
      device->CreateUnorderedAccessView(segmentReplacement[index].Get(), nullptr,
                                        &uav,
                                        Offset(cpuStart, descriptorIncrement,
                                               5 + index * 2));
    }

    std::uint64_t releases{};
    for (const auto &texture : textures)
      releases += texture ? 1u : 0u;
    for (const auto &texture : segmentTextures)
      releases += texture ? 1u : 0u;
    stats.resourceReleases += releases;
    textures = std::move(replacement);
    segmentTextures = std::move(segmentReplacement);
    stats.resourceCreates += 4;
    fullWidth = width;
    fullHeight = height;
    resolutionScale = normalizedScale;
    stats.outputWidth = scaledWidth;
    stats.outputHeight = scaledHeight;
    const auto allocation = device->GetResourceAllocationInfo(0, 1, &description);
    const auto segmentAllocation =
        device->GetResourceAllocationInfo(0, 1, &segmentDescription);
    const bool allocationFits =
        allocation.SizeInBytes <= (std::numeric_limits<std::size_t>::max)() / 2 &&
                segmentAllocation.SizeInBytes <=
                    (std::numeric_limits<std::size_t>::max)() / 2 &&
                std::size_t(allocation.SizeInBytes) * 2 <=
                    (std::numeric_limits<std::size_t>::max)() - BlueNoiseBytes - DiagnosticBytes -
                    std::size_t(segmentAllocation.SizeInBytes) * 2;
    stats.residentBytes = allocationFits
        ? BlueNoiseBytes + DiagnosticBytes + std::size_t(allocation.SizeInBytes) * 2 +
              std::size_t(segmentAllocation.SizeInBytes) * 2
        : (std::numeric_limits<std::size_t>::max)();
    stats.historyValid = false;
    writeIndex = 0;
    temporalSequenceIndex = 0;
    previousTimeValid = false;
    ++stats.historyResets;
    return true;
  }
};

VolumetricAtmospherePassD3D12::VolumetricAtmospherePassD3D12() = default;
VolumetricAtmospherePassD3D12::~VolumetricAtmospherePassD3D12() = default;
VolumetricAtmospherePassD3D12::VolumetricAtmospherePassD3D12(
    VolumetricAtmospherePassD3D12 &&) noexcept = default;
VolumetricAtmospherePassD3D12 &
VolumetricAtmospherePassD3D12::operator=(
    VolumetricAtmospherePassD3D12 &&) noexcept = default;

bool VolumetricAtmospherePassD3D12::Initialize(
    ID3D12Device *device, std::span<const std::byte> computeShader,
    std::uint32_t outputWidth, std::uint32_t outputHeight,
    float resolutionScale) {
  if (!device || computeShader.empty() || !outputWidth || !outputHeight)
    return false;
  auto candidate = std::make_unique<Impl>();
  candidate->device = device;
  candidate->stats.temporalDiagnosticsAvailable =
      AEVRYND_INTERNAL_RENDERER_TEST_HOOKS != 0;

  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors = 9 + ConstantFrameSlices
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
                        + ConstantFrameSlices
#endif
      ;
  heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device->CreateDescriptorHeap(
          &heap, IID_PPV_ARGS(&candidate->descriptorHeap))))
    return false;
  candidate->descriptorIncrement = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_HEAP_PROPERTIES uploadHeap{};
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC constantDescription{};
  constantDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  constantDescription.Width = ConstantSliceBytes * ConstantFrameSlices;
  constantDescription.Height = 1;
  constantDescription.DepthOrArraySize = 1;
  constantDescription.MipLevels = 1;
  constantDescription.SampleDesc.Count = 1;
  constantDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if (FAILED(device->CreateCommittedResource(
          &uploadHeap, D3D12_HEAP_FLAG_NONE, &constantDescription,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&candidate->constantBuffer))) ||
      FAILED(candidate->constantBuffer->Map(
          0, nullptr, reinterpret_cast<void **>(&candidate->constantMapped))))
    return false;
  D3D12_RESOURCE_DESC noiseDescription = constantDescription;
  noiseDescription.Width = BlueNoiseBytes;
  std::byte *noiseMapped{};
  if (FAILED(device->CreateCommittedResource(
          &uploadHeap, D3D12_HEAP_FLAG_NONE, &noiseDescription,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&candidate->blueNoiseBuffer))) ||
      FAILED(candidate->blueNoiseBuffer->Map(
          0, nullptr, reinterpret_cast<void **>(&noiseMapped))))
    return false;
  // 64x64 deterministic rank tile；低頻能量由測試鎖定，且沒有 runtime allocation。
  for (std::uint32_t index = 0; index < BlueNoiseBytes; ++index) {
    const std::uint32_t x = index & 63u, y = index >> 6u;
    noiseMapped[index] = std::byte(VolumetricBlueNoiseR8Sample3D(x, y));
  }
  candidate->blueNoiseBuffer->Unmap(0, nullptr);
  D3D12_SHADER_RESOURCE_VIEW_DESC noiseSrv{};
  noiseSrv.Format = DXGI_FORMAT_R32_TYPELESS;
  noiseSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  noiseSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  noiseSrv.Buffer.NumElements = UINT(BlueNoiseBytes / 4);
  noiseSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
  device->CreateShaderResourceView(
      candidate->blueNoiseBuffer.Get(), &noiseSrv,
      Offset(candidate->descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
             candidate->descriptorIncrement, 8));
  ++candidate->stats.resourceCreates;
  candidate->stats.residentBytes += BlueNoiseBytes;
  candidate->stats.blueNoiseResidentBytes = BlueNoiseBytes;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  D3D12_RESOURCE_DESC counterDescription = constantDescription;
  counterDescription.Width = 16;
  D3D12_RESOURCE_DESC counterDefaultDescription = counterDescription;
  counterDefaultDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  D3D12_HEAP_PROPERTIES defaultHeap{};
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_HEAP_PROPERTIES readbackHeap{};
  readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
  for (UINT frameSlice = 0; frameSlice < ConstantFrameSlices; ++frameSlice) {
    if (FAILED(device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &counterDefaultDescription,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&candidate->temporalCounters[frameSlice]))) ||
        FAILED(device->CreateCommittedResource(
            &readbackHeap, D3D12_HEAP_FLAG_NONE, &counterDescription,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&candidate->temporalReadbacks[frameSlice]))))
      return false;
  }
  if (FAILED(device->CreateCommittedResource(
          &uploadHeap, D3D12_HEAP_FLAG_NONE, &counterDescription,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&candidate->temporalCounterZero))) ||
      FAILED(candidate->temporalCounterZero->Map(
          0, nullptr,
          reinterpret_cast<void **>(&candidate->temporalCounterZeroMapped))))
    return false;
  std::memset(candidate->temporalCounterZeroMapped, 0, 16);
#endif

  D3D12_DESCRIPTOR_RANGE ranges[
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
      7
#else
      6
#endif
  ]{};
  ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  ranges[0].NumDescriptors = 1;
  ranges[0].BaseShaderRegister = 0;
  ranges[0].OffsetInDescriptorsFromTableStart = 0;
  ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[1].NumDescriptors = 1;
  ranges[1].BaseShaderRegister = 0;
  ranges[1].OffsetInDescriptorsFromTableStart = 0;
  ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[2].NumDescriptors = 1;
  ranges[2].BaseShaderRegister = 1;
  ranges[2].OffsetInDescriptorsFromTableStart = 0;
  ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  ranges[3].NumDescriptors = 1;
  ranges[3].BaseShaderRegister = 1;
  ranges[3].OffsetInDescriptorsFromTableStart = 0;
  ranges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[4].NumDescriptors = 1;
  ranges[4].BaseShaderRegister = 2;
  ranges[4].OffsetInDescriptorsFromTableStart = 0;
  ranges[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[5].NumDescriptors = 1;
  ranges[5].BaseShaderRegister = 3;
  ranges[5].OffsetInDescriptorsFromTableStart = 0;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  ranges[6].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  ranges[6].NumDescriptors = 1;
  ranges[6].BaseShaderRegister = 2;
  ranges[6].OffsetInDescriptorsFromTableStart = 0;
  D3D12_ROOT_PARAMETER parameters[8]{};
#else
  D3D12_ROOT_PARAMETER parameters[7]{};
#endif
  parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[0].DescriptorTable = {1, &ranges[0]};
  parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[1].DescriptorTable = {1, &ranges[1]};
  parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[2].DescriptorTable = {1, &ranges[2]};
  parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[3].DescriptorTable = {1, &ranges[3]};
  parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[4].DescriptorTable = {1, &ranges[4]};
  parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[5].DescriptorTable = {1, &ranges[5]};
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  parameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[6].DescriptorTable = {1, &ranges[6]};
  parameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[7].Descriptor = {0, 0};
#else
  parameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[6].Descriptor = {0, 0};
#endif
  D3D12_ROOT_SIGNATURE_DESC rootDescription{};
  rootDescription.NumParameters =
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
      8;
#else
      7;
#endif
  rootDescription.pParameters = parameters;
  rootDescription.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(
          &rootDescription, D3D_ROOT_SIGNATURE_VERSION_1,
          &serialized, &errors)) ||
      FAILED(device->CreateRootSignature(
          0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
          IID_PPV_ARGS(&candidate->rootSignature))))
    return false;

  D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
  pipeline.pRootSignature = candidate->rootSignature.Get();
  pipeline.CS = {computeShader.data(), computeShader.size_bytes()};
  if (FAILED(device->CreateComputePipelineState(
          &pipeline, IID_PPV_ARGS(&candidate->pipeline))))
    return false;
  if (!candidate->CreateResources(outputWidth, outputHeight, resolutionScale))
    return false;
  D3D12_SHADER_RESOURCE_VIEW_DESC nullDepth{};
  nullDepth.Format = DXGI_FORMAT_R32_FLOAT;
  nullDepth.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  nullDepth.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  nullDepth.Texture2D.MipLevels = 1;
  for (UINT frameSlice = 0; frameSlice < ConstantFrameSlices; ++frameSlice)
    device->CreateShaderResourceView(
        nullptr, &nullDepth,
        Offset(candidate->descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
               candidate->descriptorIncrement, 9 + frameSlice));
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  D3D12_UNORDERED_ACCESS_VIEW_DESC counterUav{};
  counterUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  counterUav.Format = DXGI_FORMAT_UNKNOWN;
  counterUav.Buffer.NumElements = 4;
  counterUav.Buffer.StructureByteStride = sizeof(std::uint32_t);
  for (UINT frameSlice = 0; frameSlice < ConstantFrameSlices; ++frameSlice)
    device->CreateUnorderedAccessView(
        candidate->temporalCounters[frameSlice].Get(), nullptr, &counterUav,
        Offset(candidate->descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
               candidate->descriptorIncrement,
               9 + ConstantFrameSlices + frameSlice));
  candidate->stats.resourceCreates += ConstantFrameSlices * 2 + 1;
  candidate->stats.residentBytes += ConstantFrameSlices * 32 + 16;
#endif
  impl_ = std::move(candidate);
  return true;
}

bool VolumetricAtmospherePassD3D12::Resize(std::uint32_t outputWidth,
                                           std::uint32_t outputHeight,
                                           float resolutionScale) {
  if (!impl_)
    return false;
  const float normalizedScale = NormalizeScale(resolutionScale);
  if (outputWidth == impl_->fullWidth && outputHeight == impl_->fullHeight &&
      normalizedScale == impl_->resolutionScale)
    return true;
  // 呼叫端必須先等待使用舊資源的 GPU fence，避免釋放仍在飛行中的材質。
  return impl_->CreateResources(outputWidth, outputHeight, normalizedScale);
}

bool VolumetricAtmospherePassD3D12::Record(
    ID3D12GraphicsCommandList *commandList,
    const VolumetricAtmospherePassSettings3D &source) {
  return Record(commandList, nullptr, source);
}

bool VolumetricAtmospherePassD3D12::Record(
    ID3D12GraphicsCommandList *commandList, ID3D12Resource *sceneDepth,
    const VolumetricAtmospherePassSettings3D &source) {
  if (!impl_ || !commandList || !source.enabled ||
      NormalizeScale(source.resolutionScale) != impl_->resolutionScale) {
    if (impl_)
      ++impl_->stats.skippedFrames;
    return false;
  }
  auto &state = *impl_;
  if (source.resetHistory) {
    InvalidateHistory();
    state.temporalSequenceIndex = 0;
  }
  const std::uint32_t frameSlice = source.frameSlice % ConstantFrameSlices;
  const UINT current = state.writeIndex;
  const UINT previous = current ^ 1u;
  D3D12_SHADER_RESOURCE_VIEW_DESC depthSrv{};
  depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
  depthSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  depthSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  depthSrv.Texture2D.MipLevels = 1;
  // 每個 frame 只覆寫 fence 已完成的 depth descriptor slice。
  state.device->CreateShaderResourceView(
      sceneDepth, &depthSrv,
      Offset(state.descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
             state.descriptorIncrement, 9 + frameSlice));
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  if (source.enableTemporalDiagnostics) {
  D3D12_RESOURCE_BARRIER counterToCopy{};
  counterToCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  counterToCopy.Transition = {
      state.temporalCounters[frameSlice].Get(),
      D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      state.temporalCounterStates[frameSlice],
      D3D12_RESOURCE_STATE_COPY_DEST};
  commandList->ResourceBarrier(1, &counterToCopy);
  commandList->CopyBufferRegion(state.temporalCounters[frameSlice].Get(), 0,
                                state.temporalCounterZero.Get(), 0, 16);
  D3D12_RESOURCE_BARRIER counterToUav = counterToCopy;
  counterToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  counterToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  commandList->ResourceBarrier(1, &counterToUav);
  }
#endif
  float sunX = FiniteOr(source.sunDirectionX, .35f);
  float sunY = FiniteOr(source.sunDirectionY, .82f);
  float sunZ = FiniteOr(source.sunDirectionZ, -.44f);
  const float sunLengthSquared = sunX * sunX + sunY * sunY + sunZ * sunZ;
  if (!std::isfinite(sunLengthSquared) || sunLengthSquared < 1e-8f) {
    sunX = .35f;
    sunY = .82f;
    sunZ = -.44f;
  }
  const float normalizedSunLengthSquared = sunX * sunX + sunY * sunY + sunZ * sunZ;
  const float inverseSunLength = 1.f / std::sqrt(normalizedSunLengthSquared);
  sunX *= inverseSunLength;
  sunY *= inverseSunLength;
  sunZ *= inverseSunLength;
  D3D12_RESOURCE_BARRIER toWrite[2]{};
  for (auto &barrier : toWrite) {
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  }
  toWrite[0].Transition.pResource = state.textures[current].Get();
  toWrite[1].Transition.pResource = state.segmentTextures[current].Get();
  commandList->ResourceBarrier(2, toWrite);
  ID3D12DescriptorHeap *heaps[] = {state.descriptorHeap.Get()};
  commandList->SetDescriptorHeaps(1, heaps);
  commandList->SetComputeRootSignature(state.rootSignature.Get());
  commandList->SetPipelineState(state.pipeline.Get());
  const auto gpuStart = state.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
  commandList->SetComputeRootDescriptorTable(
      0, Offset(gpuStart, state.descriptorIncrement, current * 2 + 1));
  commandList->SetComputeRootDescriptorTable(
      1, Offset(gpuStart, state.descriptorIncrement, previous * 2));
  commandList->SetComputeRootDescriptorTable(
      2, Offset(gpuStart, state.descriptorIncrement, 9 + frameSlice));
  commandList->SetComputeRootDescriptorTable(
      3, Offset(gpuStart, state.descriptorIncrement, 5 + current * 2));
  commandList->SetComputeRootDescriptorTable(
      4, Offset(gpuStart, state.descriptorIncrement, 4 + previous * 2));
  commandList->SetComputeRootDescriptorTable(
      5, Offset(gpuStart, state.descriptorIncrement, 8));
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  commandList->SetComputeRootDescriptorTable(
      6, Offset(gpuStart, state.descriptorIncrement,
                9 + ConstantFrameSlices + frameSlice));
#endif
  std::array<std::uint32_t, 128> constants{
      state.stats.outputWidth,
      state.stats.outputHeight,
      state.fullWidth,
      state.fullHeight,
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.viewportOriginX, 0.f, 0.f, float(state.fullWidth))),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.viewportOriginY, 0.f, 0.f, float(state.fullHeight))),
      std::bit_cast<std::uint32_t>(ClampFinite(
          source.viewportWidth, float(state.fullWidth), 1.f, float(state.fullWidth))),
      std::bit_cast<std::uint32_t>(ClampFinite(
          source.viewportHeight, float(state.fullHeight), 1.f, float(state.fullHeight))),
      (std::min)(std::uint32_t(source.phase), 2u),
      state.stats.historyValid ? 1u : 0u,
      std::bit_cast<std::uint32_t>(ClampFinite(source.historyWeight, .88f, 0.f, .98f)),
      std::bit_cast<std::uint32_t>(FiniteOr(source.timeSeconds, 0.f)),
      std::bit_cast<std::uint32_t>(ClampFinite(source.cloudCoverage, .48f, .05f, .95f)),
      std::bit_cast<std::uint32_t>(ClampFinite(source.cloudDensity, 1.08f, 0.f, 4.f)),
      std::bit_cast<std::uint32_t>(ClampFinite(source.cloudErosion, .28f, 0.f, .8f)),
      std::bit_cast<std::uint32_t>(ClampFinite(source.cloudWindSpeed, 18.f, -200.f, 200.f)),
      std::bit_cast<std::uint32_t>(ClampFinite(source.lightShaftIntensity, .68f, 0.f, 1.5f)),
      std::bit_cast<std::uint32_t>(ClampFinite(source.lightShaftAnisotropy, .72f, -.8f, .85f))};
  constants[18] =
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
      source.enableTemporalDiagnostics ? 1u : 0u;
#else
      0u;
#endif
  constants[19] =
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
      (std::min)(source.testMutation, 9u);
#else
      0u;
#endif
  constants[20] = std::bit_cast<std::uint32_t>(sunX);
  constants[21] = std::bit_cast<std::uint32_t>(sunY);
  constants[22] = std::bit_cast<std::uint32_t>(sunZ);
  constants[23] = source.useExternalTemporalSampleIndex
      ? source.temporalSampleIndex & 7u
      : state.temporalSequenceIndex & 7u;
  for (std::size_t index = 0; index < 16; ++index)
    constants[24 + index] = std::bit_cast<std::uint32_t>(
        FiniteOr(source.inverseViewProjection[index], index % 5 == 0 ? 1.f : 0.f));
  constants[40] = std::bit_cast<std::uint32_t>(FiniteOr(source.cameraPositionX, 0.f));
  constants[41] = std::bit_cast<std::uint32_t>(FiniteOr(source.cameraPositionY, 0.f));
  constants[42] = std::bit_cast<std::uint32_t>(FiniteOr(source.cameraPositionZ, 0.f));
  constants[43] = std::bit_cast<std::uint32_t>(ClampFinite(source.cloudBaseAltitude, 1200.f, -10000.f, 100000.f));
  constants[44] = std::bit_cast<std::uint32_t>(ClampFinite(source.cloudThickness, 1800.f, 1.f, 50000.f));
  constants[45] = sceneDepth ? 1u : 0u;
  constants[46] = source.projectionOrthographic ? 1u : 0u;
  constants[47] = source.previousProjectionOrthographic ? 1u : 0u;
  for (std::size_t index = 0; index < 16; ++index)
    constants[48 + index] = std::bit_cast<std::uint32_t>(
        FiniteOr(source.previousViewProjection[index], index % 5 == 0 ? 1.f : 0.f));
  constants[64] = (std::clamp)(source.cloudSampleCount, 4u, 32u);
  constants[65] = (std::clamp)(source.lightShaftSampleCount, 2u, 16u);
  constants[66] = (std::clamp)(source.cloudShadowSampleCount, 1u, 8u);
  constants[67] = source.cloudShadowsEnabled ? 1u : 0u;
  constants[68] = std::bit_cast<std::uint32_t>(ClampFinite(source.cloudMaximumDistance, 12000.f, 64.f, 50000.f));
  constants[69] = std::bit_cast<std::uint32_t>(ClampFinite(source.lightShaftMaximumDistance, 320.f, 1.f, 5000.f));
  constants[70] = std::bit_cast<std::uint32_t>(ClampFinite(source.cloudShadowStrength, .72f, 0.f, 1.f));
  constants[71] = std::bit_cast<std::uint32_t>(ClampFinite(source.lightShaftShadowStrength, .72f, 0.f, 1.f));
  constants[72] = std::bit_cast<std::uint32_t>(ClampFinite(source.heightFogDensity, .0035f, 0.f, 1.f));
  constants[73] = std::bit_cast<std::uint32_t>(ClampFinite(source.heightFogFalloff, .09f, 0.f, 4.f));
  constants[74] = std::bit_cast<std::uint32_t>(ClampFinite(source.heightFogBaseHeight, -2.f, -10000.f, 10000.f));
  constants[75] = std::bit_cast<std::uint32_t>(ClampFinite(source.heightFogMaximumOpacity, .76f, 0.f, .98f));
  constants[76] = std::bit_cast<std::uint32_t>(ClampFinite(source.sunRadianceR, 1.f, 0.f, 64.f));
  constants[77] = std::bit_cast<std::uint32_t>(ClampFinite(source.sunRadianceG, .96f, 0.f, 64.f));
  constants[78] = std::bit_cast<std::uint32_t>(ClampFinite(source.sunRadianceB, .88f, 0.f, 64.f));
  constants[79] = std::bit_cast<std::uint32_t>(ClampFinite(source.sunRadianceIntensity, 1.f, 0.f, 64.f));
  constants[80] = std::bit_cast<std::uint32_t>(ClampFinite(source.ambientRadianceR, .12f, 0.f, 16.f));
  constants[81] = std::bit_cast<std::uint32_t>(ClampFinite(source.ambientRadianceG, .17f, 0.f, 16.f));
  constants[82] = std::bit_cast<std::uint32_t>(ClampFinite(source.ambientRadianceB, .24f, 0.f, 16.f));
  const float currentTime = FiniteOr(source.timeSeconds, 0.f);
  const float rawTemporalDelta = state.previousTimeValid
      ? currentTime - state.previousTimeSeconds : 0.f;
  const float maximumTemporalDelta = ClampFinite(
      source.maximumTemporalDeltaSeconds, .5f, .01f, 5.f);
  const bool timeJump = state.previousTimeValid &&
      (!std::isfinite(rawTemporalDelta) ||
       std::abs(rawTemporalDelta) > maximumTemporalDelta);
  const float temporalDelta = timeJump ? 0.f : rawTemporalDelta;
  constants[83] = std::bit_cast<std::uint32_t>(temporalDelta);
  constants[84] = source.cloudBaseNoiseSeed;
  constants[85] = source.cloudDetailNoiseSeed == source.cloudBaseNoiseSeed ? source.cloudDetailNoiseSeed ^ 0x85ebca6bu : source.cloudDetailNoiseSeed;
  constants[86] = std::bit_cast<std::uint32_t>(ClampFinite(source.cloudBaseNoiseRotation, .37f, -6.2831853f, 6.2831853f));
  constants[87] = std::bit_cast<std::uint32_t>(ClampFinite(source.cloudDetailNoiseRotation, -1.11f, -6.2831853f, 6.2831853f));
  std::uint64_t fingerprint = 14695981039346656037ull;
  const auto mix = [&](std::uint32_t value) { fingerprint ^= value; fingerprint *= 1099511628211ull; };
  for (std::size_t index : {8u,12u,13u,14u,15u,16u,17u,19u,20u,21u,22u,43u,44u}) mix(constants[index]);
  for (std::size_t index = 64; index < 88; ++index) if (index != 83) mix(constants[index]);
  const bool topologyChanged = state.topologyFingerprint && state.topologyFingerprint != fingerprint;
  if (topologyChanged || timeJump) {
    InvalidateHistory();
    state.temporalSequenceIndex = 0;
    if (topologyChanged)
      ++state.stats.temporalTopologyResets;
    if (timeJump)
      ++state.stats.temporalTimeJumpResets;
  }
  state.topologyFingerprint = fingerprint;
  state.previousTimeSeconds = currentTime;
  state.previousTimeValid = true;
  constants[9] = state.stats.historyValid ? 1u : 0u;
  constants[23] = source.useExternalTemporalSampleIndex
      ? source.temporalSampleIndex & 7u
      : state.temporalSequenceIndex & 7u;
  state.stats.appliedCloudSampleCount = constants[64];
  state.stats.appliedLightShaftSampleCount = constants[65];
  state.stats.appliedCloudShadowSampleCount = constants[66];
  state.stats.appliedCloudMaximumDistance = std::bit_cast<float>(constants[68]);
  state.stats.appliedLightShaftMaximumDistance = std::bit_cast<float>(constants[69]);
  state.stats.appliedCloudShadowStrength = std::bit_cast<float>(constants[70]);
  state.stats.appliedLightShaftShadowStrength = std::bit_cast<float>(constants[71]);
  state.stats.appliedSettingsFingerprint = fingerprint;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  state.stats.appliedTestMutation = constants[19];
  state.stats.appliedFixturePatternHash = constants[19] == 9
      ? VolumetricAtmosphereUpscaleFixturePatternHash3D(
            state.stats.outputWidth, state.stats.outputHeight)
      : 0;
  state.stats.appliedFixtureWidth = constants[19] == 9 ? state.stats.outputWidth : 0;
  state.stats.appliedFixtureHeight = constants[19] == 9 ? state.stats.outputHeight : 0;
  state.stats.appliedResetHistory = source.resetHistory;
  state.stats.appliedHistoryValid = constants[9] != 0;
  state.stats.appliedTemporalSampleIndex = constants[23];
#endif
  std::memcpy(state.constantMapped + std::size_t(frameSlice) * ConstantSliceBytes,
              constants.data(), ConstantSliceBytes);
  commandList->SetComputeRootConstantBufferView(
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
      7,
#else
      6,
#endif
      state.constantBuffer->GetGPUVirtualAddress() +
             std::uint64_t(frameSlice) * ConstantSliceBytes);
  const UINT groupsX = (state.stats.outputWidth + 7) / 8;
  const UINT groupsY = (state.stats.outputHeight + 7) / 8;
  commandList->Dispatch(groupsX, groupsY, 1);
  D3D12_RESOURCE_BARRIER barriers[4]{};
  barriers[0].Type = barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barriers[0].UAV.pResource = state.textures[current].Get();
  barriers[1].UAV.pResource = state.segmentTextures[current].Get();
  barriers[2] = toWrite[0];
  barriers[3] = toWrite[1];
  std::swap(barriers[2].Transition.StateBefore, barriers[2].Transition.StateAfter);
  std::swap(barriers[3].Transition.StateBefore, barriers[3].Transition.StateAfter);
  commandList->ResourceBarrier(4, barriers);
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  if (source.enableTemporalDiagnostics) {
  D3D12_RESOURCE_BARRIER counterToReadback{};
  counterToReadback.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  counterToReadback.Transition = {
      state.temporalCounters[frameSlice].Get(),
      D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_SOURCE};
  commandList->ResourceBarrier(1, &counterToReadback);
  commandList->CopyBufferRegion(state.temporalReadbacks[frameSlice].Get(), 0,
                                state.temporalCounters[frameSlice].Get(), 0, 16);
  state.temporalCounterStates[frameSlice] = D3D12_RESOURCE_STATE_COPY_SOURCE;
  state.temporalReadbackPending[frameSlice] = true;
  }
#endif
  ++state.stats.dispatches;
  state.stats.dispatchedThreadGroups += std::uint64_t(groupsX) * groupsY;
  state.stats.historyValid = true;
  if (source.resetHistory || topologyChanged || timeJump)
    ++state.stats.temporalResetFrames;
  else if (constants[9])
    ++state.stats.temporalReprojectionFrames;
  state.writeIndex ^= 1u;
  if (!source.useExternalTemporalSampleIndex)
    state.temporalSequenceIndex = (state.temporalSequenceIndex + 1u) & 7u;
  return true;
}

void VolumetricAtmospherePassD3D12::InvalidateHistory() noexcept {
  if (!impl_)
    return;
  if (impl_->stats.historyValid)
    ++impl_->stats.historyResets;
  impl_->stats.historyValid = false;
  // 歷程失效後從固定輸出槽重新開始，避免擷取結果受先前呈現幀數的奇偶性影響。
  impl_->writeIndex = 0;
  impl_->previousTimeValid = false;
}

bool VolumetricAtmospherePassD3D12::CollectTemporalDiagnostics(
    std::uint32_t frameSlice) {
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  if (!impl_)
    return false;
  auto &state = *impl_;
  frameSlice %= ConstantFrameSlices;
  if (!state.temporalReadbackPending[frameSlice])
    return false;
  const std::uint32_t *values{};
  const D3D12_RANGE readRange{0, 16};
  if (FAILED(state.temporalReadbacks[frameSlice]->Map(
          0, &readRange,
          reinterpret_cast<void **>(const_cast<std::uint32_t **>(&values)))))
    return false;
  state.stats.temporalAcceptedPixels += values[0];
  state.stats.temporalRejectedPixels += values[1];
  state.stats.temporalInvalidPixels += values[2];
  ++state.stats.temporalDiagnosticFrames;
  const D3D12_RANGE noWrite{};
  state.temporalReadbacks[frameSlice]->Unmap(0, &noWrite);
  state.temporalReadbackPending[frameSlice] = false;
  return true;
#else
  (void)frameSlice;
  return false;
#endif
}

void VolumetricAtmospherePassD3D12::Shutdown() noexcept {
  if (!impl_)
    return;
  for (auto &texture : impl_->textures)
    if (texture) {
      texture.Reset();
      ++impl_->stats.resourceReleases;
    }
  for (auto &texture : impl_->segmentTextures)
    if (texture) {
      texture.Reset();
      ++impl_->stats.resourceReleases;
    }
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  for (auto &resource : impl_->temporalCounters)
    if (resource) {
      resource.Reset();
      ++impl_->stats.resourceReleases;
    }
  for (auto &resource : impl_->temporalReadbacks)
    if (resource) {
      resource.Reset();
      ++impl_->stats.resourceReleases;
    }
  if (impl_->temporalCounterZero) {
    if (impl_->temporalCounterZeroMapped)
      impl_->temporalCounterZero->Unmap(0, nullptr);
    impl_->temporalCounterZeroMapped = nullptr;
    impl_->temporalCounterZero.Reset();
    ++impl_->stats.resourceReleases;
  }
#endif
  if (impl_->blueNoiseBuffer) {
    impl_->blueNoiseBuffer.Reset();
    ++impl_->stats.resourceReleases;
  }
  impl_->stats.blueNoiseResidentBytes = 0;
  impl_->stats.residentBytes = 0;
  impl_->stats.outputWidth = 0;
  impl_->stats.outputHeight = 0;
  impl_->stats.historyValid = false;
  impl_->pipeline.Reset();
  impl_->rootSignature.Reset();
  impl_->descriptorHeap.Reset();
  if (impl_->constantBuffer && impl_->constantMapped)
    impl_->constantBuffer->Unmap(0, nullptr);
  impl_->constantMapped = nullptr;
  impl_->constantBuffer.Reset();
  impl_->device.Reset();
}

ID3D12Resource *VolumetricAtmospherePassD3D12::OutputResource() const noexcept {
  if (!impl_ || !impl_->stats.dispatches)
    return nullptr;
  return impl_->textures[impl_->writeIndex ^ 1u].Get();
}

const VolumetricAtmospherePassStats3D &
VolumetricAtmospherePassD3D12::Stats() const noexcept {
  static constexpr VolumetricAtmospherePassStats3D Empty{};
  return impl_ ? impl_->stats : Empty;
}

} // namespace Aevrynd
