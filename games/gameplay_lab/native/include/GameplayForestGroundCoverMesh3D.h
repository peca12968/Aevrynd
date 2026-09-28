#pragma once

#include <Aevrynd/Mesh3D.h>
#include <array>
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

enum class GameplayForestGroundCoverVariant3D : std::uint8_t {
  Fern,
  Riverbank,
  Community,
};

enum class GameplayForestGroundCoverAtlas3D : std::uint8_t {
  IslandTree01,
  TreeSmall02,
};

struct GameplayForestGroundCoverUvRect3D {
  float minimumU{};
  float minimumV{};
  float maximumU{1.f};
  float maximumV{1.f};

  [[nodiscard]] constexpr bool operator==(
      const GameplayForestGroundCoverUvRect3D&) const noexcept = default;
};

// 正式葉片來源是 atlas，不是可整張平鋪的單一葉片。區域保留數個 texel
// 的透明 padding，避免線性取樣跨到相鄰葉片；所有 PBR 貼圖共用同一 UV。
[[nodiscard]] constexpr GameplayForestGroundCoverUvRect3D
GameplayForestGroundCoverAtlasUv3D(GameplayForestGroundCoverAtlas3D atlas,
                                   std::uint32_t item) noexcept {
  constexpr float inverseExtent = 1.f / 1024.f;
  constexpr std::array<GameplayForestGroundCoverUvRect3D, 5> islandTree{{
      {12.f * inverseExtent, 16.f * inverseExtent, 152.f * inverseExtent,
       526.f * inverseExtent},
      {164.f * inverseExtent, 16.f * inverseExtent, 349.f * inverseExtent,
       405.f * inverseExtent},
      {365.f * inverseExtent, 27.f * inverseExtent, 498.f * inverseExtent,
       373.f * inverseExtent},
      {525.f * inverseExtent, 46.f * inverseExtent, 668.f * inverseExtent,
       375.f * inverseExtent},
      {706.f * inverseExtent, 16.f * inverseExtent, 839.f * inverseExtent,
       427.f * inverseExtent},
  }};
  constexpr std::array<GameplayForestGroundCoverUvRect3D, 5> treeSmall{{
      {46.f * inverseExtent, 36.f * inverseExtent, 214.f * inverseExtent,
       194.f * inverseExtent},
      {191.f * inverseExtent, 16.f * inverseExtent, 354.f * inverseExtent,
       184.f * inverseExtent},
      {571.f * inverseExtent, 21.f * inverseExtent, 769.f * inverseExtent,
       209.f * inverseExtent},
      {256.f * inverseExtent, 231.f * inverseExtent, 434.f * inverseExtent,
       389.f * inverseExtent},
      {51.f * inverseExtent, 221.f * inverseExtent, 244.f * inverseExtent,
       394.f * inverseExtent},
  }};
  return atlas == GameplayForestGroundCoverAtlas3D::IslandTree01
             ? islandTree[item % islandTree.size()]
             : treeSmall[item % treeSmall.size()];
}

// 莖使用葉片內部已知不透明的小區域，避免在 foliage 材質上拉伸整片
// 葉形造成中軸斷裂；四周仍留有足夠 texel 避免跨區域取樣。
[[nodiscard]] constexpr GameplayForestGroundCoverUvRect3D
GameplayForestGroundCoverStemUv3D(GameplayForestGroundCoverAtlas3D atlas) noexcept {
  constexpr float inverseExtent = 1.f / 1024.f;
  return atlas == GameplayForestGroundCoverAtlas3D::IslandTree01
             ? GameplayForestGroundCoverUvRect3D{
                   78.f * inverseExtent, 267.f * inverseExtent,
                   86.f * inverseExtent, 275.f * inverseExtent}
             : GameplayForestGroundCoverUvRect3D{
                   126.f * inverseExtent, 111.f * inverseExtent,
                   134.f * inverseExtent, 119.f * inverseExtent};
}

// 線性空間乘數只壓低樹冠貼圖能量；alpha 維持原值，避免改變遮罩輪廓。
[[nodiscard]] constexpr Aevrynd::Color GameplayForestGroundCoverTintMultiplier3D(
    GameplayForestGroundCoverVariant3D variant) noexcept {
  switch (variant) {
    case GameplayForestGroundCoverVariant3D::Fern:
      return {.70f, .78f, .62f, 1.f};
    case GameplayForestGroundCoverVariant3D::Riverbank:
      return {.68f, .73f, .76f, 1.f};
    case GameplayForestGroundCoverVariant3D::Community:
      // The production TreeSmall02 material is already colour authored.  A
      // second warm multiplier removed roughly 28% of its linear luminance
      // and turned the middle layer into black fragments at dusk/night.
      return {1.f, 1.f, 1.f, 1.f};
  }
  return {1.f, 1.f, 1.f, 1.f};
}

// 三種地被共用既有葉片 atlas 的已驗證子區域；呼叫端必須提交含 alpha
// 的對應貼圖，並沿用這份 mask/double-sided 契約。
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

// 公尺建模的蕨與灌叢必須維持低矮、橫向展開的林下比例。舊版只放大 Y
// 至 2.2~3.2 倍，使約半公尺的植株變成 1.2~1.8m 的瘦高碎葉；R102
// 將高度限制在既有水平 footprint 可支撐的範圍。位置、identity、碰撞、
// triangle 與 draw-call 全部保持不變。
[[nodiscard]] constexpr Aevrynd::Vec3
GameplayForestGroundCoverPresentationScale3D(
    Aevrynd::Vec3 ecologyScale,
    GameplayForestGroundCoverVariant3D variant,
    std::uint32_t lodGroup) noexcept {
  ecologyScale = GameplayForestGroundCoverWorldScale3D(ecologyScale);
  if (variant == GameplayForestGroundCoverVariant3D::Riverbank)
    return ecologyScale;
  if (variant == GameplayForestGroundCoverVariant3D::Fern)
    ecologyScale.y = lodGroup == 0 ? 1.4f : lodGroup == 1 ? 1.25f : 1.05f;
  else if (variant == GameplayForestGroundCoverVariant3D::Community)
    ecologyScale.y = lodGroup == 0 ? 1.1f : lodGroup == 1 ? 1.05f : 1.f;
  return ecologyScale;
}

[[nodiscard]] constexpr GameplayForestGroundCoverAlphaCardContract3D
GameplayForestGroundCoverAlphaCardContract() noexcept {
  return {};
}

// 樹冠與林下地被共用同一份葉片 atlas，但用途不同：樹冠需要較高門檻
// 保持枝葉間隙，縮小後的蕨與低灌叢則需要保留亞像素葉片。此門檻只在
// ground-cover 變體提交時使用，不改寫來源材質、mip、樹木或其他 foliage。
[[nodiscard]] constexpr float GameplayForestGroundCoverAlphaCutoff3D(
    GameplayForestGroundCoverVariant3D) noexcept {
  return .34f;
}

[[nodiscard]] inline Aevrynd::Material3D
PrepareGameplayForestGroundCoverMaterial3D(Aevrynd::Material3D material,
                                           bool castsShadow = false) noexcept {
  const float authoredAlphaCutoff = material.alphaCutoff;
  const bool hasAuthoredMaskCutoff =
      material.alphaMode == Aevrynd::MaterialAlphaMode3D::Mask &&
      authoredAlphaCutoff > 0.f && authoredAlphaCutoff < 1.f;
  material.materialClass = Aevrynd::MaterialClass3D::Foliage;
  material.alphaMode = Aevrynd::MaterialAlphaMode3D::Mask;
  material.alphaCutoff = hasAuthoredMaskCutoff
                             ? authoredAlphaCutoff
                             : GameplayForestGroundCoverAlphaCardContract()
                                   .alphaCutoff;
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

[[nodiscard]] inline Aevrynd::Material3D
PrepareGameplayForestGroundCoverMaterial3D(
    Aevrynd::Material3D material,
    GameplayForestGroundCoverVariant3D variant,
    bool castsShadow = false) noexcept {
  material = PrepareGameplayForestGroundCoverMaterial3D(material, castsShadow);
  material.alphaCutoff = GameplayForestGroundCoverAlphaCutoff3D(variant);
  const auto multiplier = GameplayForestGroundCoverTintMultiplier3D(variant);
  material.tint = {material.tint.r * multiplier.r,
                   material.tint.g * multiplier.g,
                   material.tint.b * multiplier.b,
                   material.tint.a};
  return material;
}

[[nodiscard]] constexpr std::size_t GameplayForestGroundCoverTriangleBudget(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 88;
    case GameplayForestGroundCoverDetail3D::Mid: return 48;
    case GameplayForestGroundCoverDetail3D::Far: return 32;
  }
  return 0;
}

[[nodiscard]] constexpr std::size_t GameplayForestGroundCoverCommunityTriangleBudget(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 138;
    case GameplayForestGroundCoverDetail3D::Mid: return 80;
    case GameplayForestGroundCoverDetail3D::Far: return 46;
  }
  return 0;
}

// Fallen branch cards are baked into the existing Community mesh.  Their LOD
// count is explicit so tests and future budget changes cannot silently turn
// this floor-detail layer into another draw or placement resource.
[[nodiscard]] constexpr std::size_t GameplayForestGroundCoverBranchCount(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 5;
    case GameplayForestGroundCoverDetail3D::Mid: return 3;
    case GameplayForestGroundCoverDetail3D::Far: return 1;
  }
  return 0;
}

// 河岸專用單一實例網格：低伏落葉／苔蘚先覆蓋濕土，再以縮短的蕨葉
// 提供垂直輪廓。它不含灌木，避免水線旁形成不自然的等高綠牆。
[[nodiscard]] constexpr std::size_t
GameplayForestRiverbankGroundCoverTriangleBudget(
    GameplayForestGroundCoverDetail3D detail) noexcept {
  switch (detail) {
    case GameplayForestGroundCoverDetail3D::Near: return 108;
    case GameplayForestGroundCoverDetail3D::Mid: return 62;
    case GameplayForestGroundCoverDetail3D::Far: return 38;
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
