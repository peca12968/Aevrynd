#include "Aevrynd/Renderer.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
void Put32(std::vector<std::byte> &bytes, std::size_t offset,
           std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index)
    bytes[offset + index] = std::byte(value >> (index * 8));
}

bool WriteDds(const std::filesystem::path &path, std::uint32_t width,
              std::uint32_t height, std::uint32_t format, bool cube,
              std::uint32_t mipCount) {
  std::size_t payloadBytes{};
  for (std::uint32_t face = 0; face < (cube ? 6u : 1u); ++face)
    for (std::uint32_t mip = 0; mip < mipCount; ++mip) {
      const auto mipWidth = (std::max)(1u, width >> mip);
      const auto mipHeight = (std::max)(1u, height >> mip);
      payloadBytes += cube ? std::size_t((mipWidth + 3) / 4) *
                                 ((mipHeight + 3) / 4) * 16
                           : std::size_t(mipWidth) * mipHeight * 4;
    }
  std::vector<std::byte> bytes(148 + payloadBytes);
  Put32(bytes, 0, 0x20534444);
  Put32(bytes, 4, 124);
  Put32(bytes, 12, height);
  Put32(bytes, 16, width);
  Put32(bytes, 28, mipCount);
  Put32(bytes, 76, 32);
  Put32(bytes, 80, 4);
  Put32(bytes, 84, 0x30315844);
  Put32(bytes, 128, format);
  Put32(bytes, 132, 3);
  Put32(bytes, 136, cube ? 4 : 0);
  Put32(bytes, 140, 1);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               std::streamsize(bytes.size()));
  return bool(output);
}

bool PatchPayloadByte(const std::filesystem::path &path, unsigned value) {
  std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
  file.seekp(148);
  const char byte = char(value);
  file.write(&byte, 1);
  return bool(file);
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("aevrynd-ibl-gpu-" +
                     std::to_string(GetCurrentProcessId()));
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  if (error)
    return 1;
  const auto specular = root / "specular.dds";
  const auto irradiance = root / "irradiance.dds";
  const auto brdf = root / "brdf.dds";
  if (!WriteDds(specular, 16, 16, 95, true, 5) ||
      !WriteDds(irradiance, 8, 8, 95, true, 4) ||
      !WriteDds(brdf, 16, 16, 34, false, 1))
    return 2;

  const EnvironmentIblDefinition3D definition{specular, irradiance, brdf,
                                               .375f};
  EnvironmentLightingStore3D store;
  const auto handle = store.Create(definition);
  if (!handle)
    return 3;
  HWND window = CreateWindowExW(0, L"STATIC",
                                L"AevryndEnvironmentLightingGpuTest",
                                WS_OVERLAPPED, 0, 0, 320, 180, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
  if (!window)
    return 4;
  int result{};
  {
    D3D12Renderer renderer;
    renderer.Initialize(window, 320, 180, 320, 180, false);
    const RendererStats baseline = renderer.GetStats();
    // typed null descriptors 不配置 GPU resource，純 2D 啟動必須維持真零配置。
    if (baseline.environmentLightingResourceAllocations ||
        baseline.residentEnvironmentLightingBytes ||
        baseline.residentEnvironmentLightingDescriptors ||
        baseline.environmentLightingFallbackDescriptors != 4 ||
        baseline.environmentLightingActive)
      result = 5;

    for (int checkpoint = 0; !result && checkpoint < 4; ++checkpoint) {
      const RendererStats before = renderer.GetStats();
      D3D12RendererTestAccess3D::SetEnvironmentResourceFailureCheckpoint(
          renderer, checkpoint);
      if (renderer.CreateEnvironmentLighting(store, handle)) {
        result = 6;
        break;
      }
      const RendererStats after = renderer.GetStats();
      if (after.environmentLightingResourceAllocations !=
              before.environmentLightingResourceAllocations ||
          after.residentEnvironmentLightingBytes !=
              before.residentEnvironmentLightingBytes ||
          after.residentEnvironmentLightingSets !=
              before.residentEnvironmentLightingSets ||
          after.residentEnvironmentLightingDescriptors !=
              before.residentEnvironmentLightingDescriptors ||
          after.environmentLightingAtomicRollbacks !=
              before.environmentLightingAtomicRollbacks + 1)
        result = 7;
    }
    D3D12RendererTestAccess3D::SetEnvironmentResourceFailureCheckpoint(renderer,
                                                                       -1);
    if (!result &&
        (renderer.CreateEnvironmentLighting(store,
                                            {0xfedcba9876543210ull}) ||
         renderer.GetStats().environmentLightingResourceAllocations))
      result = 8;
    if (!result && (!renderer.CreateEnvironmentLighting(store, handle) ||
                    !renderer.CreateEnvironmentLighting(store, handle)))
      result = 9;
    if (!result) {
      const RendererStats created = renderer.GetStats();
      if (created.residentEnvironmentLightingSets != 1 ||
          !created.residentEnvironmentLightingBytes ||
          created.environmentLightingResourceAllocations != 3 ||
          created.environmentLightingUploads != 1 ||
          created.residentEnvironmentLightingDescriptors != 3 ||
          created.peakResidentEnvironmentLightingBytes <
              created.residentEnvironmentLightingBytes ||
          !created.peakEnvironmentLightingUploadStagingBytes)
        result = 10;
    }
    if (!result && (!renderer.SetEnvironmentLighting(handle) ||
                    renderer.ActiveEnvironmentLighting() != handle))
      result = 11;
    if (!result) {
      store.Destroy(handle);
      DrawList frame{DrawListCapacityProfile::Minimal2D};
      frame.Quad({160, 90}, {64, 64}, {1, 1, 1, 1});
      frame.WorldTriangle({-.5f, -.5f, .5f}, {0, .5f, .5f},
                          {.5f, -.5f, .5f}, {1, 1, 1, 1});
      renderer.Render(frame, {.01f, .02f, .03f, 1});
      renderer.WaitIdle();
      renderer.DestroyEnvironmentLighting(handle);
      if (renderer.ActiveEnvironmentLighting() != handle)
        result = 12;
      renderer.DestroyEnvironmentLighting(handle);
      renderer.WaitIdle();
      renderer.TrimEnvironmentLighting();
      const RendererStats released = renderer.GetStats();
      if (renderer.ActiveEnvironmentLighting() ||
          released.environmentLightingActive ||
          released.residentEnvironmentLightingSets ||
          released.residentEnvironmentLightingBytes ||
          released.pendingEnvironmentLightingReleaseBytes ||
          released.environmentLightingUploadStagingBytes ||
          released.residentEnvironmentLightingDescriptors ||
          released.environmentLightingResourceReleases != 3)
        result = 13;
    }

    // 填滿全部 immutable slots，確認第十七組原子拒絕；fence 完成後 slot 才可重用。
    std::vector<EnvironmentLightingHandle3D> slotHandles;
    for (unsigned index = 0; !result && index < 17; ++index) {
      const auto path = root / ("specular-slot-" + std::to_string(index) +
                                ".dds");
      std::filesystem::copy_file(specular, path,
                                 std::filesystem::copy_options::overwrite_existing,
                                 error);
      if (error || !PatchPayloadByte(path, index + 1)) {
        result = 14;
        break;
      }
      const auto candidate =
          store.Create({path, irradiance, brdf, float(index) * .01f});
      if (!candidate) {
        result = 15;
        break;
      }
      slotHandles.push_back(candidate);
      const RendererStats before = renderer.GetStats();
      const bool created = renderer.CreateEnvironmentLighting(store, candidate);
      if (index < 16 && !created)
        result = 16;
      if (index == 16 &&
          (created || renderer.GetStats().environmentLightingResourceAllocations !=
                          before.environmentLightingResourceAllocations))
        result = 17;
    }
    for (std::size_t index = 0; index < slotHandles.size() && index < 16;
         ++index)
      renderer.DestroyEnvironmentLighting(slotHandles[index]);
    renderer.WaitIdle();
    if (!result &&
        !renderer.CreateEnvironmentLighting(store, slotHandles.back()))
      result = 18;
    if (!result)
      renderer.DestroyEnvironmentLighting(slotHandles.back());
    renderer.WaitIdle();
  }

  // active upload/frame 尚在 flight 時直接析構，析構內 Flush 必須安全完成。
  if (!result) {
    const auto shutdownHandle = store.Create(definition);
    if (!shutdownHandle)
      result = 19;
    else {
      D3D12Renderer renderer;
      renderer.Initialize(window, 320, 180, 320, 180, false);
      if (!renderer.CreateEnvironmentLighting(store, shutdownHandle) ||
          !renderer.SetEnvironmentLighting(shutdownHandle))
        result = 20;
      else {
        DrawList frame{DrawListCapacityProfile::Minimal2D};
        frame.WorldTriangle({-.5f, -.5f, .5f}, {0, .5f, .5f},
                            {.5f, -.5f, .5f}, {1, 1, 1, 1});
        renderer.Render(frame, {0, 0, 0, 1});
      }
    }
  }

  DestroyWindow(window);
  std::filesystem::remove_all(root, error);
  if (result)
    std::cerr << "Environment D3D12 lifecycle failed at " << result << '\n';
  return result;
}
