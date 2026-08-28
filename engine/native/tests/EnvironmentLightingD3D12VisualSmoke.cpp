#include "Aevrynd/EnvironmentIbl3D.h"
#include "Aevrynd/Mesh3D.h"
#include "Aevrynd/Renderer.h"

#include <Windows.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace Aevrynd;

namespace {
struct Image {
  unsigned width{}, height{};
  std::vector<std::uint8_t> rgba;
};
struct Region {
  unsigned left{}, top{}, right{}, bottom{};
};

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
      bytes[offset] = std::byte{0x00};
      bytes[offset + 1] = std::byte{0x3c};
    }
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               std::streamsize(bytes.size()));
  return bool(output);
}

bool DecodePng(const std::filesystem::path &path, Image &output) {
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> converter;
  UINT width{}, height{};
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
      FAILED(factory->CreateDecoderFromFilename(
          path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
          &decoder)) ||
      FAILED(decoder->GetFrame(0, &frame)) ||
      FAILED(frame->GetSize(&width, &height)) || !width || !height ||
      FAILED(factory->CreateFormatConverter(&converter)) ||
      FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom)))
    return false;
  output = {width, height, std::vector<std::uint8_t>(std::size_t(width) *
                                                     height * 4)};
  return SUCCEEDED(converter->CopyPixels(nullptr, width * 4,
                                         UINT(output.rgba.size()),
                                         output.rgba.data()));
}

double Luminance(const Image &image, unsigned x, unsigned y) {
  const auto offset = (std::size_t(y) * image.width + x) * 4;
  return (0.2126 * image.rgba[offset] + 0.7152 * image.rgba[offset + 1] +
          0.0722 * image.rgba[offset + 2]) /
         255.0;
}

double MeanLuminance(const Image &image, Region region) {
  double sum{};
  std::size_t count{};
  for (unsigned y = region.top; y < region.bottom; ++y)
    for (unsigned x = region.left; x < region.right; ++x) {
      sum += Luminance(image, x, y);
      ++count;
    }
  return count ? sum / double(count) : 0;
}

double MeanAbsoluteDifference(const Image &left, const Image &right,
                              Region region) {
  double sum{};
  std::size_t count{};
  for (unsigned y = region.top; y < region.bottom; ++y)
    for (unsigned x = region.left; x < region.right; ++x) {
      sum += std::abs(Luminance(left, x, y) - Luminance(right, x, y));
      ++count;
    }
  return count ? sum / double(count) : 0;
}

std::array<double, 2> HighlightCentroid(const Image &image, Region region) {
  double maximum{};
  for (unsigned y = region.top; y < region.bottom; ++y)
    for (unsigned x = region.left; x < region.right; ++x)
      maximum = (std::max)(maximum, Luminance(image, x, y));
  const double threshold = maximum * .82;
  double total{}, weightedX{}, weightedY{};
  for (unsigned y = region.top; y < region.bottom; ++y)
    for (unsigned x = region.left; x < region.right; ++x) {
      const double luminance = Luminance(image, x, y);
      const double weight = (std::max)(0.0, luminance - threshold);
      total += weight;
      weightedX += weight * x;
      weightedY += weight * y;
    }
  return total > 0 ? std::array<double, 2>{weightedX / total,
                                          weightedY / total}
                   : std::array<double, 2>{double(region.left),
                                           double(region.top)};
}

std::uint64_t Fnv1a64(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::uint64_t hash = 14695981039346656037ull;
  std::array<char, 16384> buffer{};
  while (input) {
    input.read(buffer.data(), std::streamsize(buffer.size()));
    for (std::streamsize index = 0; index < input.gcount(); ++index) {
      hash ^= std::uint8_t(buffer[std::size_t(index)]);
      hash *= 1099511628211ull;
    }
  }
  return hash;
}
} // 命名空間

int main() {
  const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const auto temporary = std::filesystem::temp_directory_path() /
                         ("aevrynd-ibl-visual-" +
                          std::to_string(GetCurrentProcessId()));
  const std::filesystem::path artifacts{AEVRYND_TEST_ARTIFACT_DIR};
  std::error_code fileError;
  std::filesystem::remove_all(temporary, fileError);
  std::filesystem::create_directories(temporary, fileError);
  std::filesystem::create_directories(artifacts, fileError);
  if (fileError)
    return 1;
  const std::filesystem::path directional =
      std::filesystem::path{AEVRYND_SOURCE_DIR} / "engine" / "native" /
      "tests" / "fixtures" / "environment-directional-bc6h.dds";
  const auto blackIrradiance = temporary / "black-irradiance.dds";
  const auto brdf = temporary / "unit-brdf.dds";
  if (!std::filesystem::is_regular_file(directional) ||
      !WriteDds(blackIrradiance, 128, 128, 95, true, 8) ||
      !WriteDds(brdf, 64, 64, 34, false, 1, true)) {
    std::cerr << "IBL visual fixture generation failed\n";
    return 2;
  }

  EnvironmentLightingStore3D store;
  constexpr float HalfPi = 1.57079632679489661923f;
  const auto rotation0 = store.Create({directional, directional, brdf, 0.f});
  const auto rotation90 =
      store.Create({directional, directional, brdf, HalfPi});
  const auto blackDiffuse =
      store.Create({directional, blackIrradiance, brdf, HalfPi});
  if (!rotation0 || !rotation90 || !blackDiffuse) {
    std::cerr << "IBL visual definitions failed validation\n";
    return 3;
  }

  HWND window = CreateWindowExW(0, L"STATIC", L"AevryndIblVisualTest",
                                WS_OVERLAPPED, 0, 0, 640, 480, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window)
    return 4;
  int result{};
  try {
    D3D12Renderer renderer;
    renderer.Initialize(window, 640, 480, 640, 480, false);
    renderer.SetQualitySettings({false, false, false, false});
    const Mesh3D sphereData = Mesh3D::CreateUnitSphere(48, 32);
    const auto sphere = renderer.CreateMesh(sphereData.vertices,
                                            sphereData.indices);
    DrawList draw;
    draw.SetViewProjection(Matrix4::Identity());
    draw.SetBackfaceCulling(false);
    draw.SetDirectionalLight({{0, -1, 0}, 1.f, {1, 1, 1, 1}, 0.f});
    const auto sphereTransform = [](float x, float y, float scale) {
      return CreateTransformMatrix({x, y, .62f}, {}, {scale, scale, scale});
    };
    const std::array<float, 3> roughness{.1f, .5f, .9f};
    for (unsigned index = 0; index < roughness.size(); ++index)
      draw.MeshInstance(sphere,
                        sphereTransform(-.62f + float(index) * .62f, .28f,
                                        .52f),
                        {1, 1, 1, 1}, {roughness[index], 1.f, 0.f, 1.f},
                        MeshRenderLayer3D::Opaque, {}, .25f, false,
                        MaterialClass3D::Surface, .62f, false);
    draw.MeshInstance(sphere, sphereTransform(-.32f, -.43f, .58f),
                      {1, 1, 1, 1}, {.35f, 1.f, 0.f, 1.f},
                      MeshRenderLayer3D::Opaque, {}, .25f, false,
                      MaterialClass3D::Surface, .62f, false);
    draw.MeshInstance(sphere, sphereTransform(.32f, -.43f, .58f),
                      {.72f, .72f, .72f, 1}, {.35f, 0.f, 0.f, 1.f},
                      MeshRenderLayer3D::Opaque, {}, .25f, false,
                      MaterialClass3D::Surface, .62f, false);

    const auto capture = [&](EnvironmentLightingHandle3D lighting,
                             const std::filesystem::path &path, Image &image) {
      if (!renderer.SetEnvironmentLighting(lighting) ||
          !renderer.CaptureNextFrame(path))
        return false;
      renderer.Render(draw, {.008f, .012f, .018f, 1});
      renderer.WaitIdle();
      return DecodePng(path, image);
    };
    if (!sphere || !renderer.CreateEnvironmentLighting(store, rotation0) ||
        !renderer.CreateEnvironmentLighting(store, rotation90) ||
        !renderer.CreateEnvironmentLighting(store, blackDiffuse)) {
      std::cerr << "IBL visual GPU setup failed\n";
      result = 5;
    }
    const auto path0 = artifacts / "environment-rotation-000.png";
    const auto path90 = artifacts / "environment-rotation-090.png";
    const auto pathBlack = artifacts / "environment-black-irradiance.png";
    Image image0, image90, imageBlack;
    if (!result &&
        (!capture(rotation0, path0, image0) ||
         !capture(rotation90, path90, image90) ||
         !capture(blackDiffuse, pathBlack, imageBlack))) {
      std::cerr << "IBL visual capture or PNG readback failed\n";
      result = 6;
    }
    if (!result) {
      const Region full{0, 0, 640, 480};
      const Region highlight{250, 100, 390, 240};
      const std::array<Region, 3> roughnessRegions = {
          Region{111, 157, 131, 177}, Region{310, 157, 330, 177},
          Region{508, 157, 528, 177}};
      const Region metal{180, 324, 255, 399};
      const Region dielectric{385, 324, 460, 399};
      const auto centroid0 = HighlightCentroid(image0, highlight);
      const auto centroid90 = HighlightCentroid(image90, highlight);
      const double centroidDistance =
          std::hypot(centroid90[0] - centroid0[0],
                     centroid90[1] - centroid0[1]);
      const double rotationDifference =
          MeanAbsoluteDifference(image0, image90, full);
      const std::array<double, 3> roughnessEnergy = {
          MeanLuminance(image90, roughnessRegions[0]),
          MeanLuminance(image90, roughnessRegions[1]),
          MeanLuminance(image90, roughnessRegions[2])};
      const double metalDiffuseDelta =
          MeanAbsoluteDifference(image90, imageBlack, metal);
      const double dielectricDiffuseDelta =
          MeanAbsoluteDifference(image90, imageBlack, dielectric);
      const double dielectricEnergy = MeanLuminance(image90, dielectric);
      const bool finite =
          std::isfinite(rotationDifference) && std::isfinite(centroidDistance) &&
          std::ranges::all_of(roughnessEnergy,
                              [](double value) { return std::isfinite(value); }) &&
          std::isfinite(metalDiffuseDelta) &&
          std::isfinite(dielectricDiffuseDelta) &&
          std::isfinite(dielectricEnergy);
      const bool rotationPass = rotationDifference > .003 &&
                                centroidDistance > 2.0;
      const bool roughnessPass = roughnessEnergy[0] > roughnessEnergy[1] &&
                                 roughnessEnergy[1] > roughnessEnergy[2];
      const bool materialPass = dielectricDiffuseDelta > .003 &&
                                metalDiffuseDelta <
                                    dielectricDiffuseDelta * .2 + .001 &&
                                dielectricEnergy > .01 &&
                                dielectricEnergy < .95;

      const auto manifest = artifacts / "environment-lighting-visual.txt";
      std::ofstream output(manifest, std::ios::trunc);
      output << std::fixed << std::setprecision(8)
             << "fixture=directional per-face BC6H 128x128 8 mips\n"
             << "roughness=0.1,0.5,0.9\n"
             << "metallic=1,1,1,1,0\n"
             << "rotationRadians=0," << HalfPi << '\n'
             << "rotationDifference=" << rotationDifference << '\n'
             << "highlightCentroidDistance=" << centroidDistance << '\n'
             << "roughnessEnergy=" << roughnessEnergy[0] << ','
             << roughnessEnergy[1] << ',' << roughnessEnergy[2] << '\n'
             << "metalDiffuseDelta=" << metalDiffuseDelta << '\n'
             << "dielectricDiffuseDelta=" << dielectricDiffuseDelta << '\n'
             << "dielectricEnergy=" << dielectricEnergy << '\n'
             << std::hex << std::setfill('0')
             << "rotation000Fnv1a64=" << std::setw(16) << Fnv1a64(path0)
             << '\n'
             << "rotation090Fnv1a64=" << std::setw(16) << Fnv1a64(path90)
             << '\n'
             << "blackIrradianceFnv1a64=" << std::setw(16)
             << Fnv1a64(pathBlack) << '\n';
      output.close();
      std::cout << "rotationDifference=" << rotationDifference
                << " centroidDistance=" << centroidDistance
                << " roughness=" << roughnessEnergy[0] << ','
                << roughnessEnergy[1] << ',' << roughnessEnergy[2]
                << " metalDelta=" << metalDiffuseDelta
                << " dielectricDelta=" << dielectricDiffuseDelta
                << " artifacts=" << artifacts.string() << '\n';
      if (!finite || !rotationPass || !roughnessPass || !materialPass) {
        std::cerr << "IBL visual/readback contract failed\n";
        result = 7;
      }
    }
    renderer.SetEnvironmentLighting({});
    renderer.DestroyEnvironmentLighting(blackDiffuse);
    renderer.DestroyEnvironmentLighting(rotation90);
    renderer.DestroyEnvironmentLighting(rotation0);
    renderer.DestroyMesh(sphere);
    renderer.WaitIdle();
  } catch (const std::exception &error) {
    std::cerr << "IBL visual test failed: " << error.what() << '\n';
    result = 8;
  }
  DestroyWindow(window);
  std::filesystem::remove_all(temporary, fileError);
  if (apartment == S_OK || apartment == S_FALSE)
    CoUninitialize();
  return result;
}
