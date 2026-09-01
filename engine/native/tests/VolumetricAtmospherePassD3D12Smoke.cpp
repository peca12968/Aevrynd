#include "Aevrynd/VolumetricAtmospherePassD3D12.h"
#include <Windows.h>
#include "VolumetricAtmosphereComputeShader.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace Aevrynd;

namespace {
void Require(HRESULT result, const char *message) {
  if (FAILED(result))
    throw std::runtime_error(message);
}

float HalfToFloat(std::uint16_t half) {
  const std::uint32_t sign = std::uint32_t(half & 0x8000u) << 16;
  std::uint32_t exponent = (half >> 10) & 0x1fu;
  std::uint32_t mantissa = half & 0x3ffu;
  std::uint32_t bits{};
  if (!exponent) {
    if (!mantissa)
      bits = sign;
    else {
      exponent = 1;
      while ((mantissa & 0x400u) == 0) {
        mantissa <<= 1;
        --exponent;
      }
      mantissa &= 0x3ffu;
      bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    }
  } else if (exponent == 31)
    bits = sign | 0x7f800000u | (mantissa << 13);
  else
    bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
  return std::bit_cast<float>(bits);
}

struct Capture {
  std::uint64_t hash{14695981039346656037ull};
  double average{}, variance{};
  double nonzeroFraction{}, neutralWhiteFraction{}, maximumVerticalRunFraction{},
      longColumnFraction{};
  std::uint64_t gpuNanoseconds{};
  std::uint64_t temporalAccepted{}, temporalRejected{}, temporalInvalid{};
};

void WriteBmp(const std::filesystem::path &path, std::uint32_t width,
              std::uint32_t height, std::uint32_t rowPitch,
              const std::byte *bytes) {
  std::ofstream output(path, std::ios::binary);
  const auto write16 = [&](std::uint16_t value) {
    output.put(char(value & 0xffu));
    output.put(char(value >> 8));
  };
  const auto write32 = [&](std::uint32_t value) {
    write16(std::uint16_t(value & 0xffffu));
    write16(std::uint16_t(value >> 16));
  };
  const std::uint32_t imageBytes = width * height * 4u;
  output.put('B');
  output.put('M');
  write32(54u + imageBytes);
  write32(0);
  write32(54);
  write32(40);
  write32(width);
  write32(height);
  write16(1);
  write16(32);
  write32(0);
  write32(imageBytes);
  write32(2835);
  write32(2835);
  write32(0);
  write32(0);
  for (std::uint32_t fileY = 0; fileY < height; ++fileY) {
    const std::uint32_t y = height - 1 - fileY;
    const auto *row = reinterpret_cast<const std::uint16_t *>(
        bytes + std::size_t(y) * rowPitch);
    for (std::uint32_t x = 0; x < width; ++x) {
      std::array<std::uint8_t, 3> srgb{};
      for (std::uint32_t channel = 0; channel < 3; ++channel) {
        const float linear = (std::max)(0.f, HalfToFloat(row[x * 4 + channel]));
        const float mapped = std::pow(linear / (1.f + linear), 1.f / 2.2f);
        srgb[channel] =
            std::uint8_t((std::clamp)(mapped, 0.f, 1.f) * 255.f);
      }
      output.put(char(srgb[2]));
      output.put(char(srgb[1]));
      output.put(char(srgb[0]));
      output.put(static_cast<char>(-1));
    }
  }
}
} // namespace

int main() {
  try {
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
      debug->EnableDebugLayer();
      ComPtr<ID3D12Debug1> debug1;
      if (SUCCEEDED(debug.As(&debug1)))
        debug1->SetEnableGPUBasedValidation(TRUE);
    }
    ComPtr<IDXGIFactory6> factory;
    Require(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
                               IID_PPV_ARGS(&factory)),
            "Factory creation failed");
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0; SUCCEEDED(factory->EnumAdapterByGpuPreference(
             index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             IID_PPV_ARGS(&adapter)));
         ++index) {
      DXGI_ADAPTER_DESC1 description{};
      adapter->GetDesc1(&description);
      if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
          SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                      __uuidof(ID3D12Device), nullptr)))
        break;
      adapter.Reset();
    }
    if (!adapter)
      Require(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),
              "No D3D12 adapter");
    ComPtr<ID3D12Device> device;
    Require(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                              IID_PPV_ARGS(&device)),
            "Device creation failed");
    ComPtr<ID3D12InfoQueue> infoQueue;
    device.As(&infoQueue);

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    Require(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)),
            "Queue creation failed");
    ComPtr<ID3D12CommandAllocator> allocator;
    Require(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(&allocator)),
            "Allocator creation failed");
    ComPtr<ID3D12GraphicsCommandList> list;
    Require(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      allocator.Get(), nullptr,
                                      IID_PPV_ARGS(&list)),
            "Command list creation failed");
    Require(list->Close(), "Initial close failed");
    ComPtr<ID3D12Fence> fence;
    Require(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
            "Fence creation failed");
    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
      return 2;
    std::uint64_t fenceValue{};
    auto wait = [&] {
      const auto value = ++fenceValue;
      Require(queue->Signal(fence.Get(), value), "Signal failed");
      if (fence->GetCompletedValue() < value) {
        Require(fence->SetEventOnCompletion(value, eventHandle),
                "Fence event failed");
        WaitForSingleObject(eventHandle, INFINITE);
      }
    };

    D3D12_QUERY_HEAP_DESC queryDescription{};
    queryDescription.Count = 2;
    queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    ComPtr<ID3D12QueryHeap> queryHeap;
    Require(device->CreateQueryHeap(&queryDescription,
                                    IID_PPV_ARGS(&queryHeap)),
            "Timestamp heap failed");
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC timestampDescription{};
    timestampDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    timestampDescription.Width = 16;
    timestampDescription.Height = 1;
    timestampDescription.DepthOrArraySize = 1;
    timestampDescription.MipLevels = 1;
    timestampDescription.SampleDesc.Count = 1;
    timestampDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> timestampReadback;
    Require(device->CreateCommittedResource(
                &readbackHeap, D3D12_HEAP_FLAG_NONE, &timestampDescription,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&timestampReadback)),
            "Timestamp readback failed");
    UINT64 timestampFrequency{};
    Require(queue->GetTimestampFrequency(&timestampFrequency),
            "Timestamp frequency failed");

    VolumetricAtmospherePassD3D12 pass;
    const auto shaderBytes = std::as_bytes(
        std::span{VolumetricAtmosphereComputeShader});
    if (!pass.Initialize(device.Get(), shaderBytes, 960, 540, .5f))
      return 3;
    const auto captureDirectory =
        std::filesystem::current_path() / "volumetric-atmosphere-captures";
    std::filesystem::create_directories(captureDirectory);

    auto execute = [&](VolumetricAtmospherePassSettings3D settings,
                       const char *captureName) {
      const auto acceptedBefore = pass.Stats().temporalAcceptedPixels;
      const auto rejectedBefore = pass.Stats().temporalRejectedPixels;
      const auto invalidBefore = pass.Stats().temporalInvalidPixels;
      Require(allocator->Reset(), "Allocator reset failed");
      Require(list->Reset(allocator.Get(), nullptr), "List reset failed");
      list->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
      const bool recorded = pass.Record(list.Get(), settings);
      ComPtr<ID3D12Resource> pixelReadback;
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
      UINT rows{};
      UINT64 rowBytes{}, totalBytes{};
      if (recorded) {
        const auto description = pass.OutputResource()->GetDesc();
        device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, &rows,
                                      &rowBytes, &totalBytes);
        D3D12_RESOURCE_DESC buffer = timestampDescription;
        buffer.Width = totalBytes;
        const HRESULT pixelReadbackResult=device->CreateCommittedResource(
                    &readbackHeap, D3D12_HEAP_FLAG_NONE, &buffer,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                    IID_PPV_ARGS(&pixelReadback));
        if(FAILED(pixelReadbackResult)){std::cerr<<"Pixel readback HRESULT=0x"<<std::hex<<std::uint32_t(pixelReadbackResult)<<" removed=0x"<<std::uint32_t(device->GetDeviceRemovedReason())<<std::dec<<'\n';if(infoQueue){const auto count=infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();for(std::uint64_t index=0;index<count;++index){SIZE_T bytes{};infoQueue->GetMessage(index,nullptr,&bytes);std::vector<std::byte> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());if(SUCCEEDED(infoQueue->GetMessage(index,message,&bytes)))std::cerr<<"D3D id="<<message->ID<<" severity="<<message->Severity<<" text="<<message->pDescription<<'\n';}}}
        Require(pixelReadbackResult,"Pixel readback failed");
        D3D12_RESOURCE_BARRIER copy{};
        copy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        copy.Transition = {pass.OutputResource(),
                           D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_COPY_SOURCE};
        list->ResourceBarrier(1, &copy);
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = pixelReadback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = pass.OutputResource();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        std::swap(copy.Transition.StateBefore, copy.Transition.StateAfter);
        list->ResourceBarrier(1, &copy);
      }
      list->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
      list->ResolveQueryData(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2,
                             timestampReadback.Get(), 0);
      Require(list->Close(), "Command list close failed");
      ID3D12CommandList *lists[] = {list.Get()};
      queue->ExecuteCommandLists(1, lists);
      wait();
      (void)pass.CollectTemporalDiagnostics(settings.frameSlice);
      Capture capture{};
      capture.temporalAccepted = pass.Stats().temporalAcceptedPixels - acceptedBefore;
      capture.temporalRejected = pass.Stats().temporalRejectedPixels - rejectedBefore;
      capture.temporalInvalid = pass.Stats().temporalInvalidPixels - invalidBefore;
      std::uint64_t *timestamps{};
      D3D12_RANGE timestampRange{0, 16};
      Require(timestampReadback->Map(0, &timestampRange,
                                     reinterpret_cast<void **>(&timestamps)),
              "Timestamp map failed");
      capture.gpuNanoseconds = timestampFrequency && timestamps[1] >= timestamps[0]
                                   ? (timestamps[1] - timestamps[0]) *
                                         1000000000ull / timestampFrequency
                                   : 0;
      D3D12_RANGE noWrite{};
      timestampReadback->Unmap(0, &noWrite);
      if (!recorded)
        return capture;
      const std::byte *pixels{};
      D3D12_RANGE pixelRange{0, SIZE_T(totalBytes)};
      Require(pixelReadback->Map(0, &pixelRange,
                                 reinterpret_cast<void **>(
                                     const_cast<std::byte **>(&pixels))),
              "Pixel map failed");
      const auto width = pass.Stats().outputWidth;
      const auto height = pass.Stats().outputHeight;
      double sum{}, squareSum{};
      std::size_t nonzero{}, neutralWhite{};
      std::vector<std::uint32_t> currentRuns(width), maximumRuns(width);
      for (std::uint32_t y = 0; y < height; ++y) {
        const auto *row = reinterpret_cast<const std::uint16_t *>(
            pixels + std::size_t(y) * footprint.Footprint.RowPitch);
        for (std::uint32_t x = 0; x < width; ++x) {
          const float red=(std::max)(0.f,HalfToFloat(row[x*4])),green=(std::max)(0.f,HalfToFloat(row[x*4+1])),blue=(std::max)(0.f,HalfToFloat(row[x*4+2]));
          const double luminance=red*.2126+green*.7152+blue*.0722;
          sum += luminance;
          squareSum += luminance * luminance;
          nonzero += luminance > 1e-5;
          const float maximum=(std::max)({red,green,blue}),minimum=(std::min)({red,green,blue});
          const bool white=maximum>.8f&&(maximum-minimum)/(std::max)(maximum,1e-5f)<=.03f;
          neutralWhite+=white;
          const bool column=maximum>.6f&&(maximum-minimum)/(std::max)(maximum,1e-5f)<=.15f;
          currentRuns[x]=column?currentRuns[x]+1u:0u;
          maximumRuns[x]=(std::max)(maximumRuns[x],currentRuns[x]);
          for (std::uint32_t channel = 0; channel < 4; ++channel) {
            const auto value = row[x * 4 + channel];
            capture.hash = (capture.hash ^ (value & 0xffu)) * 1099511628211ull;
            capture.hash = (capture.hash ^ (value >> 8)) * 1099511628211ull;
          }
        }
      }
      const double count = double(width) * height;
      capture.average = sum / count;
      capture.variance = squareSum / count - capture.average * capture.average;
      capture.nonzeroFraction=double(nonzero)/count;
      capture.neutralWhiteFraction=double(neutralWhite)/count;
      const auto maximumRun=*std::ranges::max_element(maximumRuns);
      capture.maximumVerticalRunFraction=double(maximumRun)/height;
      capture.longColumnFraction=double(std::ranges::count_if(maximumRuns,[&](std::uint32_t run){return run>height*.18;}))/width;
      WriteBmp(captureDirectory / captureName, width, height,
               footprint.Footprint.RowPitch, pixels);
      pixelReadback->Unmap(0, &noWrite);
      return capture;
    };

    VolumetricAtmospherePassSettings3D settings{};
    settings.enableTemporalDiagnostics = true;
    // 單位投影只決定射線方向；雲層仍使用公里級世界座標以覆蓋程序噪音。
    settings.viewportWidth = 960.f;
    settings.viewportHeight = 540.f;
    settings.resetHistory = true;
    const auto day = execute(settings, "day.bmp");
    settings.resetHistory = false;
    Capture dayTemporal{};
    for (std::uint32_t sample = 1; sample < 8; ++sample) {
      settings.temporalSampleIndex = sample;
      settings.frameSlice = sample % 3;
      dayTemporal = execute(settings, "day-temporal.bmp");
    }
    settings.resetHistory = true;
    settings.temporalSampleIndex = 0;
    const auto dayRepeat = execute(settings, "day-repeat.bmp");
    settings.phase = VolumetricAtmospherePhase3D::Dusk;
    const auto dusk = execute(settings, "dusk.bmp");
    settings.phase = VolumetricAtmospherePhase3D::Night;
    const auto night = execute(settings, "night.bmp");
    settings.phase = static_cast<VolumetricAtmospherePhase3D>(99);
    settings.historyWeight = (std::numeric_limits<float>::quiet_NaN)();
    settings.cloudCoverage = (std::numeric_limits<float>::infinity)();
    settings.cloudDensity = -(std::numeric_limits<float>::infinity)();
    settings.lightShaftIntensity = (std::numeric_limits<float>::infinity)();
    settings.heightFogDensity = (std::numeric_limits<float>::infinity)();
    settings.heightFogMaximumOpacity = (std::numeric_limits<float>::infinity)();
    settings.sunDirectionX = settings.sunDirectionY = settings.sunDirectionZ = 0;
    const auto hostile = execute(settings, "hostile-normalized.bmp");
    VolumetricAtmospherePassSettings3D temporalReset{};
    temporalReset.enableTemporalDiagnostics = true;
    temporalReset.viewportWidth = 960.f;
    temporalReset.viewportHeight = 540.f;
    temporalReset.resetHistory = true;
    temporalReset.timeSeconds = 1.f;
    execute(temporalReset, "temporal-reset.bmp");
    temporalReset.resetHistory = false;
    temporalReset.frameSlice = 1;
    temporalReset.timeSeconds = 1.1f;
    execute(temporalReset, "temporal-history.bmp");
    const auto timeJumpsBefore = pass.Stats().temporalTimeJumpResets;
    temporalReset.frameSlice = 2;
    temporalReset.timeSeconds = 20.f;
    execute(temporalReset, "temporal-time-jump.bmp");
    const auto topologyBefore = pass.Stats().temporalTopologyResets;
    temporalReset.frameSlice = 0;
    temporalReset.timeSeconds = 20.1f;
    temporalReset.phase = VolumetricAtmospherePhase3D::Dusk;
    execute(temporalReset, "temporal-phase-reset.bmp");
    if (pass.Stats().temporalTimeJumpResets != timeJumpsBefore + 1 ||
        pass.Stats().temporalTopologyResets != topologyBefore + 1)
      return 10;
    const std::array routePhases{VolumetricAtmospherePhase3D::Day,
                                 VolumetricAtmospherePhase3D::Dusk,
                                 VolumetricAtmospherePhase3D::Night};
    const std::array routeNames{"day","dusk","night"};
    for(std::size_t routePhase=0;routePhase<routePhases.size();++routePhase){
      VolumetricAtmospherePassSettings3D route{};route.enableTemporalDiagnostics=true;route.viewportWidth=960;route.viewportHeight=540;route.phase=routePhases[routePhase];route.resetHistory=true;route.frameSlice=std::uint32_t(routePhase%3);
      route.cloudDensity=0;route.lightShaftIntensity=0;const auto bothOff=execute(route,(std::string("route-")+routeNames[routePhase]+"-off.bmp").c_str());
      route.cloudDensity=1.08f;route.lightShaftIntensity=0;const auto cloudOnly=execute(route,(std::string("route-")+routeNames[routePhase]+"-cloud.bmp").c_str());
      route.cloudDensity=0;route.lightShaftIntensity=.68f;const auto shaftOnly=execute(route,(std::string("route-")+routeNames[routePhase]+"-shaft.bmp").c_str());
      route.cloudDensity=1.08f;route.lightShaftIntensity=.68f;const auto both=execute(route,(std::string("route-")+routeNames[routePhase]+"-both.bmp").c_str());
      const auto repeated=execute(route,(std::string("route-")+routeNames[routePhase]+"-repeat.bmp").c_str());
      const double whiteLimit=routePhase==0?.03:(routePhase==1?.01:.001);
      const bool routeValid=bothOff.nonzeroFraction==0&&cloudOnly.nonzeroFraction>=.05&&cloudOnly.nonzeroFraction<=.8&&cloudOnly.variance>1e-7&&shaftOnly.nonzeroFraction>0&&both.nonzeroFraction>0&&both.hash==repeated.hash&&both.neutralWhiteFraction<=whiteLimit&&both.maximumVerticalRunFraction<=.18&&both.longColumnFraction<=.02&&both.gpuNanoseconds<=1500000ull&&both.gpuNanoseconds<=bothOff.gpuNanoseconds+750000ull;
      std::cout<<"route phase="<<routeNames[routePhase]<<" cloudChanged="<<cloudOnly.nonzeroFraction<<" cloudVariance="<<cloudOnly.variance<<" shaftChanged="<<shaftOnly.nonzeroFraction<<" white="<<both.neutralWhiteFraction<<" vertical="<<both.maximumVerticalRunFraction<<" columns="<<both.longColumnFraction<<" gpuUs="<<bothOff.gpuNanoseconds/1000<<'/'<<both.gpuNanoseconds/1000<<" deterministic="<<(both.hash==repeated.hash)<<'\n';
      if(!routeValid)return 9;
    }
    const auto dispatchesBeforeDisabled = pass.Stats().dispatches;
    const auto groupsBeforeDisabled = pass.Stats().dispatchedThreadGroups;
    settings.enabled = false;
    const auto disabled = execute(settings, "disabled.bmp");
    settings.enabled = true;
    settings.resolutionScale = .25f;
    const auto mismatch = execute(settings, "mismatch.bmp");
    if (pass.Stats().dispatches != dispatchesBeforeDisabled ||
        pass.Stats().dispatchedThreadGroups != groupsBeforeDisabled ||
        pass.Stats().skippedFrames != 2 || disabled.hash !=
            14695981039346656037ull ||
        mismatch.hash != 14695981039346656037ull)
      return 4;
    std::cout << "probe day=" << day.average << '/' << day.variance << '/' << day.hash
              << " temporal=" << dayTemporal.average << '/' << dayTemporal.hash
              << " repeat=" << dayRepeat.average << '/' << dayRepeat.hash
              << " dusk=" << dusk.average << '/' << dusk.variance << '/' << dusk.hash
              << " night=" << night.average << '/' << night.variance << '/' << night.hash
              << " hostile=" << hostile.average << '/' << hostile.variance
              << " temporalCounters=" << day.temporalAccepted << '/'
              << dayTemporal.temporalAccepted << '/' << dayTemporal.temporalRejected
              << '\n';
    const bool temporalCaptureProvenance =
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
        day.temporalAccepted == 0 &&
        dayTemporal.temporalAccepted + dayTemporal.temporalRejected > 0;
#else
        true;
#endif
    if (!day.gpuNanoseconds || !dusk.gpuNanoseconds || !night.gpuNanoseconds ||
        day.hash == dayTemporal.hash || day.hash != dayRepeat.hash ||
        !temporalCaptureProvenance ||
        day.hash == dusk.hash || dusk.hash == night.hash ||
        day.average <= night.average || day.variance <= .000001 ||
        dusk.variance <= .000001 || night.variance <= .000001 ||
        !std::isfinite(hostile.average) || hostile.variance <= .000001)
      return 5;

    wait();
    const auto releasesBeforeResize = pass.Stats().resourceReleases;
    if (!pass.Resize(640, 360, .25f) || pass.Stats().outputWidth != 160 ||
        pass.Stats().outputHeight != 90 ||
        pass.Stats().resourceReleases != releasesBeforeResize + 4 ||
        pass.Stats().historyValid)
      return 6;
    settings.resolutionScale = .25f;
    settings.resetHistory = true;
    settings.phase = VolumetricAtmospherePhase3D::Day;
    const auto resized = execute(settings, "resized-day.bmp");
    if (!resized.hash || resized.variance <= .000001)
      return 7;
    wait();
    const auto resourcesCreated = pass.Stats().resourceCreates;
    const auto releasesBeforeShutdown = pass.Stats().resourceReleases;
    const auto dispatches = pass.Stats().dispatches;
    const auto groups = pass.Stats().dispatchedThreadGroups;
    const auto resident = pass.Stats().residentBytes;
    const auto temporalAccepted=pass.Stats().temporalAcceptedPixels,temporalRejected=pass.Stats().temporalRejectedPixels,temporalInvalid=pass.Stats().temporalInvalidPixels,temporalDiagnosticFrames=pass.Stats().temporalDiagnosticFrames;
    std::cout << "temporal counters before shutdown accepted=" << temporalAccepted
              << " rejected=" << temporalRejected << " invalid=" << temporalInvalid
              << " frames=" << temporalDiagnosticFrames << '\n';
    // 無場景深度的 pass smoke 僅驗證 counter 管線；完整三分類由 renderer integration 強制驗證。
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    if (!(temporalAccepted + temporalRejected + temporalInvalid) ||
        temporalDiagnosticFrames < 8)
      return 8;
#else
    if (temporalAccepted || temporalRejected || temporalInvalid ||
        temporalDiagnosticFrames || pass.Stats().temporalDiagnosticsAvailable)
      return 8;
#endif
    pass.Shutdown();
    if (pass.Stats().residentBytes || pass.OutputResource() ||
        pass.Stats().resourceReleases != releasesBeforeShutdown +
            (AEVRYND_INTERNAL_RENDERER_TEST_HOOKS ? 12 : 5) ||
        resourcesCreated != pass.Stats().resourceReleases)
      return 8;

    // 同一 wrapper 在裝置重建後可重新建立 PSO、descriptor 與 history。
    if (!pass.Initialize(device.Get(), shaderBytes, 320, 180, .25f))
      return 8;
    settings = {};
    settings.resolutionScale = .25f;
    settings.resetHistory = true;
    const auto rebuilt = execute(settings, "rebuilt-device-state.bmp");
    if (!rebuilt.hash || !pass.Stats().historyValid ||
        pass.Stats().resourceCreates !=
            (AEVRYND_INTERNAL_RENDERER_TEST_HOOKS ? 12 : 5) ||
        pass.Stats().dispatches != 1)
      return 8;
    wait();
    pass.Shutdown();
    if (pass.Stats().residentBytes || pass.OutputResource() ||
        pass.Stats().resourceReleases !=
            (AEVRYND_INTERNAL_RENDERER_TEST_HOOKS ? 12 : 5))
      return 8;

    std::uint64_t gpuErrors{}, gpuWarnings{};
    if (infoQueue) {
      const auto count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
      for (std::uint64_t index = 0; index < count; ++index) {
        SIZE_T bytes{};
        infoQueue->GetMessage(index, nullptr, &bytes);
        std::vector<std::byte> storage(bytes);
        auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        if (SUCCEEDED(infoQueue->GetMessage(index, message, &bytes))) {
          gpuErrors += message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
                       message->Severity == D3D12_MESSAGE_SEVERITY_ERROR;
          gpuWarnings += message->Severity == D3D12_MESSAGE_SEVERITY_WARNING;
        }
      }
    }
    const auto removed = device->GetDeviceRemovedReason();
    std::cout << "VolumetricAtmospherePassD3D12 PASS dispatches=" << dispatches
              << " groups=" << groups << " residentBytes=" << resident
              << " dayGpuUs=" << day.gpuNanoseconds / 1000
              << " duskGpuUs=" << dusk.gpuNanoseconds / 1000
              << " nightGpuUs=" << night.gpuNanoseconds / 1000
              << " gpuErrors=" << gpuErrors << " gpuWarnings=" << gpuWarnings
              << " dayHash=" << day.hash << " duskHash=" << dusk.hash
              << " nightHash=" << night.hash
              << " temporalAccepted=" << temporalAccepted
              << " temporalRejected=" << temporalRejected
              << " temporalInvalid=" << temporalInvalid
              << " temporalDiagnosticFrames=" << temporalDiagnosticFrames
              << " captures=" << captureDirectory.string() << '\n';
    CloseHandle(eventHandle);
    return gpuErrors || gpuWarnings || FAILED(removed) ? 9 : 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 10;
  }
}
