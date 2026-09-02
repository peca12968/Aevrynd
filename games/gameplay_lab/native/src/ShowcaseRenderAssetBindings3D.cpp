#include "ShowcaseRenderAssetBindings3D.h"

#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/EnvironmentIbl3D.h>
#include <Aevrynd/StaticMeshAssetLibrary3D.h>

#include <algorithm>
#include <bit>
#include <limits>
#include <unordered_set>
#include <unordered_map>

namespace GameplayLab {

bool AssignShowcaseProductionManifestEvidence3D(
    const Aevrynd::CookedRenderManifestStats3D &stats,
    ShowcaseProductionRenderReceipt3D &receipt) noexcept {
  receipt.manifestFingerprint = 0;
  receipt.manifestSha256.clear();
  receipt.packageBytes = 0;
  receipt.payloadFiles = 0;
  receipt.validatedPayloadFiles = 0;
  receipt.materialSets = 0;
  receipt.validatedMaterialSets = 0;
  receipt.meshParts = 0;
  receipt.validatedMeshParts = 0;
  receipt.validatedReferencedFiles = 0;
  const bool validSha = stats.manifestSha256.size() == 64 &&
      std::ranges::all_of(stats.manifestSha256, [](char value) {
        return (value >= '0' && value <= '9') ||
               (value >= 'a' && value <= 'f');
      });
  if (!validSha || !stats.fingerprint || !stats.aggregateBytes ||
      !stats.files || !stats.materials || !stats.meshParts ||
      stats.validatedFiles != stats.files ||
      stats.validatedMaterials != stats.materials ||
      stats.validatedMeshParts != stats.meshParts ||
      stats.validatedReferencedFiles != stats.referencedFiles)
    return false;
  receipt.manifestFingerprint = stats.fingerprint;
  receipt.manifestSha256 = stats.manifestSha256;
  receipt.packageBytes = stats.aggregateBytes;
  receipt.payloadFiles = stats.files;
  receipt.validatedPayloadFiles = stats.validatedFiles;
  receipt.materialSets = stats.materials;
  receipt.validatedMaterialSets = stats.validatedMaterials;
  receipt.meshParts = stats.meshParts;
  receipt.validatedMeshParts = stats.validatedMeshParts;
  receipt.validatedReferencedFiles = stats.validatedReferencedFiles;
  return true;
}

struct ShowcaseRenderAssetBindings3D::ProductionState {
  struct AssetBounds { ShowcaseAssetBounds3D bounds{}; bool valid{}; };
  Aevrynd::RendererStats baseline{};
  Aevrynd::CookedRenderManifest3D manifest{};
  Aevrynd::MaterialLibrary3D materials{};
  Aevrynd::StaticMeshAssetLibrary3D meshes{};
  Aevrynd::EnvironmentLightingStore3D environment{};
  Aevrynd::EnvironmentLightingHandle3D environmentHandle{};
  std::vector<Aevrynd::StaticMeshRenderableLease3D> leases{};
  std::unordered_map<std::string, std::vector<std::size_t>> assetLeases{};
  std::unordered_map<std::string, AssetBounds> assetBounds{};
  ShowcaseProductionSubmissionStats3D submissionStats{};
  Aevrynd::EnvironmentLightingGpuReceipt3D createReceipt{}, setReceipt{};
  std::uint64_t expectedCompressedBytes{};
  bool active{};
};

ShowcaseRenderAssetBindings3D::ShowcaseRenderAssetBindings3D() = default;
ShowcaseRenderAssetBindings3D::~ShowcaseRenderAssetBindings3D() = default;

namespace {

std::uint64_t DescriptorFingerprint(
    std::span<const ShowcaseSceneDescriptor3D> descriptors) {
  std::uint64_t hash = 1469598103934665603ull;
  const auto add = [&hash](const void *data, std::size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index) {
      hash ^= bytes[index];
      hash *= 1099511628211ull;
    }
  };
  for (const auto &descriptor : descriptors) {
    const auto kind = static_cast<std::uint8_t>(descriptor.kind);
    add(&kind, sizeof(kind));
    const std::array positionBits{
        std::bit_cast<std::uint32_t>(descriptor.position.x),
        std::bit_cast<std::uint32_t>(descriptor.position.y),
        std::bit_cast<std::uint32_t>(descriptor.position.z)};
    const std::array scaleBits{
        std::bit_cast<std::uint32_t>(descriptor.scale.x),
        std::bit_cast<std::uint32_t>(descriptor.scale.y),
        std::bit_cast<std::uint32_t>(descriptor.scale.z)};
    add(positionBits.data(), sizeof(positionBits));
    add(scaleBits.data(), sizeof(scaleBits));
    const auto yaw = std::bit_cast<std::uint32_t>(descriptor.yaw);
    add(&yaw, sizeof(yaw));
    add(&descriptor.lodGroup, sizeof(descriptor.lodGroup));
    const std::uint8_t castsShadow = descriptor.castsShadow ? 1u : 0u;
    add(&castsShadow, sizeof(castsShadow));
    const auto meshBytes = static_cast<std::uint64_t>(descriptor.meshAsset.size());
    const auto materialBytes =
        static_cast<std::uint64_t>(descriptor.materialAsset.size());
    add(&meshBytes, sizeof(meshBytes));
    add(descriptor.meshAsset.data(), descriptor.meshAsset.size());
    add(&materialBytes, sizeof(materialBytes));
    add(descriptor.materialAsset.data(), descriptor.materialAsset.size());
  }
  return hash;
}

std::string_view AssetFor(const ShowcaseSceneDescriptor3D &descriptor) {
  switch (descriptor.kind) {
  case ShowcaseSceneKind3D::Terrain:
  case ShowcaseSceneKind3D::Grass:
    return descriptor.kind == ShowcaseSceneKind3D::Terrain ? "forest_ground_04"
                                                           : "Ground108";
  case ShowcaseSceneKind3D::Water:
    // 水面維持專用 water-optics shader；Ground108 只供河床／河岸層使用。
    return {};
  case ShowcaseSceneKind3D::Tree:
    if (descriptor.meshAsset == "poly-haven/island_tree_01/cooked-lod")
      return "island_tree_01";
    if (descriptor.meshAsset == "poly-haven/dead_tree_trunk_02/cooked-lod")
      return "dead_tree_trunk_02";
    return {};
  case ShowcaseSceneKind3D::Rock:
    if (descriptor.meshAsset == "poly-haven/boulder_01/cooked-lod") return "boulder_01";
    if (descriptor.meshAsset == "poly-haven/coastal_cliff_01/cooked-lod") return "coastal_cliff_01";
    return {};
  case ShowcaseSceneKind3D::Building:
    return descriptor.meshAsset.find("roof") != std::string::npos
               ? "RoofingTiles014B"
               : "Bricks100";
  }
  return {};
}

bool IsComplete(const Aevrynd::PhotorealCatalogAsset3D &asset,
                const Aevrynd::PhotorealRuntimeAssetReceipt3D &runtime,
                const ShowcaseCookedAssetReceipt3D &cooked) {
  const auto validSha = [](std::string_view value) {
    return value.size() == 64u && std::ranges::all_of(value, [](char item) {
             return (item >= '0' && item <= '9') || (item >= 'a' && item <= 'f');
           });
  };
  const auto validPath = [](std::string_view value) {
    if (value.empty() || value.size() > 512u || value.front() == '/' ||
        value.find('\\') != value.npos || value.find(':') != value.npos)
      return false;
    std::size_t start{};
    while (start < value.size()) {
      const auto end = value.find('/', start);
      const auto part = value.substr(
          start, end == value.npos ? value.size() - start : end - start);
      if (part.empty() || part == "." || part == "..")
        return false;
      if (end == value.npos)
        break;
      start = end + 1;
    }
    return true;
  };
  if (cooked.schemaVersion != 1u || !cooked.cooked ||
      !validSha(cooked.sha256) || cooked.documents.empty() ||
      !runtime.licensePassed ||
      !runtime.memoryGatePassed || !runtime.hashesPassed ||
      !runtime.channelsPassed ||
      std::ranges::find(runtime.acceptedSha256, cooked.sha256) ==
          runtime.acceptedSha256.end())
    return false;
  std::unordered_set<std::string_view> documentPaths;
  for (const auto &document : cooked.documents)
    if (!validPath(document.relativePath) || !validSha(document.sha256) ||
        !documentPaths.insert(document.relativePath).second)
      return false;
  if (asset.kind == Aevrynd::PhotorealAssetKind3D::MeshPbr) {
    const std::size_t expectedParts = asset.id == "island_tree_01" ? 3u : 1u;
    if (runtime.lodLevels != 3 || !cooked.materialSidecarBound ||
        !std::ranges::all_of(cooked.meshLodHandles,
                            [](std::uint64_t value) { return value != 0; }))
      return false;
    if (cooked.meshParts.size() != expectedParts * 3u ||
        cooked.materials.size() != expectedParts)
      return false;
    std::unordered_set<std::uint64_t> chainHandles;
    for (const auto handle : cooked.meshLodHandles)
      if (!handle || !chainHandles.insert(handle).second)
        return false;
    for (std::uint32_t lod = 0; lod < 3; ++lod)
      for (std::uint32_t part = 0; part < expectedParts; ++part) {
        const auto found = std::ranges::find_if(
            cooked.meshParts, [&](const auto &item) {
              return item.lodIndex == lod && item.submeshIndex == part;
            });
        if (found == cooked.meshParts.end() || !found->meshHandle ||
            !found->materialHandle ||
            found->materialHandle != cooked.materials[part].materialHandle ||
            !validSha(found->sourceSha256) ||
            !validSha(found->cookedSha256) ||
            std::ranges::find(runtime.acceptedSha256,
                              found->sourceSha256) ==
                runtime.acceptedSha256.end())
          return false;
        if (part == 0u && cooked.meshLodHandles[lod] != found->meshHandle)
          return false;
      }
    for (std::size_t index = 0; index < cooked.materials.size(); ++index) {
      const auto &material = cooked.materials[index];
      if (!material.materialHandle || material.materialIndex != index ||
          !validSha(material.sourceSha256) ||
          !validSha(material.cookedSha256) ||
          std::ranges::find(runtime.acceptedSha256,
                            material.sourceSha256) ==
              runtime.acceptedSha256.end())
        return false;
    }
    return true;
  }
  if (asset.kind == Aevrynd::PhotorealAssetKind3D::Hdri)
    return cooked.iblHandle != 0 && validSha(cooked.iblSourceSha256) &&
           validSha(cooked.iblCookedSha256) &&
           std::ranges::find(runtime.acceptedSha256,
                             cooked.iblSourceSha256) !=
               runtime.acceptedSha256.end();
  if (cooked.materials.empty())
    return false;
  for (std::size_t index = 0; index < cooked.materials.size(); ++index) {
    const auto &material = cooked.materials[index];
    if (!material.materialHandle || material.materialIndex != index ||
        !validSha(material.sourceSha256) ||
        !validSha(material.cookedSha256) ||
        std::ranges::find(runtime.acceptedSha256, material.sourceSha256) ==
            runtime.acceptedSha256.end())
      return false;
  }
  return true;
}

bool LeaseMatches(const ShowcaseCookedResourceRequest3D &request,
                  const ShowcaseCookedResourceLease3D &lease) {
  return lease.validated && lease.uploadComplete &&
         lease.kind == request.kind && lease.lifetime &&
         lease.handle == request.claimedHandle && lease.ownerIdentity &&
         lease.generation && lease.cpuBytes && lease.gpuBytes &&
         lease.assetId == request.assetId &&
         lease.sourceSha256 == request.sourceSha256 &&
         lease.cookedSha256 == request.cookedSha256;
}

} // namespace

std::string_view ResolveShowcasePhotorealAssetId3D(
    const ShowcaseSceneDescriptor3D &descriptor) noexcept {
  return AssetFor(descriptor);
}

bool ShowcaseRenderAssetBindings3D::Load(
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const Aevrynd::PhotorealAssetCatalog3D &catalog,
    const Aevrynd::PhotorealRuntimeManifest3D &runtime,
    Aevrynd::PhotorealAssetProfile3D &profile,
    ShowcaseCookedResourceResolver3D &resolver,
    std::span<const ShowcaseCookedAssetReceipt3D> cooked,
    ShowcaseRenderAssetLoadReceipt3D &receipt) {
  receipt = {};
  if (descriptors.empty() || !runtime.Passed() || active_)
    return false;
  const std::uint64_t descriptorFingerprint =
      DescriptorFingerprint(descriptors);
  std::unordered_map<std::string_view, const ShowcaseCookedAssetReceipt3D *>
      cookedById;
  for (const auto &item : cooked)
    if (item.assetId.empty() || !cookedById.emplace(item.assetId, &item).second)
      return false;

  std::vector<ShowcaseResolvedRenderBinding3D> candidate;
  std::vector<std::string> visible;
  std::vector<std::string> admitted;
  std::vector<std::string> leasedAssetIds;
  std::vector<ShowcaseCookedResourceLease3D> leases;
  candidate.reserve(descriptors.size());
  const auto rollback = [&] {
    for (auto it = admitted.rbegin(); it != admitted.rend(); ++it) {
      Aevrynd::PhotorealAssetResidencyReceipt3D ignored{};
      profile.Unload(*it, ignored);
    }
  };
  const auto admit = [&](std::string_view id) {
    const auto *asset = catalog.Find(id);
    const auto *runtimeAsset = runtime.Find(id);
    const auto found = cookedById.find(id);
    if (!asset || !runtimeAsset || found == cookedById.end() ||
        !IsComplete(*asset, *runtimeAsset, *found->second))
      return false;
    const auto &cook = *found->second;
    if (std::ranges::find(leasedAssetIds, std::string(id)) ==
        leasedAssetIds.end()) {
      if (!resolver.VerifyReceipt(id, cook.schemaVersion, cook.documents))
        return false;
      std::uint64_t verifiedCpuBytes{};
      std::uint64_t verifiedGpuBytes{};
      const auto acquire = [&](ShowcaseCookedResourceRequest3D request) {
        ShowcaseCookedResourceLease3D lease{};
        if (!resolver.Acquire(request, lease) || !LeaseMatches(request, lease))
          return false;
        if (lease.cpuBytes >
                (std::numeric_limits<std::uint64_t>::max)() -
                    verifiedCpuBytes ||
            lease.gpuBytes >
                (std::numeric_limits<std::uint64_t>::max)() -
                    verifiedGpuBytes)
          return false;
        verifiedCpuBytes += lease.cpuBytes;
        verifiedGpuBytes += lease.gpuBytes;
        leases.push_back(std::move(lease));
        return true;
      };
      if (asset->kind == Aevrynd::PhotorealAssetKind3D::MeshPbr) {
        for (const auto &part : cook.meshParts)
          if (!acquire({ShowcaseCookedResourceKind3D::Mesh, id,
                        part.sourceSha256, part.cookedSha256, part.meshHandle,
                        part.lodIndex, part.submeshIndex, 0}))
            return false;
        for (const auto &material : cook.materials)
          if (!acquire({ShowcaseCookedResourceKind3D::Material, id,
                        material.sourceSha256, material.cookedSha256,
                        material.materialHandle, 0, 0,
                        material.materialIndex}))
            return false;
      } else if (asset->kind == Aevrynd::PhotorealAssetKind3D::Hdri) {
        if (!acquire({ShowcaseCookedResourceKind3D::Ibl, id,
                      cook.iblSourceSha256, cook.iblCookedSha256,
                      cook.iblHandle, 0, 0, 0}))
          return false;
      } else {
        for (const auto &material : cook.materials)
          if (!acquire({ShowcaseCookedResourceKind3D::Material, id,
                        material.sourceSha256, material.cookedSha256,
                        material.materialHandle, 0, 0,
                        material.materialIndex}))
            return false;
      }
      if (verifiedCpuBytes != cook.cpuBytes ||
          verifiedGpuBytes != cook.gpuBytes)
        return false;
      leasedAssetIds.emplace_back(id);
    }
    if (profile.IsResident(id))
      return true;
    Aevrynd::PhotorealAssetResidencyRequest3D request{};
    request.assetId = std::string(id);
    request.receipt = {std::string(id), asset->source, found->second->sha256,
                       found->second->cpuBytes, found->second->cpuBytes,
                       runtimeAsset->channels, true};
    request.selection = profile.Select(
        *asset, {4.f, .5f, Aevrynd::PhotorealImportance3D::Important});
    request.cpuBytes = found->second->cpuBytes;
    request.gpuBytes = found->second->gpuBytes;
    Aevrynd::PhotorealAssetResidencyReceipt3D admittedReceipt{};
    if (!profile.Admit(request, admittedReceipt))
      return false;
    admitted.emplace_back(id);
    return true;
  };

  bool allResolved = true;
  for (std::size_t index = 0; index < descriptors.size(); ++index) {
    const auto id = ResolveShowcasePhotorealAssetId3D(descriptors[index]);
    ShowcaseResolvedRenderBinding3D binding{};
    binding.descriptorIndex = index;
    binding.kind = descriptors[index].kind;
    binding.lodGroup = descriptors[index].lodGroup;
    binding.assetId = id;
    binding.renderOnly = descriptors[index].kind == ShowcaseSceneKind3D::Water ||
                         descriptors[index].kind == ShowcaseSceneKind3D::Grass;
    if (id.empty() && descriptors[index].kind == ShowcaseSceneKind3D::Water) {
      binding.assetId = descriptors[index].materialAsset;
      candidate.push_back(std::move(binding));
      continue;
    }
    if (!admit(id)) {
      binding.fallback = true;
      allResolved = false;
    } else {
      const auto *asset = catalog.Find(id);
      const auto &cook = *cookedById.at(id);
      binding.meshLodHandles = cook.meshLodHandles;
      binding.meshParts = cook.meshParts;
      binding.materialHandles.reserve(cook.materials.size());
      for (const auto &material : cook.materials)
        binding.materialHandles.push_back(material.materialHandle);
      binding.photorealActive = asset->kind != Aevrynd::PhotorealAssetKind3D::Unknown;
      if (std::ranges::find(visible, std::string(id)) == visible.end())
        visible.emplace_back(id);
    }
    candidate.push_back(std::move(binding));
  }

  ShowcaseResolvedRenderBinding3D path{};
  path.assetId = "stony_dirt_path";
  path.renderOnly = true;
  if (!admit(path.assetId)) {
    path.fallback = true;
    allResolved = false;
  } else {
    for (const auto &material : cookedById.at(path.assetId)->materials)
      path.materialHandles.push_back(material.materialHandle);
    path.photorealActive = true;
    visible.emplace_back(path.assetId);
  }
  if (!admit("xanderklinge")) {
    allResolved = false;
  } else {
    iblHandle_ = cookedById.at("xanderklinge")->iblHandle;
    visible.emplace_back("xanderklinge");
  }
  if (!allResolved) {
    rollback();
    iblHandle_ = 0;
    receipt.sequence = ++sequence_;
    receipt.descriptorFingerprint = descriptorFingerprint;
    receipt.descriptorCount = descriptors.size();
    receipt.fallbackCount = std::ranges::count_if(
        candidate, [](const auto &item) { return item.fallback; });
    return false;
  }
  bindings_.swap(candidate);
  pathBinding_ = std::move(path);
  visibleAssetIds_.swap(visible);
  ownedAssets_.swap(admitted);
  resourceLeases_.swap(leases);
  profile_ = &profile;
  active_ = true;
  receipt.sequence = ++sequence_;
  receipt.descriptorFingerprint = descriptorFingerprint;
  receipt.descriptorCount = descriptors.size();
  receipt.resolvedCount = bindings_.size() + 1u;
  receipt.admittedAssets = ownedAssets_.size();
  receipt.collisionStableIdsUnchanged =
      DescriptorFingerprint(descriptors) == descriptorFingerprint;
  receipt.photorealActive = receipt.collisionStableIdsUnchanged;
  receipt.loaded = receipt.collisionStableIdsUnchanged;
  if (!receipt.loaded) {
    Release();
    receipt = {};
    return false;
  }
  return true;
}

const ShowcaseResolvedRenderBinding3D *
ShowcaseRenderAssetBindings3D::Resolve(std::size_t descriptorIndex) const noexcept {
  return active_ && descriptorIndex < bindings_.size()
             ? &bindings_[descriptorIndex]
             : nullptr;
}

const ShowcaseResolvedRenderBinding3D *
ShowcaseRenderAssetBindings3D::ResolvePath() const noexcept {
  return active_ ? &pathBinding_ : nullptr;
}

Aevrynd::PhotorealAssetTrimReceipt3D
ShowcaseRenderAssetBindings3D::Trim(std::uint64_t targetCpuBytes,
                                    std::uint64_t targetGpuBytes) {
  if (!profile_)
    return {};
  std::vector<std::string_view> protectedIds;
  protectedIds.reserve(visibleAssetIds_.size());
  for (const auto &id : visibleAssetIds_)
    protectedIds.emplace_back(id);
  return profile_->Trim(targetCpuBytes, targetGpuBytes, protectedIds);
}

ShowcaseRenderAssetReleaseReceipt3D ShowcaseRenderAssetBindings3D::Release() {
  ShowcaseRenderAssetReleaseReceipt3D receipt{};
  receipt.sequence = ++sequence_;
  receipt.bindingsReleased = bindings_.size() + (pathBinding_.assetId.empty() ? 0 : 1);
  std::vector<std::string> ownedAssets;
  ownedAssets.swap(ownedAssets_);
  std::vector<ShowcaseResolvedRenderBinding3D>{}.swap(bindings_);
  ShowcaseResolvedRenderBinding3D emptyPath{};
  std::swap(pathBinding_, emptyPath);
  std::vector<std::string>{}.swap(visibleAssetIds_);
  std::vector<ShowcaseCookedResourceLease3D>{}.swap(resourceLeases_);
  iblHandle_ = 0;
  if (profile_)
    for (auto it = ownedAssets.rbegin(); it != ownedAssets.rend(); ++it) {
      Aevrynd::PhotorealAssetResidencyReceipt3D unloaded{};
      if (profile_->Unload(*it, unloaded))
        ++receipt.assetsUnloaded;
    }
  std::vector<std::string>{}.swap(ownedAssets);
  if (profile_)
    receipt.retainedBytesAfter = profile_->Stats().residentCpuBytes +
                                 profile_->Stats().residentGpuBytes;
  profile_ = nullptr;
  active_ = false;
  return receipt;
}

bool ShowcaseRenderAssetBindings3D::LoadProduction(
    Aevrynd::Engine &engine, Aevrynd::SceneECSRegistry3D &registry,
    const std::filesystem::path &manifestPath,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    ShowcaseProductionRenderReceipt3D &receipt) {
  receipt = {};
  if (production_ || descriptors.empty())
    return false;
  auto state = std::make_unique<ProductionState>();
  state->baseline = engine.GetRendererStats();
  engine.SetMeshMemoryBudget(512ull * 1024ull * 1024ull);
  engine.SetMaterialTextureMemoryBudget(512ull * 1024ull * 1024ull);
  Aevrynd::CookedRenderManifestError3D error{};
  if (!state->manifest.LoadFile(manifestPath, &error) ||
      state->manifest.Stats().files != 54 ||
      state->manifest.Materials().size() != 11 ||
      state->manifest.MeshParts().size() != 6 ||
      state->manifest.Stats().manifestSha256.size() != 64 ||
      state->manifest.Stats().validatedFiles !=
          state->manifest.Stats().files ||
      state->manifest.Stats().validatedMaterials !=
          state->manifest.Materials().size() ||
      state->manifest.Stats().validatedMeshParts !=
          state->manifest.MeshParts().size() ||
      state->manifest.Stats().validatedReferencedFiles !=
          state->manifest.Stats().referencedFiles)
    return false;

  constexpr std::uint64_t DdsHeaderBytes = 148;
  std::vector<Aevrynd::MaterialAssetDefinition3D> materialDefinitions;
  materialDefinitions.reserve(state->manifest.Materials().size());
  for (const auto &material : state->manifest.Materials()) {
    if (material.baseColorBytes <= DdsHeaderBytes ||
        material.surfaceBytes <= DdsHeaderBytes ||
        material.normalBytes <= DdsHeaderBytes)
      return false;
    state->expectedCompressedBytes += material.baseColorBytes +
        material.surfaceBytes + material.normalBytes - DdsHeaderBytes * 3;
    materialDefinitions.push_back(material.Definition());
  }
  if (!state->materials.Initialize(engine, state->manifest.AssetRoot()) ||
      !state->materials.LoadAll(materialDefinitions))
    return false;

  if (!state->meshes.Initialize(engine, registry,
                                state->manifest.AssetRoot())) {
    state->materials.Shutdown();
    return false;
  }
  std::vector<Aevrynd::StaticMeshAssetDefinition3D> meshDefinitions;
  meshDefinitions.reserve(state->manifest.MeshParts().size());
  for (const auto &part : state->manifest.MeshParts()) {
    const auto *material = state->manifest.FindMaterial(part.materialId);
    if (!material)
      return false;
    Aevrynd::StaticMeshAssetDefinition3D definition{};
    definition.name = part.id;
    definition.meshPath = part.lodPaths[0];
    definition.material = material->Definition();
    definition.generateLods = false;
    definition.importedLodCount = 2;
    definition.importedLods[0].meshPath = part.lodPaths[1];
    definition.importedLods[1].meshPath = part.lodPaths[2];
    definition.lodSwitchDistances = {18.f, 48.f, 96.f};
    definition.lodTransitionWidths = {3.f, 6.f, 10.f};
    definition.cpuRetention = Aevrynd::MeshLodCpuRetention3D::None;
    meshDefinitions.push_back(std::move(definition));
  }
  if (!state->meshes.LoadAll(meshDefinitions))
    return false;
  state->leases.reserve(meshDefinitions.size());
  for (std::size_t index = 0; index < meshDefinitions.size(); ++index) {
    auto lease = state->meshes.ResolveRenderable(meshDefinitions[index].name);
    if (!lease || lease->Renderable().lodLevelCount != 3)
      return false;
    state->assetLeases[state->manifest.MeshParts()[index].assetId]
        .push_back(state->leases.size());
    state->leases.push_back(std::move(*lease));
    const auto &renderable=state->leases.back().Renderable();
    std::array<Aevrynd::MeshBounds3D, 3> lodBounds{};
    if (renderable.lodLevelCount != lodBounds.size())
      return false;
    for (std::size_t level = 0; level < lodBounds.size(); ++level)
      lodBounds[level] = renderable.lodLevels[level].mesh.bounds;
    if (!ValidateShowcaseAssetLodBounds3D(lodBounds))
      return false;
    const auto &bounds=renderable.lodLevelCount
                           ? renderable.lodLevels[0].mesh.bounds
                           : renderable.nearMesh.bounds;
    auto &combined=state->assetBounds[state->manifest.MeshParts()[index].assetId];
    const Aevrynd::Vec3 minimum=bounds.center-bounds.extents,maximum=bounds.center+bounds.extents;
    if(!combined.valid){combined.bounds.minimum=minimum;combined.bounds.maximum=maximum;combined.valid=true;}
    else{combined.bounds.minimum.x=(std::min)(combined.bounds.minimum.x,minimum.x);combined.bounds.minimum.y=(std::min)(combined.bounds.minimum.y,minimum.y);combined.bounds.minimum.z=(std::min)(combined.bounds.minimum.z,minimum.z);combined.bounds.maximum.x=(std::max)(combined.bounds.maximum.x,maximum.x);combined.bounds.maximum.y=(std::max)(combined.bounds.maximum.y,maximum.y);combined.bounds.maximum.z=(std::max)(combined.bounds.maximum.z,maximum.z);}
  }

  const auto &ibl = state->manifest.Ibl();
  state->environmentHandle = state->environment.Create(
      {state->manifest.AssetRoot() / ibl.specularPath,
       state->manifest.AssetRoot() / ibl.irradiancePath,
       state->manifest.AssetRoot() / ibl.brdfLutPath, 0.f});
  if (!state->environmentHandle ||
      !engine.CreateEnvironmentLighting3D(state->environment,
                                           state->environmentHandle,
                                           &state->createReceipt) ||
      !engine.SetEnvironmentLighting3D(state->environment,
                                        state->environmentHandle,
                                        &state->setReceipt))
    return false;
  const auto upload = engine.SubmitPendingUploads3D();
  if (!upload)
    return false;
  engine.WaitRendererIdle3D();
  const auto resident = engine.GetRendererStats();
  if (!engine.IsUploadComplete3D(upload) ||
      resident.residentMeshes != state->baseline.residentMeshes + 18 ||
      resident.residentMeshReferences !=
          state->baseline.residentMeshReferences + 18 ||
      resident.residentMaterialTextureSets !=
          state->baseline.residentMaterialTextureSets + 11 ||
      resident.residentMaterialTextureReferences !=
          state->baseline.residentMaterialTextureReferences + 17 ||
      resident.residentEnvironmentLightingSets !=
          state->baseline.residentEnvironmentLightingSets + 1 ||
      resident.totalMaterialTextureDecodedBytes !=
          state->baseline.totalMaterialTextureDecodedBytes ||
      resident.materialDerivedDataCacheHits !=
          state->baseline.materialDerivedDataCacheHits ||
      resident.materialDerivedDataCacheMisses !=
          state->baseline.materialDerivedDataCacheMisses ||
      resident.materialDerivedDataCacheWrites !=
          state->baseline.materialDerivedDataCacheWrites ||
      resident.totalMaterialTextureCompressedBytes !=
          state->baseline.totalMaterialTextureCompressedBytes +
              state->expectedCompressedBytes ||
      !resident.environmentLightingActive)
    return false;

  receipt.descriptorFingerprint = DescriptorFingerprint(descriptors);
  if (!AssignShowcaseProductionManifestEvidence3D(state->manifest.Stats(),
                                                  receipt))
    return false;
  receipt.profileAssets = 10;
  receipt.meshLodLevels = receipt.meshParts * 3;
  receipt.iblFiles = 3;
  receipt.uploadComplete = true;
  receipt.collisionStableIdsUnchanged =
      receipt.descriptorFingerprint == DescriptorFingerprint(descriptors);
  receipt.photorealActive = receipt.collisionStableIdsUnchanged;
  state->active = receipt.photorealActive;
  production_ = std::move(state);
  return receipt.photorealActive;
}

const Aevrynd::Material3D *ShowcaseRenderAssetBindings3D::ProductionMaterial(
    std::string_view assetId) const noexcept {
  if (!production_ || !production_->active)
    return nullptr;
  for (const auto &material : production_->manifest.Materials())
    if (material.assetId == assetId)
      return production_->materials.Find(material.id);
  return nullptr;
}

bool ShowcaseRenderAssetBindings3D::SubmitProductionMesh(
    Aevrynd::Scene3D &scene, std::string_view assetId,
    const Aevrynd::Transform3D &transform,
    Aevrynd::RenderObjectIdentity3D identity,
    std::uint32_t firstLodObjectIndex,
    std::vector<ShowcaseProductionSubmittedLodChain3D> *submitted) const {
  if (!production_ || !production_->active)
    return false;
  const auto found = production_->assetLeases.find(std::string(assetId));
  if (found == production_->assetLeases.end() || found->second.empty())
    return false;
  Aevrynd::Transform3D normalized{};
  const auto boundsFound=production_->assetBounds.find(std::string(assetId));
  if(boundsFound==production_->assetBounds.end()||!boundsFound->second.valid)
    return false;
  if (!NormalizeShowcaseAssetTransform3D(boundsFound->second.bounds, transform,
                                         normalized)) {
    ++production_->submissionStats.normalizationRejected;
    return false;
  }
  ++production_->submissionStats.normalizedAssets;
  const auto before = scene.MeshCount();
  std::vector<ShowcaseProductionSubmittedLodChain3D> pending;
  pending.reserve(found->second.size());
  std::uint32_t submesh{};
  for (const auto index : found->second) {
    if (index >= production_->leases.size() || !production_->leases[index])
      return false;
    const auto &renderable = production_->leases[index].Renderable();
    Aevrynd::Vec3 minimum{}, maximum{};
    bool hasBounds{};
    for (std::uint32_t level = 0; level < renderable.lodLevelCount; ++level) {
      const auto &bounds = renderable.lodLevels[level].mesh.bounds;
      const auto levelMinimum = bounds.center - bounds.extents;
      const auto levelMaximum = bounds.center + bounds.extents;
      if (!hasBounds) {
        minimum = levelMinimum;
        maximum = levelMaximum;
        hasBounds = true;
      } else {
        minimum.x = (std::min)(minimum.x, levelMinimum.x);
        minimum.y = (std::min)(minimum.y, levelMinimum.y);
        minimum.z = (std::min)(minimum.z, levelMinimum.z);
        maximum.x = (std::max)(maximum.x, levelMaximum.x);
        maximum.y = (std::max)(maximum.y, levelMaximum.y);
        maximum.z = (std::max)(maximum.z, levelMaximum.z);
      }
    }
    const std::uint32_t lodObjectIndex = firstLodObjectIndex + submesh;
    ShowcaseProductionSubmittedLodChain3D chain{};
    chain.identity = identity;
    chain.submesh = submesh;
    chain.lodObjectIndex = lodObjectIndex;
    chain.lodSeedBase = (lodObjectIndex * 5u + 3u) & 7u;
    chain.normalizedTransform = normalized;
    chain.localUnionBounds.center = (minimum + maximum) * .5f;
    chain.localUnionBounds.extents = (maximum - minimum) * .5f;
    chain.localUnionBounds.radius = Aevrynd::Length(chain.localUnionBounds.extents);
    chain.levelCount = renderable.lodLevelCount;
    for (std::uint32_t level = 0; level + 1 < renderable.lodLevelCount; ++level) {
      chain.switchDistances[level] = renderable.lodLevels[level].switchDistance;
      chain.transitionWidths[level] = renderable.lodLevels[level].transitionWidth;
    }
    if (!scene.MeshLodChain(
            std::span<const Aevrynd::MeshLodLevel3D>{
                renderable.lodLevels.data(), renderable.lodLevelCount},
            renderable.lodDistanceMetric, normalized,
            renderable.assetLease, identity)) {
      ++production_->submissionStats.meshChainsRejected;
      return false;
    }
    pending.push_back(chain);
    ++production_->submissionStats.meshChainsSubmitted;
    ++submesh;
  }
  const bool success = scene.MeshCount() == before + found->second.size();
  if (success && submitted)
    submitted->insert(submitted->end(), pending.begin(), pending.end());
  return success;
}

ShowcaseProductionSubmissionStats3D
ShowcaseRenderAssetBindings3D::ProductionSubmissionStats() const noexcept {
  return production_ ? production_->submissionStats
                     : ShowcaseProductionSubmissionStats3D{};
}

void ShowcaseRenderAssetBindings3D::ResetProductionSubmissionStats() noexcept {
  if (production_)
    production_->submissionStats = {};
}

bool NormalizeShowcaseAssetTransform3D(
    const ShowcaseAssetBounds3D &sourceBounds,
    const Aevrynd::Transform3D &targetBounds,
    Aevrynd::Transform3D &normalized) noexcept {
  const auto finite = [](const Aevrynd::Vec3 &value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
  };
  if (!finite(sourceBounds.minimum) || !finite(sourceBounds.maximum) ||
      !finite(targetBounds.position) || !finite(targetBounds.rotation) ||
      !finite(targetBounds.scale))
    return false;
  const Aevrynd::Vec3 sourceSize =
      sourceBounds.maximum - sourceBounds.minimum;
  constexpr float minimumExtent = 0.0001f;
  if (sourceSize.x <= minimumExtent || sourceSize.y <= minimumExtent ||
      sourceSize.z <= minimumExtent || targetBounds.scale.x <= 0.0f ||
      targetBounds.scale.y <= 0.0f || targetBounds.scale.z <= 0.0f)
    return false;
  const float uniform = (std::min)(
      {targetBounds.scale.x / sourceSize.x,
       targetBounds.scale.y / sourceSize.y,
       targetBounds.scale.z / sourceSize.z});
  if (!std::isfinite(uniform) || uniform <= 0.0f)
    return false;

  normalized = targetBounds;
  normalized.scale = {uniform, uniform, uniform};
  const Aevrynd::Vec3 center =
      (sourceBounds.minimum + sourceBounds.maximum) * 0.5f;
  const float localX = center.x * uniform;
  const float localZ = center.z * uniform;
  const float sine = std::sin(targetBounds.rotation.y);
  const float cosine = std::cos(targetBounds.rotation.y);
  normalized.position.x -= cosine * localX + sine * localZ;
  normalized.position.z -= -sine * localX + cosine * localZ;
  normalized.position.y -= sourceBounds.minimum.y * uniform;
  return finite(normalized.position) && finite(normalized.scale);
}

bool ValidateShowcaseAssetLodBounds3D(
    std::span<const Aevrynd::MeshBounds3D> lodBounds) noexcept {
  if (lodBounds.size() != 3)
    return false;
  const auto finite = [](const Aevrynd::Vec3 &value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
  };
  constexpr float minimumExtent = 0.0001f;
  for (const auto &bounds : lodBounds)
    if (!finite(bounds.center) || !finite(bounds.extents) ||
        !std::isfinite(bounds.radius) || bounds.radius <= minimumExtent ||
        bounds.extents.x <= minimumExtent ||
        bounds.extents.y <= minimumExtent ||
        bounds.extents.z <= minimumExtent)
      return false;
  const auto &source = lodBounds.front();
  for (std::size_t level = 1; level < lodBounds.size(); ++level) {
    const auto &candidate = lodBounds[level];
    if (Aevrynd::Length(candidate.center - source.center) >
        (std::max)(0.001f, source.radius * 0.08f))
      return false;
    for (const auto ratio : {candidate.extents.x / source.extents.x,
                             candidate.extents.y / source.extents.y,
                             candidate.extents.z / source.extents.z})
      if (!std::isfinite(ratio) || ratio < 0.70f || ratio > 1.15f)
        return false;
  }
  return true;
}

bool ShowcaseRenderAssetBindings3D::ProductionActive() const noexcept {
  return production_ && production_->active;
}

ShowcaseProductionReleaseReceipt3D
ShowcaseRenderAssetBindings3D::ReleaseProduction(Aevrynd::Engine &engine,
                                                  Aevrynd::Scene3D &scene) {
  ShowcaseProductionReleaseReceipt3D receipt{};
  if (!production_)
    return receipt;
  auto &state = *production_;
  engine.Draw().Clear();
  scene.Clear();
  for (auto iterator = state.leases.rbegin(); iterator != state.leases.rend();
       ++iterator) {
    iterator->Reset();
    ++receipt.leasesReleased;
  }
  state.leases.clear();
  for (auto iterator = state.manifest.MeshParts().rbegin();
       iterator != state.manifest.MeshParts().rend(); ++iterator)
    if (state.meshes.Unload(iterator->id))
      ++receipt.meshesReleased;
  state.meshes.Shutdown();
  for (auto iterator = state.manifest.Materials().rbegin();
       iterator != state.manifest.Materials().rend(); ++iterator)
    if (state.materials.Unload(iterator->id))
      ++receipt.materialsReleased;
  state.materials.Shutdown();
  engine.ClearEnvironmentLighting3D();
  engine.DestroyEnvironmentLighting3D(state.environment,
                                       state.environmentHandle);
  state.environment.Destroy(state.environmentHandle);
  state.environmentHandle = {};
  state.environment.Trim();
  state.environment.Shutdown();
  state.assetLeases.clear();
  state.active = false;
  state.manifest.Clear();
  engine.WaitRendererIdle3D();
  engine.TrimEnvironmentLighting3D();
  engine.TrimUnusedMemory();
  engine.WaitRendererIdle3D();
  const auto released = engine.GetRendererStats();
  receipt.baselineMeshes=state.baseline.residentMeshes;receipt.releasedMeshes=released.residentMeshes;
  receipt.baselineMeshReferences=state.baseline.residentMeshReferences;receipt.releasedMeshReferences=released.residentMeshReferences;
  receipt.baselineMaterials=state.baseline.residentMaterialTextureSets;receipt.releasedMaterials=released.residentMaterialTextureSets;
  receipt.baselineMaterialReferences=state.baseline.residentMaterialTextureReferences;receipt.releasedMaterialReferences=released.residentMaterialTextureReferences;
  receipt.baselineIbl=state.baseline.residentEnvironmentLightingSets;receipt.releasedIbl=released.residentEnvironmentLightingSets;
  receipt.pendingGpuBytes=released.pendingGpuReleaseBytes;receipt.pendingIblBytes=released.pendingEnvironmentLightingReleaseBytes;
  receipt.rendererBaselineRestored =
      released.residentMeshes == state.baseline.residentMeshes &&
      released.residentMeshReferences ==
          state.baseline.residentMeshReferences &&
      released.residentMaterialTextureSets ==
          state.baseline.residentMaterialTextureSets &&
      released.residentMaterialTextureReferences ==
          state.baseline.residentMaterialTextureReferences &&
      released.residentEnvironmentLightingSets ==
          state.baseline.residentEnvironmentLightingSets &&
      released.pendingGpuReleaseBytes == 0 &&
      released.pendingEnvironmentLightingReleaseBytes == 0 &&
      !released.environmentLightingActive;
  production_.reset();
  return receipt;
}

} // namespace GameplayLab
