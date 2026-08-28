#include "Aevrynd/EnvironmentIbl3D.h"
#include "Aevrynd/Renderer.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace Aevrynd;

namespace {
constexpr std::size_t MaximumResidentBytes = 16u * 1024u * 1024u;

void Put32(std::vector<std::byte> &bytes, std::size_t offset,
           std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index)
    bytes[offset + index] = std::byte(value >> (index * 8));
}

bool WriteDds(const std::filesystem::path &path, std::uint32_t width,
              std::uint32_t height, std::uint32_t format, bool cube,
              std::uint32_t mipCount, bool unitBrdf = false) {
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
  if (unitBrdf)
    for (std::size_t offset = 148; offset + 3 < bytes.size(); offset += 4) {
      // R16G16_FLOAT 的 A=1、B=0，讓 benchmark 只衡量 IBL 取樣成本。
      bytes[offset] = std::byte{0x00};
      bytes[offset + 1] = std::byte{0x3c};
    }
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               std::streamsize(bytes.size()));
  return bool(output);
}

std::uint64_t Median(std::vector<std::uint64_t> samples) {
  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2];
}
} // 命名空間

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("aevrynd-ibl-d3d12-benchmark-" +
                     std::to_string(GetCurrentProcessId()));
  std::error_code fileError;
  std::filesystem::remove_all(root, fileError);
  std::filesystem::create_directories(root, fileError);
  if (fileError)
    return 1;
  const auto specular = root / "specular-1024.dds";
  const auto irradiance = root / "irradiance-64.dds";
  const auto brdf = root / "brdf-256.dds";
  if (!WriteDds(specular, 1024, 1024, 95, true, 11) ||
      !WriteDds(irradiance, 64, 64, 95, true, 7) ||
      !WriteDds(brdf, 256, 256, 34, false, 1, true)) {
    std::cerr << "IBL benchmark fixture generation failed\n";
    return 2;
  }

  EnvironmentLightingStore3D store;
  const auto lighting = store.Create({specular, irradiance, brdf, 0.f});
  if (!lighting) {
    std::cerr << "IBL benchmark fixture validation failed\n";
    return 3;
  }
  HWND window = CreateWindowExW(0, L"STATIC", L"AevryndIblBenchmark",
                                WS_OVERLAPPED, 0, 0, 1920, 1080, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window)
    return 4;

  int result{};
  try {
    D3D12Renderer renderer;
    renderer.Initialize(window, 1920, 1080, 1920, 1080, false);
    renderer.SetQualitySettings({false, false, false, false});
    const std::array<Vertex3D, 3> vertices = {
        Vertex3D{-0.8f, -0.8f, 0.f, PackNormalSnorm8({0, 0, -1}),
                 0xffffffffu, 0, 1},
        Vertex3D{0.f, 0.8f, 0.f, PackNormalSnorm8({0, 0, -1}),
                 0xffffffffu, .5f, 0},
        Vertex3D{0.8f, -0.8f, 0.f, PackNormalSnorm8({0, 0, -1}),
                 0xffffffffu, 1, 1}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    const auto mesh = renderer.CreateMesh(vertices, indices);
    DrawList draw;
    draw.SetViewProjection(Matrix4::Identity());
    draw.SetBackfaceCulling(false);
    draw.SetDirectionalLight({{0, -1, 0}, 1.f, {1, 1, 1, 1}, 0.f});
    for (unsigned y = 0; y < 100; ++y)
      for (unsigned x = 0; x < 100; ++x) {
        const Vec3 position{-0.99f + float(x) * .02f,
                            -0.99f + float(y) * .02f, .5f};
        if (!draw.MeshInstance(
                mesh, CreateTransformMatrix(position, {}, {.012f, .012f, 1}),
                {1, 1, 1, 1}, {.3f, 1.f, 0.f, 1.f},
                MeshRenderLayer3D::Opaque, {}, .25f, false,
                MaterialClass3D::Surface, .5f, false)) {
          std::cerr << "10k IBL instance submission failed\n";
          result = 5;
          break;
        }
      }
    if (!result &&
        (!mesh || draw.meshInstances.size() != 10000 ||
         !renderer.CreateEnvironmentLighting(store, lighting) ||
         !renderer.SetEnvironmentLighting(lighting))) {
      std::cerr << "IBL GPU benchmark setup failed\n";
      result = 6;
    }

    const auto measure = [&](EnvironmentLightingHandle3D active) {
      if (!renderer.SetEnvironmentLighting(active))
        throw std::runtime_error("IBL benchmark binding failed");
      renderer.Render(draw, {.01f, .015f, .02f, 1});
      renderer.WaitIdle();
      return renderer.GetStats().lastGpuFrameNanoseconds;
    };
    if (!result) {
      // 預熱涵蓋兩條 shader 分支及所有 1024 cubemap mip residency。
      for (unsigned iteration = 0; iteration < 12; ++iteration) {
        measure({});
        measure(lighting);
      }
      constexpr unsigned SampleRounds = 31;
      std::vector<std::uint64_t> fallbackSamples, iblSamples;
      fallbackSamples.reserve(SampleRounds);
      iblSamples.reserve(SampleRounds);
      for (unsigned iteration = 0; iteration < SampleRounds; ++iteration) {
        if (iteration & 1u) {
          iblSamples.push_back(measure(lighting));
          fallbackSamples.push_back(measure({}));
        } else {
          fallbackSamples.push_back(measure({}));
          iblSamples.push_back(measure(lighting));
        }
      }
      const std::uint64_t fallbackMedian = Median(fallbackSamples);
      const std::uint64_t iblMedian = Median(iblSamples);
      const std::int64_t delta = std::int64_t(iblMedian) -
                                 std::int64_t(fallbackMedian);
      const RendererStats stats = renderer.GetStats();
      const std::size_t combinedAllocationPeak =
          stats.peakResidentEnvironmentLightingBytes +
          stats.peakEnvironmentLightingUploadStagingBytes;
      const bool performancePass = delta <= 500000;
      const bool memoryPass = stats.residentEnvironmentLightingBytes > 0 &&
                              stats.residentEnvironmentLightingBytes <=
                                  MaximumResidentBytes &&
                              stats.peakResidentEnvironmentLightingBytes <
                                  stats.residentEnvironmentLightingBytes * 2 &&
                              combinedAllocationPeak <
                                  stats.residentEnvironmentLightingBytes * 2;
      std::cout << "instances=10000 resolution=1920x1080 fallbackMedianUs="
                << fallbackMedian / 1000 << " iblMedianUs=" << iblMedian / 1000
                << " deltaUs=" << delta / 1000
                << " residentBytes=" << stats.residentEnvironmentLightingBytes
                << " peakResidentBytes="
                << stats.peakResidentEnvironmentLightingBytes
                << " uploadPeakBytes="
                << stats.peakEnvironmentLightingUploadStagingBytes
                << " combinedPeakBytes=" << combinedAllocationPeak
                << " descriptors="
                << stats.residentEnvironmentLightingDescriptors << '\n';
      if (!performancePass || !memoryPass ||
          stats.residentEnvironmentLightingDescriptors != 3 ||
          stats.environmentLightingFallbackDescriptors != 4) {
        std::cerr << "IBL GPU performance or allocation budget failed\n";
        result = 7;
      }
    }
    renderer.SetEnvironmentLighting({});
    renderer.DestroyEnvironmentLighting(lighting);
    renderer.DestroyMesh(mesh);
    renderer.WaitIdle();
  } catch (const std::exception &error) {
    std::cerr << "IBL GPU benchmark failed: " << error.what() << '\n';
    result = 8;
  }
  DestroyWindow(window);
  std::filesystem::remove_all(root, fileError);
  return result;
}
