#pragma once

#include <Aevrynd/Mesh3D.h>
#include <cstddef>
#include <cstdint>

namespace GameplayLab {

enum class GameplayForestGroundCoverSpecies3D : std::uint8_t {
  Fern,
  LowShrub,
  LeafMoss,
};

enum class GameplayForestGroundCoverDetail3D : std::uint8_t {
  Near,
  Mid,
  Far,
};

// 三種地被可共用既有、完整 0..1 UV 的葉片 alpha 紋理；呼叫端必須提交
// 含 alpha 的貼圖，並沿用這份 mask/double-sided 契約。
struct GameplayForestGroundCoverAlphaCardContract3D {
  Aevrynd::MaterialAlphaMode3D alphaMode{Aevrynd::MaterialAlphaMode3D::Mask};
  float alphaCutoff{.45f};
  bool requiresTexture{true};
  bool requiresUv{true};
  bool doubleSided{true};
  std::uint8_t atlasColumns{1};
};

struct GameplayForestGroundCoverLodSet3D {
  Aevrynd::Mesh3D nearMesh;
  Aevrynd::Mesh3D midMesh;
  Aevrynd::Mesh3D farMesh;
};

// Fern, shrub and leaf-moss vertices are authored in metres.  Ecology placement
// keeps the useful horizontal footprint variation that was originally designed
// for unit grass cards, but its Y component must not shrink metre-authored
// plants a second time.
[[nodiscard]] constexpr Aevrynd::Vec3
GameplayForestGroundCoverWorldScale3D(Aevrynd::Vec3 ecologyScale) noexcept {
  ecologyScale.y = 1.f;
  return ecologyScale;
}

[[nodiscard]] constexpr GameplayForestGroundCoverAlphaCardContract3D
GameplayForestGroundCoverAlphaCardContract() noexcept {
  return {};
}

[[nodiscard]] inline Aevrynd::Material3D
PrepareGameplayForestGroundCoverMaterial3D(Aevrynd::Material3D material,
                                           bool castsShadow = false) noexcept {
  material.materialClass = Aevrynd::MaterialClass3D::Foliage;
  material.alphaMode = Aevrynd::MaterialAlphaMode3D::Mask;
  material.alphaCutoff = GameplayForestGroundCoverAlphaCardContract().alphaCutoff;
  material.useUv = true;
  material.doubleSided = true;
  material.castsShadow = castsShadow;
  material.surface.roughness = .86f;
  material.surface.metallic = 0.f;
  material.surface.emissive = 0.f;
  material.normalScale = .84f;
  material.textureScale = 1.f;
  return material;
}

[[nodiscard]] constexpr std::size_t GameplayForestGroundCoverTriangleBudget(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 24;
    case GameplayForestGroundCoverDetail3D::Mid: return 14;
    case GameplayForestGroundCoverDetail3D::Far: return 8;
  }
  return 0;
}

[[nodiscard]] constexpr std::size_t GameplayForestGroundCoverCommunityTriangleBudget(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 64;
    case GameplayForestGroundCoverDetail3D::Mid: return 40;
    case GameplayForestGroundCoverDetail3D::Far: return 24;
  }
  return 0;
}

// 河岸專用單一實例網格：低伏落葉／苔蘚先覆蓋濕土，再以縮短的蕨葉
// 提供垂直輪廓。它不含灌木，避免水線旁形成不自然的等高綠牆。
[[nodiscard]] constexpr std::size_t
GameplayForestRiverbankGroundCoverTriangleBudget(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 44;
    case GameplayForestGroundCoverDetail3D::Mid: return 26;
    case GameplayForestGroundCoverDetail3D::Far: return 14;
  }
  return 0;
}

[[nodiscard]] Aevrynd::Mesh3D CreateGameplayForestGroundCoverMesh3D(
    GameplayForestGroundCoverSpecies3D species,
    GameplayForestGroundCoverDetail3D detail = GameplayForestGroundCoverDetail3D::Near,
    std::uint32_t variationSeed = 1);

// 直接建立包含蕨類、低灌木與落葉/苔蘚的可實例化單一網格。
[[nodiscard]] Aevrynd::Mesh3D CreateGameplayForestGroundCoverCommunityMesh3D(
    GameplayForestGroundCoverDetail3D detail = GameplayForestGroundCoverDetail3D::Near,
    std::uint32_t variationSeed = 1);

[[nodiscard]] Aevrynd::Mesh3D CreateGameplayForestRiverbankGroundCoverMesh3D(
    GameplayForestGroundCoverDetail3D detail = GameplayForestGroundCoverDetail3D::Near,
    std::uint32_t variationSeed = 1);

[[nodiscard]] GameplayForestGroundCoverLodSet3D
CreateGameplayForestGroundCoverLodSet3D(
    GameplayForestGroundCoverSpecies3D species,
    std::uint32_t variationSeed = 1);

} // namespace GameplayLab
