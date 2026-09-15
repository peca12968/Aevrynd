#pragma once

#include <Aevrynd/CookedRenderManifest3D.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace GameplayLab {

enum class ShowcaseTerrainMaterialFailure3D : std::uint8_t {
  None,
  MissingLayer,
  InvalidPackedOrm,
  MissingSemanticIdentity,
  DuplicateSemanticIdentity
};

struct ShowcaseTerrainMaterialReceipt3D {
  ShowcaseTerrainMaterialFailure3D failure{
      ShowcaseTerrainMaterialFailure3D::None};
  std::array<std::uint64_t, 3> semanticIdentities{};
};

[[nodiscard]] inline bool ValidateShowcaseTerrainMaterialSources3D(
    const std::array<const Aevrynd::CookedRenderMaterial3D *, 3> &materials,
    ShowcaseTerrainMaterialReceipt3D *receipt = nullptr) {
  ShowcaseTerrainMaterialReceipt3D result{};
  const auto reject = [&](ShowcaseTerrainMaterialFailure3D failure) {
    result.failure = failure;
    if (receipt) *receipt = result;
    return false;
  };
  for (std::size_t index = 0; index < materials.size(); ++index) {
    if (!materials[index])
      return reject(ShowcaseTerrainMaterialFailure3D::MissingLayer);
    const auto &material = *materials[index];
    if (material.surfaceEncoding !=
        Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrm)
      return reject(ShowcaseTerrainMaterialFailure3D::InvalidPackedOrm);
    if (!material.baseColorBytes || !material.surfaceBytes ||
        !material.normalBytes || material.baseColorSha256.size() != 64u ||
        material.surfaceSha256.size() != 64u ||
        material.normalSha256.size() != 64u)
      return reject(ShowcaseTerrainMaterialFailure3D::MissingSemanticIdentity);
    std::uint64_t identity = 1469598103934665603ull;
    const auto mix = [&](std::string_view value) {
      for (const unsigned char byte : value) {
        identity ^= byte;
        identity *= 1099511628211ull;
      }
      identity ^= 0xffu;
      identity *= 1099511628211ull;
    };
    mix(material.baseColorSha256);
    mix(material.surfaceSha256);
    mix(material.normalSha256);
    result.semanticIdentities[index] = identity ? identity : 1u;
  }
  if (result.semanticIdentities[0] == result.semanticIdentities[1] ||
      result.semanticIdentities[0] == result.semanticIdentities[2] ||
      result.semanticIdentities[1] == result.semanticIdentities[2])
    return reject(ShowcaseTerrainMaterialFailure3D::DuplicateSemanticIdentity);
  if (receipt) *receipt = result;
  return true;
}

// 由三組真實 cooked PBR 材質組成單一 terrain draw。身分取自封存來源
// payload，而不是 descriptor 數量；重複使用同一層時必須拒絕。
[[nodiscard]] inline bool BuildShowcaseTerrainMaterialTextures3D(
    const Aevrynd::CookedRenderManifest3D &manifest,
    const std::filesystem::path &assetRoot,
    Aevrynd::MaterialTextureSetDefinition3D &textures,
    ShowcaseTerrainMaterialReceipt3D *receipt = nullptr) {
  ShowcaseTerrainMaterialReceipt3D result{};
  constexpr std::array<std::string_view, 3> ids{
      "forest_ground_04:slot-0", "Ground108:slot-0",
      "stony_dirt_path:slot-0"};
  std::array<const Aevrynd::CookedRenderMaterial3D *, 3> materials{};
  for (std::size_t index = 0; index < ids.size(); ++index) {
    materials[index] = manifest.FindMaterial(ids[index]);
  }
  if (!ValidateShowcaseTerrainMaterialSources3D(materials, &result)) {
    if (receipt) *receipt = result;
    return false;
  }

  const auto absoluteDefinition = [&](std::size_t index) {
    auto definition = materials[index]->Definition(2048).textures;
    definition.baseColorPath = assetRoot / definition.baseColorPath;
    definition.roughnessPath = assetRoot / definition.roughnessPath;
    definition.normalPath = assetRoot / definition.normalPath;
    return definition;
  };
  textures = absoluteDefinition(0);
  const auto soil = absoluteDefinition(1);
  const auto path = absoluteDefinition(2);
  textures.additionalLayers[0] = {
      soil.baseColorPath, soil.roughnessPath, soil.normalPath,
      soil.surfaceEncoding, soil.integrity};
  textures.additionalLayers[1] = {
      path.baseColorPath, path.roughnessPath, path.normalPath,
      path.surfaceEncoding, path.integrity};
  result.failure = ShowcaseTerrainMaterialFailure3D::None;
  if (receipt) *receipt = result;
  return true;
}

} // namespace GameplayLab
