#pragma once

#include "GameplayShowcaseEnvironment3D.h"

#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/PhotorealAssetProfile3D.h>
#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {
class Engine;
class Scene3D;
class SceneECSRegistry3D;
struct Material3D;
struct MaterialAssetDefinition3D;
struct CookedRenderManifestStats3D;
}

namespace GameplayLab {

// Applies the authored Showcase response only to the seven living-tree bark
// slots. Non-bark ids and malformed bark definitions fail closed before any
// field is changed.
[[nodiscard]] bool ApplyShowcaseProductionBarkResponse3D(
    std::string_view materialId,
    Aevrynd::MaterialAssetDefinition3D &definition) noexcept;

// Applies the authored canopy response only to the three living-tree leaf slots.
// The shared material count and texture identities stay unchanged; invalid or
// non-leaf definitions fail closed before any field is changed.
[[nodiscard]] bool ApplyShowcaseProductionLeafResponse3D(
    std::string_view materialId,
    Aevrynd::MaterialAssetDefinition3D &definition) noexcept;

// Applies a forest-integrated response only to the authored deadwood and
// boulder slots. Other ids and malformed definitions fail closed before any
// field is changed.
[[nodiscard]] bool ApplyShowcaseProductionWeatheredPropResponse3D(
    std::string_view materialId,
    Aevrynd::MaterialAssetDefinition3D &definition) noexcept;

enum class ShowcaseCookedResourceKind3D : std::uint8_t {
  Mesh,
  Material,
  Ibl
};

struct ShowcaseCookedDocumentReceipt3D {
  std::string relativePath;
  std::string sha256;
};

struct ShowcaseCookedResourceRequest3D {
  ShowcaseCookedResourceKind3D kind{};
  std::string_view assetId;
  std::string_view sourceSha256;
  std::string_view cookedSha256;
  std::uint64_t claimedHandle{};
  std::uint32_t lodIndex{};
  std::uint32_t submeshIndex{};
  std::uint32_t materialIndex{};
};

struct ShowcaseCookedResourceLease3D {
  ShowcaseCookedResourceKind3D kind{};
  std::string assetId;
  std::string sourceSha256;
  std::string cookedSha256;
  std::uint64_t handle{};
  std::uint64_t ownerIdentity{};
  std::uint32_t generation{};
  std::uint64_t cpuBytes{};
  std::uint64_t gpuBytes{};
  std::shared_ptr<const void> lifetime;
  bool validated{};
  bool uploadComplete{};
};

class ShowcaseCookedResourceResolver3D {
public:
  virtual ~ShowcaseCookedResourceResolver3D() = default;
  virtual bool VerifyReceipt(std::string_view assetId,
                             std::uint32_t schemaVersion,
                             std::span<const ShowcaseCookedDocumentReceipt3D>
                                 documents) = 0;
  virtual bool Acquire(const ShowcaseCookedResourceRequest3D &request,
                       ShowcaseCookedResourceLease3D &lease) = 0;
};

struct ShowcaseCookedMeshPart3D {
  std::uint64_t meshHandle{};
  std::uint64_t materialHandle{};
  std::uint32_t submeshIndex{};
  std::uint32_t lodIndex{};
  std::string sourceSha256;
  std::string cookedSha256;
};

struct ShowcaseCookedMaterial3D {
  std::uint64_t materialHandle{};
  std::uint32_t materialIndex{};
  std::string sourceSha256;
  std::string cookedSha256;
};

struct ShowcaseCookedAssetReceipt3D {
  std::uint32_t schemaVersion{};
  std::string assetId;
  std::string sha256;
  std::vector<ShowcaseCookedDocumentReceipt3D> documents;
  std::array<std::uint64_t, 3> meshLodHandles{};
  std::vector<ShowcaseCookedMeshPart3D> meshParts;
  std::vector<ShowcaseCookedMaterial3D> materials;
  std::uint64_t iblHandle{};
  std::string iblSourceSha256;
  std::string iblCookedSha256;
  std::uint64_t cpuBytes{};
  std::uint64_t gpuBytes{};
  bool cooked{};
  bool materialSidecarBound{};
};

struct ShowcaseResolvedRenderBinding3D {
  std::size_t descriptorIndex{};
  ShowcaseSceneKind3D kind{};
  std::uint32_t lodGroup{};
  std::string assetId;
  std::array<std::uint64_t, 3> meshLodHandles{};
  std::vector<ShowcaseCookedMeshPart3D> meshParts;
  std::vector<std::uint64_t> materialHandles;
  bool renderOnly{};
  bool fallback{};
  bool photorealActive{};
};

struct ShowcaseRenderAssetLoadReceipt3D {
  std::uint64_t sequence{};
  std::uint64_t descriptorFingerprint{};
  std::uint64_t assetIdentityFingerprint{};
  std::size_t descriptorCount{};
  std::size_t resolvedCount{};
  std::size_t fallbackCount{};
  std::size_t admittedAssets{};
  std::array<std::size_t, 3> terrainLayerDescriptors{};
  std::uint32_t requiredAssetIdentityMask{};
  bool collisionStableIdsUnchanged{};
  bool assetIdentitiesExact{};
  bool descriptorOrderDeterministic{};
  bool photorealActive{};
  bool loaded{};
};

struct ShowcaseRenderAssetReleaseReceipt3D {
  std::uint64_t sequence{};
  std::size_t bindingsReleased{};
  std::size_t assetsUnloaded{};
  std::uint64_t retainedBytesAfter{};
};

struct ShowcaseProductionRenderReceipt3D {
  std::uint64_t descriptorFingerprint{};
  std::uint64_t manifestFingerprint{};
  std::string manifestSha256;
  std::uint64_t packageBytes{};
  std::size_t payloadFiles{};
  std::size_t validatedPayloadFiles{};
  std::size_t profileAssets{};
  std::size_t materialSets{};
  std::size_t validatedMaterialSets{};
  std::size_t meshParts{};
  std::size_t validatedMeshParts{};
  std::size_t validatedReferencedFiles{};
  std::size_t meshLodLevels{};
  std::size_t iblFiles{};
  std::size_t visualFirReplacements{};
  std::size_t barkMaterialResponses{};
  std::uint64_t barkMaterialResponseFingerprint{};
  std::size_t weatheredPropMaterialResponses{};
  std::uint64_t weatheredPropMaterialResponseFingerprint{};
  bool uploadComplete{};
  bool collisionStableIdsUnchanged{};
  bool photorealActive{};
};

[[nodiscard]] bool AssignShowcaseProductionManifestEvidence3D(
    const Aevrynd::CookedRenderManifestStats3D &stats,
    ShowcaseProductionRenderReceipt3D &receipt) noexcept;

struct ShowcaseProductionReleaseReceipt3D {
  std::size_t leasesReleased{};
  std::size_t meshesReleased{};
  std::size_t materialsReleased{};
  bool rendererBaselineRestored{};
  std::size_t baselineMeshes{}, releasedMeshes{};
  std::size_t baselineMeshReferences{}, releasedMeshReferences{};
  std::size_t baselineMaterials{}, releasedMaterials{};
  std::size_t baselineMaterialReferences{}, releasedMaterialReferences{};
  std::size_t baselineIbl{}, releasedIbl{};
  std::uint64_t pendingGpuBytes{}, pendingIblBytes{};
};

struct ShowcaseProductionSubmissionStats3D {
  std::size_t normalizedAssets{};
  std::size_t normalizationRejected{};
  std::size_t meshChainsSubmitted{};
  std::size_t meshChainsRejected{};
};

enum class ShowcaseHlodDescriptorPolicy3D : std::uint8_t {
  AggregateOpaque,
  DedicatedTerrain,
  DedicatedWater,
  DedicatedVegetation,
  DedicatedBuilding,
  FailClosed
};

[[nodiscard]] ShowcaseHlodDescriptorPolicy3D
ResolveShowcaseHlodDescriptorPolicy3D(ShowcaseSceneKind3D kind) noexcept;

struct ShowcaseAggregateProxySource3D {
  const Aevrynd::Mesh3D *mesh{};
  Aevrynd::Transform3D transform{};
  Aevrynd::StableObjectId3D objectId{};
  std::uint64_t generation{}, meshContentIdentity{}, materialIdentity{},
      resourceOwnerIdentity{};
};

struct ShowcaseAggregateProxyReceipt3D {
  std::uint64_t sourceContractIdentity{}, aggregateContentIdentity{};
  std::array<std::uint8_t, 32> aggregateContentDigest{};
  std::size_t sourceInstances{}, sourceVertices{}, sourceTriangles{},
      aggregateVertices{}, aggregateTriangles{};
  bool materialCompatible{}, immutableIdentityValidated{};

  [[nodiscard]] constexpr bool Complete() const noexcept {
    return sourceContractIdentity && aggregateContentIdentity &&
        aggregateContentDigest != std::array<std::uint8_t, 32>{} &&
        sourceInstances >= 2 && aggregateVertices &&
        sourceVertices && aggregateTriangles &&
        aggregateTriangles < sourceTriangles && materialCompatible &&
        immutableIdentityValidated;
  }
};

[[nodiscard]] std::array<std::uint8_t, 32>
ShowcaseAggregateProxyContentDigest3D(const Aevrynd::Mesh3D &mesh) noexcept;

[[nodiscard]] bool BuildShowcaseAggregateProxyMesh3D(
    std::span<const ShowcaseAggregateProxySource3D> sources,
    Aevrynd::Mesh3D &mesh,
    ShowcaseAggregateProxyReceipt3D &receipt) noexcept;
[[nodiscard]] bool ValidateShowcaseAggregateProxyMesh3D(
    const Aevrynd::Mesh3D &mesh,
    const ShowcaseAggregateProxyReceipt3D &receipt) noexcept;

// 僅供敵意測試走過與正式路徑相同的 pinned cache transaction；不載入 GPU。
[[nodiscard]] bool ExerciseShowcaseAggregateProxyCache3DForTesting(
    const std::filesystem::path &assetRoot, const Aevrynd::Mesh3D &mesh,
    const ShowcaseAggregateProxyReceipt3D &receipt) noexcept;

struct ShowcaseProductionHlodBuildReceipt3D {
  std::uint64_t descriptorFingerprint{}, descriptorPolicyHash{},
      hlodDataHash{}, hlodManifestHash{}, aggregateProxyContentIdentity{},
      aggregateResourceContractIdentity{};
  std::size_t inputDescriptors{}, eligibleRockDescriptors{}, sourceInstances{},
      clusters{}, excludedTerrain{}, excludedWater{}, excludedTrees{},
      excludedGrass{}, excludedBuildings{}, aggregateProxyVertices{},
      aggregateProxyTriangles{};
  std::uint8_t coveredDescriptorKindMask{};
  bool usedProductionAssetLeases{}, representativeLowLodProxy{},
      aggregateProxyBuilt{}, aggregateResourceContractsValidated{},
      aggregateIdentityValidated{}, allDescriptorKindsCovered{};

  [[nodiscard]] constexpr bool Complete() const noexcept {
    return descriptorFingerprint && hlodDataHash && hlodManifestHash &&
        inputDescriptors && eligibleRockDescriptors == sourceInstances &&
        clusters && aggregateProxyContentIdentity &&
        aggregateResourceContractIdentity && aggregateProxyVertices &&
        aggregateProxyTriangles && usedProductionAssetLeases &&
        !representativeLowLodProxy && aggregateProxyBuilt &&
        aggregateResourceContractsValidated && aggregateIdentityValidated &&
        allDescriptorKindsCovered && coveredDescriptorKindMask == 0x3fu;
  }
};

struct ShowcaseAssetBounds3D {
  Aevrynd::Vec3 minimum{};
  Aevrynd::Vec3 maximum{};
};

struct ShowcaseProductionSubmittedLodChain3D {
  Aevrynd::RenderObjectIdentity3D identity{};
  std::uint32_t submesh{}, lodObjectIndex{}, lodSeedBase{};
  Aevrynd::Transform3D normalizedTransform{};
  Aevrynd::MeshBounds3D localUnionBounds{};
  std::array<float, 3> switchDistances{}, transitionWidths{};
  std::uint32_t levelCount{};
};

// 以整個資產的聯集邊界做一次等比正規化，所有子網格必須共用結果。
[[nodiscard]] bool NormalizeShowcaseAssetTransform3D(
    const ShowcaseAssetBounds3D &sourceBounds,
    const Aevrynd::Transform3D &targetBounds,
    Aevrynd::Transform3D &normalized) noexcept;
[[nodiscard]] bool ValidateShowcaseAssetLodBounds3D(
    std::span<const Aevrynd::MeshBounds3D> lodBounds) noexcept;
[[nodiscard]] std::string_view ResolveShowcasePhotorealAssetId3D(
    const ShowcaseSceneDescriptor3D &descriptor) noexcept;
// Resolves the production-only visual species without mutating the semantic
// descriptor. Every fifth living-tree ordinal is rendered as fir_tree_01_c;
// dead trees and all non-tree descriptors retain their semantic asset id.
[[nodiscard]] std::string_view ResolveShowcaseProductionVisualAssetId3D(
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    std::size_t descriptorIndex) noexcept;
[[nodiscard]] std::uint64_t ShowcaseAssetIdentityFingerprint3D(
    std::span<const ShowcaseSceneDescriptor3D> descriptors) noexcept;

class ShowcaseRenderAssetBindings3D {
public:
  ShowcaseRenderAssetBindings3D();
  ~ShowcaseRenderAssetBindings3D();
  bool Load(std::span<const ShowcaseSceneDescriptor3D> descriptors,
            const Aevrynd::PhotorealAssetCatalog3D &catalog,
            const Aevrynd::PhotorealRuntimeManifest3D &runtime,
            Aevrynd::PhotorealAssetProfile3D &profile,
            ShowcaseCookedResourceResolver3D &resolver,
            std::span<const ShowcaseCookedAssetReceipt3D> cooked,
            ShowcaseRenderAssetLoadReceipt3D &receipt);
  [[nodiscard]] const ShowcaseResolvedRenderBinding3D *
  Resolve(std::size_t descriptorIndex) const noexcept;
  [[nodiscard]] const ShowcaseResolvedRenderBinding3D *
  ResolvePath() const noexcept;
  [[nodiscard]] std::uint64_t IblHandle() const noexcept { return iblHandle_; }
  [[nodiscard]] std::span<const std::string> VisibleAssetIds() const noexcept {
    return visibleAssetIds_;
  }
  [[nodiscard]] bool PhotorealActive() const noexcept { return active_; }
  Aevrynd::PhotorealAssetTrimReceipt3D
  Trim(std::uint64_t targetCpuBytes, std::uint64_t targetGpuBytes);
  ShowcaseRenderAssetReleaseReceipt3D Release();

  bool LoadProduction(Aevrynd::Engine &engine,
                      Aevrynd::SceneECSRegistry3D &registry,
                      const std::filesystem::path &manifestPath,
                      std::span<const ShowcaseSceneDescriptor3D> descriptors,
                      ShowcaseProductionRenderReceipt3D &receipt);
  [[nodiscard]] const Aevrynd::Material3D *
  ProductionMaterial(std::string_view assetId) const noexcept;
  [[nodiscard]] const Aevrynd::Material3D *
  ProductionMaterialById(std::string_view materialId) const noexcept;
  bool SubmitProductionMesh(Aevrynd::Scene3D &scene,
                            std::string_view assetId,
                            const Aevrynd::Transform3D &transform,
                            Aevrynd::RenderObjectIdentity3D identity = {},
                            std::uint32_t firstLodObjectIndex = 0,
                            std::vector<ShowcaseProductionSubmittedLodChain3D> *submitted = nullptr) const;
  [[nodiscard]] bool ProductionActive() const noexcept;
  [[nodiscard]] ShowcaseProductionSubmissionStats3D
  ProductionSubmissionStats() const noexcept;
  void ResetProductionSubmissionStats() noexcept;
  // 由正式 cooked 資產租約建立 Gameplay 世界的第一條 HLOD 分區資料；目前只納入
  // opaque rock，其他語意在有相容 proxy 前維持既有提交路徑。
  bool BuildProductionHlodChunk3D(
      std::span<const ShowcaseSceneDescriptor3D> descriptors,
      Aevrynd::StableObjectId3D worldId,
      Aevrynd::GlobalChunkCoordinate3D chunk,
      std::shared_ptr<Aevrynd::TerrainVegetationChunkPayload3D> &payload,
      ShowcaseProductionHlodBuildReceipt3D &receipt) const;
  [[nodiscard]] Aevrynd::StaticMeshAssetLibrary3D *
  ProductionMeshLibrary3D() noexcept;
  ShowcaseProductionReleaseReceipt3D
  ReleaseProduction(Aevrynd::Engine &engine, Aevrynd::Scene3D &scene);

private:
  Aevrynd::PhotorealAssetProfile3D *profile_{};
  std::vector<ShowcaseResolvedRenderBinding3D> bindings_;
  ShowcaseResolvedRenderBinding3D pathBinding_{};
  std::vector<std::string> visibleAssetIds_;
  std::vector<std::string> ownedAssets_;
  std::vector<ShowcaseCookedResourceLease3D> resourceLeases_;
  std::uint64_t iblHandle_{};
  std::uint64_t sequence_{};
  bool active_{};
  struct ProductionState;
  std::unique_ptr<ProductionState> production_;
};

} // namespace GameplayLab
