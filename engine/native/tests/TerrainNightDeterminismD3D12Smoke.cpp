#include "Aevrynd/Camera.h"
#include "Aevrynd/CookedRenderManifest3D.h"
#include "Aevrynd/HeightField3D.h"
#include "Aevrynd/Mesh3D.h"
#include "Aevrynd/Renderer.h"
#include "Aevrynd/WorldDeltaPersistence3D.h"
#include "GameplayShowcaseEnvironment3D.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <vector>

using namespace Aevrynd;

namespace {
std::string HexSha(std::span<const std::byte> bytes) {
  std::array<std::byte, 32> digest{};
  if (!WorldDeltaSha2563D(bytes, digest)) return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const std::byte item : digest) {
    const auto value = std::to_integer<unsigned>(item);
    result.push_back(hex[value >> 4]);
    result.push_back(hex[value & 15]);
  }
  return result;
}

MaterialTextureSetDefinition3D ForestTextures(CookedRenderManifest3D& manifest) {
  const auto manifestPath = std::filesystem::path{AEVRYND_SOURCE_DIR} /
      "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
  CookedRenderManifestError3D error{};
  if (!manifest.LoadFile(manifestPath, &error)) return {};
  const auto* material = manifest.FindMaterial("forest_ground_04:slot-0");
  if (!material) return {};
  auto result = material->Definition(2048).textures;
  result.baseColorPath = manifest.AssetRoot() / result.baseColorPath;
  result.roughnessPath = manifest.AssetRoot() / result.roughnessPath;
  result.normalPath = manifest.AssetRoot() / result.normalPath;
  return result;
}

struct Mode {
  const char* name;
  RendererQualitySettings3D quality;
};

struct ReadabilityMetrics {
  std::size_t pixels{};
  double mean{};
  double p10{};
  double clippedFraction{};
};

ReadabilityMetrics MeasureIdentity(
    const D3D12RendererTestAccess3D::LinearHdrCapture3D& capture,
    const ObjectIdCapture3D& identities,
    const RenderObjectIdentity3D* expected) {
  ReadabilityMetrics result{};
  std::vector<double> luminance;
  luminance.reserve(capture.pixels.size());
  std::size_t clipped{};
  for (std::size_t index = 0; index < capture.pixels.size(); ++index) {
    const auto* identity = identities.Resolve(identities.pixels[index]);
    if ((expected && (!identity || *identity != *expected)) ||
        (!expected && identity))
      continue;
    const auto& pixel = capture.pixels[index];
    const double value = .2126 * pixel.x + .7152 * pixel.y + .0722 * pixel.z;
    luminance.push_back(value);
    clipped += pixel.x >= .999f || pixel.y >= .999f || pixel.z >= .999f;
  }
  result.pixels = luminance.size();
  if (luminance.empty()) return result;
  std::ranges::sort(luminance);
  result.mean = std::accumulate(luminance.begin(), luminance.end(), 0.0) /
                static_cast<double>(luminance.size());
  result.p10 = luminance[(luminance.size() - 1) / 10];
  result.clippedFraction = static_cast<double>(clipped) /
                           static_cast<double>(luminance.size());
  return result;
}
}

int main() {
  HWND window = CreateWindowExW(0, L"STATIC", L"AevryndTerrainNightDeterminism",
      WS_OVERLAPPED, 0, 0, 640, 360, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window) {
    std::cerr << "terrain_night window creation failed win32=" << GetLastError() << '\n';
    return 1;
  }

  HeightField3D field({{-128.f, 0.f, -128.f}, 129, 129, 2.f});
  if (!field.GenerateUltra(0x4a17u, {10.5f, .0105f, .56f, .22f, 24.f, .34f, 7, 3, .68f, .16f})) return 2;
  const Mesh3D terrain = field.CreateMesh();
  const Mesh3D box = Mesh3D::CreateUnitBox();
  if (!terrain.Valid() || !box.Valid()) return 3;

  GameplayLab::GameplayShowcaseEnvironment3D showcase;
  if (!showcase.Initialize(0x4a17u)) return 4;
  CookedRenderManifest3D manifest;
  const auto textureDefinition = ForestTextures(manifest);
  if (textureDefinition.baseColorPath.empty() || textureDefinition.roughnessPath.empty() ||
      textureDefinition.normalPath.empty()) return 5;

  const std::array modes{
      Mode{"base", {false, false, false, false}},
      Mode{"msaa", {false, false, true, false}},
      Mode{"shadow", {true, false, false, false}},
      Mode{"ao", {false, true, false, false}},
      Mode{"full", {true, true, true, false}},
      Mode{"taa", {true, true, true, true}},
  };
  const Camera3D camera{{34.f, 92.f, -116.f}, {0.f, 0.f, 10.f}, {0.f, 1.f, 0.f},
      {ProjectionMode::Orthographic, 60.f * 3.14159265358979323846f / 180.f, 118.f, .1f, 500.f}};
  const auto viewProjection = CreateViewProjectionMatrix(camera, 640.f / 360.f);
  const RenderObjectIdentity3D terrainIdentity{{0x2ec093e0e5318ca1ull, 1}, 1};

  for (std::size_t modeIndex = 0; modeIndex < modes.size(); ++modeIndex) {
    D3D12Renderer renderer;
    renderer.Initialize(window, 640, 360, 640, 360, false);
    renderer.SetQualitySettings(modes[modeIndex].quality);
    const auto terrainHandle = renderer.CreateMesh(terrain.vertices, terrain.indices);
    const auto boxHandle = renderer.CreateMesh(box.vertices, box.indices);
    const auto textures = renderer.CreateMaterialTextureSet(textureDefinition);
    if (!terrainHandle || !boxHandle || !textures) return 6;

    DrawList draw;
    draw.SetViewProjection(viewProjection);
    draw.SetBackfaceCulling(false);
    showcase.ApplyHighProfile(draw);
    showcase.ApplyLighting(draw, GameplayLab::ShowcaseLightingProfile3D::Night, camera.position);
    draw.MeshInstance(terrainHandle, Matrix4::Identity(), GameplayLab::ShowcaseForestTerrainTint3D(),
        {.72f, 0.f, 0.f, 1.f}, MeshRenderLayer3D::Opaque, textures,
        GameplayLab::ShowcaseForestTerrainTextureScale3D(), true, MaterialClass3D::Terrain,
        std::numeric_limits<float>::quiet_NaN(), false, 1, 0, true,
        DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Automatic,
        Matrix4::Identity(), false, 1.f, MaterialAlphaMode3D::Opaque, .5f, false,
        terrainIdentity);
    for (unsigned index = 0; index < 5; ++index) {
      const Vec3 position{-32.f + float(index) * 16.f, 8.f + float(index % 2) * 5.f,
          -12.f + float(index % 3) * 18.f};
      const RenderObjectIdentity3D boxIdentity{{0x2ec093e0e5318ca1ull,
                                                 std::uint64_t(index + 2)}, 1};
      draw.MeshInstance(boxHandle, CreateTransformMatrix(position, {}, {3.f, 18.f, 3.f}),
          {.18f, .24f, .12f, 1.f}, {.82f, 0.f, 0.f, 1.f}, MeshRenderLayer3D::Opaque,
          {}, 1.f, false, MaterialClass3D::Surface,
          std::numeric_limits<float>::quiet_NaN(), false, 1, 0, false,
          DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Automatic,
          Matrix4::Identity(), false, 1.f, MaterialAlphaMode3D::Opaque, .5f,
          false, boxIdentity);
    }

    for (unsigned warmup = 0; warmup < 4; ++warmup) renderer.Render(draw, {.003f, .006f, .014f, 1.f});
    if (!renderer.RequestObjectIdReadback3D()) {
      std::cerr << "terrain_night object-id request failed mode=" << modes[modeIndex].name << '\n';
      return 7;
    }
    renderer.Render(draw, {.003f, .006f, .014f, 1.f});
    const auto identities = renderer.TakeObjectIdReadback3D();
    auto hdr = D3D12RendererTestAccess3D::CaptureLinearHdr(renderer,
        showcase.EvaluateLighting(GameplayLab::ShowcaseLightingProfile3D::Night).exposureEv);
    auto tone = D3D12RendererTestAccess3D::CaptureToneMapped(renderer,
        showcase.EvaluateLighting(GameplayLab::ShowcaseLightingProfile3D::Night).exposureEv);
    renderer.WaitIdle();
    if (!identities || !identities->Complete() || !hdr || !hdr->Complete() ||
        !tone || !tone->Complete() || hdr->width != identities->width ||
        hdr->height != identities->height || tone->width != identities->width ||
        tone->height != identities->height) {
      std::cerr << "terrain_night capture incomplete mode=" << modes[modeIndex].name
                << " oid=" << bool(identities) << '/' << (identities && identities->Complete())
                << " hdr=" << bool(hdr) << '/' << (hdr && hdr->Complete())
                << " tone=" << bool(tone) << '/' << (tone && tone->Complete()) << '\n';
      return 7;
    }
    const auto terrainHdr = MeasureIdentity(*hdr, *identities, &terrainIdentity);
    const auto terrainTone = MeasureIdentity(*tone, *identities, &terrainIdentity);
    const auto skyHdr = MeasureIdentity(*hdr, *identities, nullptr);
    const auto skyTone = MeasureIdentity(*tone, *identities, nullptr);
    const auto hash = HexSha(std::as_bytes(std::span(hdr->pixels)));
    const auto stats = renderer.GetStats();
    std::cout << "terrain_night mode=" << modes[modeIndex].name << " sha256=" << hash
              << " samples=" << stats.multisampleCount
              << " shadows=" << stats.effectiveQuality.directionalShadows
              << " ao=" << stats.ambientOcclusionEnabled
              << " terrain=" << stats.lastSubmittedTerrainInstances
              << " terrainPixels=" << terrainHdr.pixels
              << " terrainHdr=" << terrainHdr.mean << '/' << terrainHdr.p10
              << " terrainTone=" << terrainTone.mean << '/' << terrainTone.p10
              << " terrainClip=" << terrainTone.clippedFraction
              << " skyPixels=" << skyHdr.pixels
              << " skyHdr=" << skyHdr.mean << '/' << skyHdr.p10
              << " skyTone=" << skyTone.mean << '/' << skyTone.p10
              << " skyClip=" << skyTone.clippedFraction << '\n';
    if (hash.empty() || !stats.lastSubmittedTerrainInstances ||
        terrainHdr.pixels < 4096 || terrainTone.pixels != terrainHdr.pixels ||
        skyHdr.pixels < 64 || skyTone.pixels != skyHdr.pixels ||
        terrainHdr.mean < .0153 || terrainHdr.p10 < .0146 ||
        terrainTone.mean < .0100 || terrainTone.p10 < .0063 ||
        terrainTone.clippedFraction > .001 || skyTone.mean < .0144 ||
        skyTone.mean > .0147 || skyTone.clippedFraction > .001)
      return 8;
    renderer.DestroyMaterialTextureSet(textures);
    renderer.DestroyMesh(boxHandle);
    renderer.DestroyMesh(terrainHandle);
    renderer.WaitIdle();
  }
  DestroyWindow(window);
  return 0;
}
