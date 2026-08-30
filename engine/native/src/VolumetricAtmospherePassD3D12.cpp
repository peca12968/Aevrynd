#include "Aevrynd/VolumetricAtmospherePassD3D12.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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

float FiniteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float ClampFinite(float value, float fallback, float minimum,
                  float maximum) noexcept {
  return (std::clamp)(FiniteOr(value, fallback), minimum, maximum);
}

float NormalizeScale(float value) noexcept {
  return ClampFinite(value, .5f, .25f, .5f);
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
  std::array<ComPtr<ID3D12Resource>, 2> textures;
  VolumetricAtmospherePassStats3D stats{};
  std::uint32_t fullWidth{}, fullHeight{}, writeIndex{};
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

    std::uint64_t releases{};
    for (const auto &texture : textures)
      releases += texture ? 1u : 0u;
    stats.resourceReleases += releases;
    textures = std::move(replacement);
    stats.resourceCreates += 2;
    fullWidth = width;
    fullHeight = height;
    resolutionScale = normalizedScale;
    stats.outputWidth = scaledWidth;
    stats.outputHeight = scaledHeight;
    const auto allocation = device->GetResourceAllocationInfo(0, 1, &description);
    stats.residentBytes = allocation.SizeInBytes <=
                                  (std::numeric_limits<std::size_t>::max)() / 2
                              ? std::size_t(allocation.SizeInBytes) * 2
                              : (std::numeric_limits<std::size_t>::max)();
    stats.historyValid = false;
    writeIndex = 0;
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

  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors = 4;
  heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device->CreateDescriptorHeap(
          &heap, IID_PPV_ARGS(&candidate->descriptorHeap))))
    return false;
  candidate->descriptorIncrement = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_DESCRIPTOR_RANGE ranges[2]{};
  ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  ranges[0].NumDescriptors = 1;
  ranges[0].BaseShaderRegister = 0;
  ranges[0].OffsetInDescriptorsFromTableStart = 0;
  ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[1].NumDescriptors = 1;
  ranges[1].BaseShaderRegister = 0;
  ranges[1].OffsetInDescriptorsFromTableStart = 0;
  D3D12_ROOT_PARAMETER parameters[3]{};
  parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[0].DescriptorTable = {1, &ranges[0]};
  parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[1].DescriptorTable = {1, &ranges[1]};
  parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[2].Constants = {0, 0, 16};
  D3D12_ROOT_SIGNATURE_DESC rootDescription{};
  rootDescription.NumParameters = 3;
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
  if (!impl_ || !commandList || !source.enabled ||
      NormalizeScale(source.resolutionScale) != impl_->resolutionScale) {
    if (impl_)
      ++impl_->stats.skippedFrames;
    return false;
  }
  auto &state = *impl_;
  if (source.resetHistory)
    InvalidateHistory();
  const UINT current = state.writeIndex;
  const UINT previous = current ^ 1u;
  float sunX = FiniteOr(source.sunDirectionX, .35f);
  float sunY = FiniteOr(source.sunDirectionY, .82f);
  float sunZ = FiniteOr(source.sunDirectionZ, -.44f);
  const float sunLengthSquared = sunX * sunX + sunY * sunY + sunZ * sunZ;
  if (!std::isfinite(sunLengthSquared) || sunLengthSquared < 1e-8f) {
    sunX = .35f;
    sunY = .82f;
    sunZ = -.44f;
  }
  D3D12_RESOURCE_BARRIER toWrite{};
  toWrite.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toWrite.Transition = {state.textures[current].Get(),
                        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
  commandList->ResourceBarrier(1, &toWrite);
  ID3D12DescriptorHeap *heaps[] = {state.descriptorHeap.Get()};
  commandList->SetDescriptorHeaps(1, heaps);
  commandList->SetComputeRootSignature(state.rootSignature.Get());
  commandList->SetPipelineState(state.pipeline.Get());
  const auto gpuStart = state.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
  commandList->SetComputeRootDescriptorTable(
      0, Offset(gpuStart, state.descriptorIncrement, current * 2 + 1));
  commandList->SetComputeRootDescriptorTable(
      1, Offset(gpuStart, state.descriptorIncrement, previous * 2));
  const std::array<std::uint32_t, 16> constants{
      state.stats.outputWidth,
      state.stats.outputHeight,
      (std::min)(std::uint32_t(source.phase), 2u),
      state.stats.historyValid ? 1u : 0u,
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.historyWeight, .88f, 0.f, .98f)),
      std::bit_cast<std::uint32_t>(FiniteOr(source.timeSeconds, 0.f)),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.cloudCoverage, .48f, .05f, .95f)),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.cloudDensity, 1.08f, 0.f, 4.f)),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.cloudErosion, .28f, 0.f, .8f)),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.cloudWindSpeed, 18.f, -200.f, 200.f)),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.lightShaftIntensity, .68f, 0.f, 4.f)),
      std::bit_cast<std::uint32_t>(
          ClampFinite(source.lightShaftAnisotropy, .72f, -.9f, .9f)),
      std::bit_cast<std::uint32_t>(sunX),
      std::bit_cast<std::uint32_t>(sunY),
      std::bit_cast<std::uint32_t>(sunZ),
      source.temporalSampleIndex & 7u};
  commandList->SetComputeRoot32BitConstants(2, UINT(constants.size()),
                                            constants.data(), 0);
  const UINT groupsX = (state.stats.outputWidth + 7) / 8;
  const UINT groupsY = (state.stats.outputHeight + 7) / 8;
  commandList->Dispatch(groupsX, groupsY, 1);
  D3D12_RESOURCE_BARRIER barriers[2]{};
  barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barriers[0].UAV.pResource = state.textures[current].Get();
  barriers[1] = toWrite;
  std::swap(barriers[1].Transition.StateBefore,
            barriers[1].Transition.StateAfter);
  commandList->ResourceBarrier(2, barriers);
  ++state.stats.dispatches;
  state.stats.dispatchedThreadGroups += std::uint64_t(groupsX) * groupsY;
  state.stats.historyValid = true;
  state.writeIndex ^= 1u;
  return true;
}

void VolumetricAtmospherePassD3D12::InvalidateHistory() noexcept {
  if (!impl_)
    return;
  if (impl_->stats.historyValid)
    ++impl_->stats.historyResets;
  impl_->stats.historyValid = false;
}

void VolumetricAtmospherePassD3D12::Shutdown() noexcept {
  if (!impl_)
    return;
  for (auto &texture : impl_->textures)
    if (texture) {
      texture.Reset();
      ++impl_->stats.resourceReleases;
    }
  impl_->stats.residentBytes = 0;
  impl_->stats.outputWidth = 0;
  impl_->stats.outputHeight = 0;
  impl_->stats.historyValid = false;
  impl_->pipeline.Reset();
  impl_->rootSignature.Reset();
  impl_->descriptorHeap.Reset();
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
