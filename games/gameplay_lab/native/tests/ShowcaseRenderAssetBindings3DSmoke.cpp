#include "GameplayShowcaseEnvironment3D.h"
#include "ShowcaseRenderAssetBindings3D.h"

#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/PhotorealAssetProfile3D.h>

#include <Windows.h>
#include <winioctl.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

using namespace GameplayLab;

namespace {
bool VerifyManifestEvidenceReceipt() {
  Aevrynd::CookedRenderManifestStats3D stats{};
  stats.files = stats.referencedFiles = stats.validatedFiles =
      stats.validatedReferencedFiles = 9;
  stats.materials = stats.validatedMaterials = 1;
  stats.meshParts = stats.validatedMeshParts = 1;
  stats.aggregateBytes = 27;
  stats.fingerprint = 42;
  stats.manifestSha256 =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  ShowcaseProductionRenderReceipt3D receipt{};
  if (!AssignShowcaseProductionManifestEvidence3D(stats, receipt) ||
      receipt.manifestSha256 != stats.manifestSha256 ||
      receipt.manifestFingerprint != stats.fingerprint ||
      receipt.validatedPayloadFiles != 9 ||
      receipt.validatedMaterialSets != 1 ||
      receipt.validatedMeshParts != 1 ||
      receipt.validatedReferencedFiles != 9)
    return false;
  stats.manifestSha256[0] = 'A';
  if (AssignShowcaseProductionManifestEvidence3D(stats, receipt) ||
      !receipt.manifestSha256.empty() || receipt.manifestFingerprint ||
      receipt.validatedPayloadFiles)
    return false;
  stats.manifestSha256[0] = '0';
  --stats.validatedFiles;
  return !AssignShowcaseProductionManifestEvidence3D(stats, receipt) &&
         receipt.manifestSha256.empty() && !receipt.validatedPayloadFiles;
}

bool VerifyUnionBoundsNormalization() {
  constexpr float epsilon = 0.0001f;
  const auto close = [](float left, float right) {
    return std::abs(left - right) <= epsilon;
  };
  const ShowcaseAssetBounds3D unionBounds{{-1.0f, -2.0f, -3.0f},
                                           {3.0f, 2.0f, 3.0f}};
  Aevrynd::Transform3D target{};
  target.position = {10.0f, 2.0f, 20.0f};
  target.scale = {4.0f, 8.0f, 6.0f};
  Aevrynd::Transform3D first{}, second{};
  if (!NormalizeShowcaseAssetTransform3D(unionBounds, target, first) ||
      !NormalizeShowcaseAssetTransform3D(unionBounds, target, second) ||
      !close(first.scale.x, 1.0f) || !close(first.scale.y, first.scale.x) ||
      !close(first.scale.z, first.scale.x) ||
      !close(first.position.x, 9.0f) || !close(first.position.y, 4.0f) ||
      !close(first.position.z, 20.0f) ||
      !close(first.position.y + unionBounds.minimum.y * first.scale.y,
             target.position.y) ||
      !close(first.position.x, second.position.x) ||
      !close(first.position.y, second.position.y) ||
      !close(first.position.z, second.position.z))
    return false;

  auto invalid = unionBounds;
  invalid.maximum.x = invalid.minimum.x;
  if (NormalizeShowcaseAssetTransform3D(invalid, target, first))
    return false;
  invalid = unionBounds;
  invalid.maximum.y = std::numeric_limits<float>::infinity();
  if (NormalizeShowcaseAssetTransform3D(invalid, target, first))
    return false;
  auto invalidTarget = target;
  invalidTarget.scale.z = 0.0f;
  return !NormalizeShowcaseAssetTransform3D(unionBounds, invalidTarget, first);
}

std::uint64_t MeshIdentity(const Aevrynd::Mesh3D &mesh) {
  std::uint64_t hash = 1469598103934665603ull;
  const auto add = [&hash](const void *data, std::size_t bytes) {
    const auto *values = static_cast<const std::uint8_t *>(data);
    for (std::size_t index{}; index < bytes; ++index) {
      hash ^= values[index];
      hash *= 1099511628211ull;
    }
  };
  add(mesh.vertices.data(), mesh.vertices.size() * sizeof(Aevrynd::Vertex3D));
  add(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
  return hash ? hash : 1;
}

std::wstring AggregateCacheFileName(
    const std::array<std::uint8_t, 32> &digest) {
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring name = L"proxy-";
  for (const auto byte : digest) {
    name.push_back(hex[byte >> 4]);
    name.push_back(hex[byte & 15]);
  }
  return name + L".aemesh";
}

bool CreateJunction(const std::filesystem::path &link,
                    const std::filesystem::path &target) {
  struct MountPointBuffer {
    DWORD tag{};
    WORD dataLength{}, reserved{};
    WORD substituteOffset{}, substituteLength{}, printOffset{}, printLength{};
    std::array<wchar_t, 1024> paths{};
  } buffer{};
  std::error_code error;
  if (!std::filesystem::create_directory(link, error) || error)
    return false;
  const auto absolute = std::filesystem::absolute(target, error).wstring();
  const std::wstring substitute = L"\\??\\" + absolute;
  if (error || substitute.size() + absolute.size() + 2 > buffer.paths.size())
    return false;
  buffer.tag = IO_REPARSE_TAG_MOUNT_POINT;
  buffer.substituteLength = WORD(substitute.size() * sizeof(wchar_t));
  buffer.printOffset = WORD(buffer.substituteLength + sizeof(wchar_t));
  buffer.printLength = WORD(absolute.size() * sizeof(wchar_t));
  buffer.dataLength = WORD(8 + buffer.printOffset + buffer.printLength +
                           sizeof(wchar_t));
  std::memcpy(buffer.paths.data(), substitute.data(), buffer.substituteLength);
  std::memcpy(reinterpret_cast<std::byte *>(buffer.paths.data()) +
                  buffer.printOffset,
              absolute.data(), buffer.printLength);
  const HANDLE handle = CreateFileW(
      link.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE |
          FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return false;
  DWORD returned{};
  const bool success = DeviceIoControl(
      handle, FSCTL_SET_REPARSE_POINT, &buffer, 8 + buffer.dataLength, nullptr,
      0, &returned, nullptr) != FALSE;
  CloseHandle(handle);
  return success;
}

bool VerifyAggregateCacheHostility(const Aevrynd::Mesh3D &mesh,
                                   const ShowcaseAggregateProxyReceipt3D &receipt) {
  std::error_code error;
  const auto base = std::filesystem::temp_directory_path() /
      (L"aevrynd-hlod-cache-hostile-" + std::to_wstring(GetCurrentProcessId()) +
       L"-" + std::to_wstring(GetTickCount64()));
  const auto normal = base / L"normal";
  const auto junctionRoot = base / L"junction-root";
  const auto junctionTarget = base / L"junction-target";
  const auto hardlinkRoot = base / L"hardlink-root";
  const auto external = base / L"external.aemesh";
  std::filesystem::create_directories(normal, error);
  if (error || !ExerciseShowcaseAggregateProxyCache3DForTesting(
                   normal, mesh, receipt)) {
    std::cerr << "Aggregate cache normal transaction failed: " << error.message() << '\n';
    std::filesystem::remove_all(base, error);
    return false;
  }
  const auto normalCache = normal / L".aevrynd-runtime-cache" /
                           L"hlod-aggregate-proxy";
  std::size_t normalEntries{};
  for (std::filesystem::directory_iterator iterator(normalCache, error), end;
       !error && iterator != end; ++iterator) {
    ++normalEntries;
    if (!iterator->is_regular_file(error) ||
        iterator->path().extension() != L".aemesh")
      error = std::make_error_code(std::errc::invalid_argument);
  }
  if (normalEntries != 1 || error) {
    std::cerr << "Aggregate cache unique-temp cleanup failed: "
              << error.message() << '\n';
    std::filesystem::remove_all(base, error);
    return false;
  }

  std::filesystem::create_directories(junctionRoot, error);
  std::filesystem::create_directories(junctionTarget, error);
  if (error || !CreateJunction(junctionRoot / L".aevrynd-runtime-cache",
                               junctionTarget) ||
      ExerciseShowcaseAggregateProxyCache3DForTesting(junctionRoot, mesh,
                                                       receipt)) {
    std::cerr << "Aggregate cache junction rejection failed: " << GetLastError() << '\n';
    std::filesystem::remove_all(base, error);
    return false;
  }

  const auto hardlinkCache = hardlinkRoot / L".aevrynd-runtime-cache" /
                             L"hlod-aggregate-proxy";
  std::filesystem::create_directories(hardlinkCache, error);
  {
    std::ofstream output(external, std::ios::binary);
    output << "hostile external payload";
  }
  const auto cacheFile = hardlinkCache /
      AggregateCacheFileName(receipt.aggregateContentDigest);
  if (error || !CreateHardLinkW(cacheFile.c_str(), external.c_str(), nullptr) ||
      ExerciseShowcaseAggregateProxyCache3DForTesting(hardlinkRoot, mesh,
                                                       receipt) ||
      !std::filesystem::exists(external)) {
    std::cerr << "Aggregate cache hard-link rejection failed: " << GetLastError() << '\n';
    std::filesystem::remove_all(base, error);
    return false;
  }
  std::filesystem::remove_all(base, error);
  return !error;
}

bool VerifyHlodDescriptorAndAggregatePolicy() {
  constexpr std::array kinds{
      ShowcaseSceneKind3D::Terrain, ShowcaseSceneKind3D::Water,
      ShowcaseSceneKind3D::Tree, ShowcaseSceneKind3D::Grass,
      ShowcaseSceneKind3D::Rock, ShowcaseSceneKind3D::Building};
  constexpr std::array policies{
      ShowcaseHlodDescriptorPolicy3D::DedicatedTerrain,
      ShowcaseHlodDescriptorPolicy3D::DedicatedWater,
      ShowcaseHlodDescriptorPolicy3D::DedicatedVegetation,
      ShowcaseHlodDescriptorPolicy3D::DedicatedVegetation,
      ShowcaseHlodDescriptorPolicy3D::AggregateOpaque,
      ShowcaseHlodDescriptorPolicy3D::DedicatedBuilding};
  for (std::size_t index{}; index < kinds.size(); ++index)
    if (ResolveShowcaseHlodDescriptorPolicy3D(kinds[index]) != policies[index])
      return false;
  if (ResolveShowcaseHlodDescriptorPolicy3D(
          static_cast<ShowcaseSceneKind3D>(0xff)) !=
      ShowcaseHlodDescriptorPolicy3D::FailClosed)
    return false;

  const auto sourceMesh = Aevrynd::Mesh3D::CreateUnitBox();
  const auto meshIdentity = MeshIdentity(sourceMesh);
  std::array<ShowcaseAggregateProxySource3D, 2> sources{{
      {&sourceMesh, {{-2, 0, 0}, {}, {1, 1, 1}}, {7, 1}, 3,
       meshIdentity, 41, 51},
      {&sourceMesh, {{2, 0, 0}, {}, {1, 1, 1}}, {7, 2}, 3,
       meshIdentity, 41, 51}}};
  Aevrynd::Mesh3D aggregate, rebuilt;
  ShowcaseAggregateProxyReceipt3D receipt{}, rebuiltReceipt{};
  if (!BuildShowcaseAggregateProxyMesh3D(sources, aggregate, receipt) ||
      !receipt.Complete() || receipt.sourceInstances != 2 ||
      receipt.sourceVertices != sourceMesh.vertices.size() * 2 ||
      receipt.sourceTriangles != sourceMesh.indices.size() * 2 / 3 ||
      receipt.aggregateTriangles >= receipt.sourceTriangles ||
      !ValidateShowcaseAggregateProxyMesh3D(aggregate, receipt))
    return false;
  std::array reversed{sources[1], sources[0]};
  if (!BuildShowcaseAggregateProxyMesh3D(reversed, rebuilt, rebuiltReceipt) ||
      rebuiltReceipt.aggregateContentIdentity != receipt.aggregateContentIdentity ||
      rebuiltReceipt.sourceContractIdentity != receipt.sourceContractIdentity ||
      rebuiltReceipt.aggregateContentDigest != receipt.aggregateContentDigest ||
      !VerifyAggregateCacheHostility(aggregate, receipt))
    return false;

  auto tamperedMesh = aggregate;
  tamperedMesh.vertices.front().x += 1.f;
  if (ValidateShowcaseAggregateProxyMesh3D(tamperedMesh, receipt))
    return false;
  auto forgedReceipt = receipt;
  forgedReceipt.aggregateContentDigest.front() ^= 1;
  if (ValidateShowcaseAggregateProxyMesh3D(aggregate, forgedReceipt))
    return false;
  forgedReceipt = receipt;
  forgedReceipt.aggregateVertices = (std::numeric_limits<std::size_t>::max)();
  if (ValidateShowcaseAggregateProxyMesh3D(aggregate, forgedReceipt))
    return false;
  forgedReceipt = receipt;
  forgedReceipt.immutableIdentityValidated = false;
  if (ValidateShowcaseAggregateProxyMesh3D(aggregate, forgedReceipt))
    return false;
  auto invalid = sources;
  invalid[1].meshContentIdentity ^= 1;
  if (BuildShowcaseAggregateProxyMesh3D(invalid, rebuilt, rebuiltReceipt))
    return false;
  invalid = sources;
  invalid[1].mesh = nullptr;
  if (BuildShowcaseAggregateProxyMesh3D(invalid, rebuilt, rebuiltReceipt))
    return false;
  invalid = sources;
  ++invalid[1].materialIdentity;
  if (BuildShowcaseAggregateProxyMesh3D(invalid, rebuilt, rebuiltReceipt))
    return false;
  invalid = sources;
  invalid[1].objectId = invalid[0].objectId;
  if (BuildShowcaseAggregateProxyMesh3D(invalid, rebuilt, rebuiltReceipt))
    return false;
  invalid = sources;
  invalid[1].transform.scale.z = 0.f;
  return !BuildShowcaseAggregateProxyMesh3D(invalid, rebuilt, rebuiltReceipt);
}

class ControlledResolver final : public ShowcaseCookedResourceResolver3D {
public:
  explicit ControlledResolver(
      bool valid,
      std::size_t maximumAcquires =
          (std::numeric_limits<std::size_t>::max)())
      : valid_(valid), maximumAcquires_(maximumAcquires) {}
  bool VerifyReceipt(std::string_view, std::uint32_t schemaVersion,
                     std::span<const ShowcaseCookedDocumentReceipt3D>
                         documents) override {
    return valid_ && schemaVersion == 1 && !documents.empty() &&
           std::ranges::all_of(documents, [](const auto &document) {
             return !document.relativePath.empty() &&
                    document.sha256.size() == 64;
           });
  }
  bool Acquire(const ShowcaseCookedResourceRequest3D &request,
               ShowcaseCookedResourceLease3D &lease) override {
    if (acquires_++ >= maximumAcquires_)
      return false;
    lease = {request.kind,
             std::string(request.assetId),
             std::string(request.sourceSha256),
             std::string(request.cookedSha256),
             request.claimedHandle,
             valid_ ? 0x434f4f4b4544ull : 0ull,
             valid_ ? 7u : 0u,
             1u,
             2u,
             valid_ ? std::static_pointer_cast<const void>(lifetime_) : nullptr,
             valid_,
             valid_};
    return true;
  }
  [[nodiscard]] long RetainedLeaseCount() const {
    return lifetime_.use_count() - 1;
  }

private:
  bool valid_{};
  std::size_t maximumAcquires_{};
  std::size_t acquires_{};
  std::shared_ptr<int> lifetime_{std::make_shared<int>(1)};
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 1;
  if (!VerifyManifestEvidenceReceipt())
    return 13;
  if (!VerifyUnionBoundsNormalization())
    return 12;
  if (!VerifyHlodDescriptorAndAggregatePolicy())
    return 14;
  GameplayShowcaseEnvironment3D world;
  if (!world.Initialize())
    return 2;
  const auto before = world.Receipt();
  const auto descriptorSnapshot = world.Descriptors();

  Aevrynd::PhotorealAssetCatalog3D catalog;
  Aevrynd::PhotorealRuntimeManifest3D runtime;
  if (!catalog.LoadFile(argv[1]) || !runtime.LoadFile(argv[2]))
    return 3;
  Aevrynd::PhotorealAssetProfile3D profile;
  Aevrynd::PhotorealAssetProfileSettings3D settings{};
  settings.cpuBudgetBytes = 512ull * 1024ull * 1024ull;
  settings.gpuBudgetBytes = 512ull * 1024ull * 1024ull;
  if (!profile.Initialize(catalog, runtime, settings))
    return 4;

  constexpr std::array ids = {
      "forest_ground_04", "Ground108",          "coastal_cliff_01",
      "boulder_01",       "island_tree_01",     "dead_tree_trunk_02",
      "tree_small_02",    "RoofingTiles014B",   "Bricks100",
      "stony_dirt_path",  "xanderklinge"};
  std::vector<ShowcaseCookedAssetReceipt3D> cooked;
  std::uint64_t handle = 100;
  for (const auto *id : ids) {
    const auto *runtimeAsset = runtime.Find(id);
    if (!runtimeAsset || runtimeAsset->acceptedSha256.empty())
      return 5;
    ShowcaseCookedAssetReceipt3D item{};
    item.schemaVersion = 1;
    item.assetId = id;
    item.sha256 = runtimeAsset->acceptedSha256.front();
    item.documents.push_back(
        {std::string("receipts/") + id + ".json", std::string(64, 'a')});
    item.cooked = true;
    item.materialSidecarBound = true;
    const auto *asset = catalog.Find(id);
    if (asset->kind == Aevrynd::PhotorealAssetKind3D::MeshPbr) {
      const std::size_t parts = std::string_view(id) == "island_tree_01" ||
                                        std::string_view(id) == "tree_small_02"
                                    ? 3u
                                    : 1u;
      for (std::size_t part = 0; part < parts; ++part)
        item.materials.push_back(
            {handle++, static_cast<std::uint32_t>(part),
             runtimeAsset->acceptedSha256.back(), std::string(64, 'b')});
      for (std::uint32_t lod = 0; lod < 3; ++lod)
        for (std::uint32_t part = 0; part < parts; ++part)
          item.meshParts.push_back({handle++,
                                    item.materials[part].materialHandle, part,
                                    lod, runtimeAsset->acceptedSha256[lod],
                                    std::string(64, 'c')});
      for (std::uint32_t lod = 0; lod < 3; ++lod)
        item.meshLodHandles[lod] =
            item.meshParts[lod * parts].meshHandle;
    } else if (asset->kind == Aevrynd::PhotorealAssetKind3D::Hdri) {
      item.iblHandle = handle++;
      item.iblSourceSha256 = runtimeAsset->acceptedSha256.front();
      item.iblCookedSha256.assign(64, 'd');
    } else {
      item.materials.push_back({handle++, 0,
                                runtimeAsset->acceptedSha256.front(),
                                std::string(64, 'e')});
    }
    const std::uint64_t resourceCount =
        static_cast<std::uint64_t>(item.meshParts.size() +
                                   item.materials.size() +
                                   (item.iblHandle ? 1u : 0u));
    item.cpuBytes = resourceCount;
    item.gpuBytes = resourceCount * 2u;
    cooked.push_back(std::move(item));
  }

  ShowcaseRenderAssetBindings3D bindings;
  ShowcaseRenderAssetLoadReceipt3D load{};
  ControlledResolver invalidResolver(false);
  if (bindings.Load(world.Descriptors(), catalog, runtime, profile,
                    invalidResolver, cooked, load) ||
      bindings.PhotorealActive() || profile.Stats().residentAssets) {
    std::cerr << "Invalid resolver was not rejected atomically\n";
    return 6;
  }
  ControlledResolver interruptedResolver(true, 4);
  if (bindings.Load(world.Descriptors(), catalog, runtime, profile,
                    interruptedResolver, cooked, load) ||
      interruptedResolver.RetainedLeaseCount() ||
      bindings.PhotorealActive() || profile.Stats().residentAssets) {
    std::cerr << "Interrupted resolver was not rolled back atomically\n";
    return 6;
  }
  ControlledResolver resolver(true);
  std::vector<std::string> expectedVisible;
  for (const auto &descriptor : world.Descriptors()) {
    const auto id = ResolveShowcasePhotorealAssetId3D(descriptor);
    if (!id.empty() && std::ranges::find(expectedVisible, id) == expectedVisible.end())
      expectedVisible.emplace_back(id);
  }
  if (std::ranges::find(expectedVisible, "stony_dirt_path") ==
      expectedVisible.end())
    expectedVisible.emplace_back("stony_dirt_path");
  expectedVisible.emplace_back("xanderklinge");
  if (!bindings.Load(world.Descriptors(), catalog, runtime, profile, resolver, cooked,
                     load) ||
      !load.photorealActive || load.descriptorCount != before.integratedDescriptors ||
      !load.collisionStableIdsUnchanged ||
      !load.assetIdentityFingerprint || !load.assetIdentitiesExact ||
      !load.descriptorOrderDeterministic ||
      load.requiredAssetIdentityMask != 0x3ffu ||
      load.terrainLayerDescriptors !=
          std::array<std::size_t, 3>{63, 62, 62} ||
      load.assetIdentityFingerprint !=
          ShowcaseAssetIdentityFingerprint3D(world.Descriptors()) ||
      !std::ranges::equal(bindings.VisibleAssetIds(), expectedVisible) ||
      !bindings.IblHandle() ||
      resolver.RetainedLeaseCount() <= 0) {
    std::cerr << "Valid resolver binding failed: loaded=" << load.loaded
              << " photoreal=" << load.photorealActive
              << " descriptors=" << load.descriptorCount
              << " resolved=" << load.resolvedCount
              << " admitted=" << load.admittedAssets
              << " fallback=" << load.fallbackCount
              << " visible=" << bindings.VisibleAssetIds().size()
              << " ibl=" << bindings.IblHandle()
              << " leases=" << resolver.RetainedLeaseCount() << " ids=";
    for (const auto &id : bindings.VisibleAssetIds())
      std::cerr << id << ',';
    std::cerr << '\n';
    return 6;
  }
  ShowcaseSceneDescriptor3D semantic{};
  semantic.kind = ShowcaseSceneKind3D::Tree;
  semantic.meshAsset = "poly-haven/island_tree_01/cooked-lod";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "island_tree_01") return 6;
  semantic.meshAsset = "poly-haven/tree_small_02/cooked-lod";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "tree_small_02") return 6;
  semantic.meshAsset = "poly-haven/dead_tree_trunk_02/cooked-lod";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "dead_tree_trunk_02") return 6;
  semantic.meshAsset = "mutation/unknown-tree";
  if (!ResolveShowcasePhotorealAssetId3D(semantic).empty()) return 6;
  semantic.kind = ShowcaseSceneKind3D::Rock;
  semantic.meshAsset = "poly-haven/boulder_01/cooked-lod";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "boulder_01") return 6;
  semantic.meshAsset = "poly-haven/coastal_cliff_01/cooked-lod";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "coastal_cliff_01") return 6;
  semantic.meshAsset = "mutation/unknown-rock";
  if (!ResolveShowcasePhotorealAssetId3D(semantic).empty()) return 6;
  semantic.kind = ShowcaseSceneKind3D::Water;
  if (!ResolveShowcasePhotorealAssetId3D(semantic).empty()) return 6;
  semantic.kind = ShowcaseSceneKind3D::Grass;
  semantic.materialAsset = "poly-haven/stony_dirt_path/cooked-pbr";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "stony_dirt_path") return 6;
  semantic.materialAsset = "ambientcg/Ground108/cooked-pbr";
  if (ResolveShowcasePhotorealAssetId3D(semantic) != "Ground108") return 6;
  semantic.materialAsset = "mutation/unknown-ground";
  if (!ResolveShowcasePhotorealAssetId3D(semantic).empty()) return 6;
  auto reorderedDescriptors = descriptorSnapshot;
  const auto differentIdentity = std::ranges::find_if(
      reorderedDescriptors.begin() + 1, reorderedDescriptors.end(),
      [&](const auto &item) {
        return ResolveShowcasePhotorealAssetId3D(item) !=
               ResolveShowcasePhotorealAssetId3D(reorderedDescriptors.front());
      });
  if (differentIdentity == reorderedDescriptors.end()) return 14;
  std::iter_swap(reorderedDescriptors.begin(), differentIdentity);
  if (ShowcaseAssetIdentityFingerprint3D(descriptorSnapshot) ==
      ShowcaseAssetIdentityFingerprint3D(reorderedDescriptors))
    return 14;
  if (world.Receipt().worldHash != before.worldHash ||
      world.Receipt().retainedDescriptorBytes != before.retainedDescriptorBytes)
    return 7;
  for (std::size_t index = 0; index < descriptorSnapshot.size(); ++index) {
    const auto &left = descriptorSnapshot[index];
    const auto &right = world.Descriptors()[index];
    if (left.kind != right.kind || left.position.x != right.position.x ||
        left.position.y != right.position.y || left.position.z != right.position.z ||
        left.scale.x != right.scale.x || left.scale.y != right.scale.y ||
        left.scale.z != right.scale.z || left.yaw != right.yaw ||
        left.lodGroup != right.lodGroup || left.castsShadow != right.castsShadow ||
        left.meshAsset != right.meshAsset ||
        left.materialAsset != right.materialAsset)
      return 7;
  }
  bool sawCliff = false, sawRoof = false,
       sawWall = false, sawTerrain = false;
  for (std::size_t index = 0; index < world.Descriptors().size(); ++index) {
    const auto *binding = bindings.Resolve(index);
    const bool water =
        world.Descriptors()[index].kind == ShowcaseSceneKind3D::Water;
    if (!binding || binding->fallback ||
        (water ? (!binding->renderOnly || binding->photorealActive ||
                  binding->assetId != world.Descriptors()[index].materialAsset)
               : !binding->photorealActive) ||
        binding->lodGroup != world.Descriptors()[index].lodGroup)
      {std::cerr<<"binding mismatch index="<<index<<" kind="<<unsigned(world.Descriptors()[index].kind)<<" mesh="<<world.Descriptors()[index].meshAsset<<" actual="<<(binding?binding->assetId:"<null>")<<" fallback="<<(binding?binding->fallback:1)<<" photoreal="<<(binding?binding->photorealActive:0)<<'\n';return 8;}
    const auto expectedAsset = ResolveShowcasePhotorealAssetId3D(world.Descriptors()[index]);
    if (!water && binding->assetId != expectedAsset)
      {std::cerr<<"binding asset mismatch index="<<index<<" expected="<<expectedAsset<<" actual="<<binding->assetId<<'\n';return 8;}
    sawCliff |= binding->assetId == "coastal_cliff_01";
    sawRoof |= binding->assetId == "RoofingTiles014B";
    sawWall |= binding->assetId == "Bricks100";
    sawTerrain |= binding->kind == ShowcaseSceneKind3D::Terrain &&
                  !binding->materialHandles.empty();
  }
  if (!sawCliff || !sawRoof || !sawWall ||
      !sawTerrain || !bindings.ResolvePath() ||
      !bindings.ResolvePath()->renderOnly)
    return 9;
  const auto trim = bindings.Trim(0, 0);
  if (!trim.unloadedAssetIds.empty())
    return 10;
  const auto released = bindings.Release();
  if (released.bindingsReleased != load.resolvedCount ||
      released.assetsUnloaded != expectedVisible.size() ||
      released.retainedBytesAfter || profile.Stats().residentAssets ||
      resolver.RetainedLeaseCount())
    return 11;

  const auto rejectedWithDescriptors = [&] (
      std::span<const ShowcaseSceneDescriptor3D> descriptors,
      std::vector<ShowcaseCookedAssetReceipt3D> invalid) {
    ShowcaseRenderAssetLoadReceipt3D result{};
    return !bindings.Load(descriptors, catalog, runtime, profile,
                          resolver, invalid, result) &&
           !bindings.PhotorealActive() && !profile.Stats().residentAssets &&
           !bindings.Resolve(0) && world.Receipt().worldHash == before.worldHash &&
           world.Receipt().retainedDescriptorBytes ==
               before.retainedDescriptorBytes;
  };
  const auto rejected = [&](std::vector<ShowcaseCookedAssetReceipt3D> invalid) {
    return rejectedWithDescriptors(world.Descriptors(), std::move(invalid));
  };
  const auto mutate = [&](std::string_view id, const auto &operation) {
    auto invalid = cooked;
    const auto found = std::ranges::find_if(
        invalid, [&](const auto &item) { return item.assetId == id; });
    operation(*found);
    return rejected(std::move(invalid));
  };
  const auto mutateWithDescriptors = [&] (
      std::span<const ShowcaseSceneDescriptor3D> descriptors,
      std::string_view id, const auto &operation) {
    auto invalid = cooked;
    const auto found = std::ranges::find_if(
        invalid, [&](const auto &item) { return item.assetId == id; });
    operation(*found);
    return rejectedWithDescriptors(descriptors, std::move(invalid));
  };
  auto islandDescriptors = std::vector<ShowcaseSceneDescriptor3D>(
      world.Descriptors().begin(), world.Descriptors().end());
  const auto islandTree = std::ranges::find_if(islandDescriptors, [](const auto &item) {
    return item.kind == ShowcaseSceneKind3D::Tree;
  });
  islandTree->meshAsset = "mutation/quiver_tree";
  auto boulderDescriptors = std::vector<ShowcaseSceneDescriptor3D>(
      world.Descriptors().begin(), world.Descriptors().end());
  const auto boulder = std::ranges::find_if(boulderDescriptors, [](const auto &item) {
    return item.kind == ShowcaseSceneKind3D::Rock;
  });
  boulder->meshAsset = "mutation/rock_09";
  auto missing = cooked;
  missing.erase(missing.begin());
  auto duplicate = cooked;
  duplicate.push_back(duplicate.front());
  if (!rejected(std::move(missing)) || !rejected(std::move(duplicate)) ||
      !mutate("forest_ground_04", [](auto &item) {
        item.schemaVersion = 2;
      }) ||
      !mutate("forest_ground_04", [](auto &item) {
        item.documents.front().sha256.assign(64, 'A');
      }) ||
      !mutate("forest_ground_04", [](auto &item) {
        item.documents.front().relativePath = "../outside.json";
      }) ||
      !mutate("forest_ground_04", [](auto &item) {
        item.documents.push_back(item.documents.front());
      }) ||
      !mutateWithDescriptors(islandDescriptors, "island_tree_01", [](auto &item) {
        item.materialSidecarBound = false;
      }) ||
      !mutateWithDescriptors(islandDescriptors, "island_tree_01", [](auto &item) { item.meshParts.pop_back(); }) ||
      !mutateWithDescriptors(islandDescriptors, "island_tree_01", [](auto &item) {
        item.meshParts.back().lodIndex = 1;
      }) ||
      !mutateWithDescriptors(boulderDescriptors, "boulder_01", [](auto &item) {
        item.meshParts.front().materialHandle = 0;
      }) ||
      !mutateWithDescriptors(boulderDescriptors, "boulder_01", [](auto &item) {
        item.meshParts.front().cookedSha256.assign(64, 'g');
      }) ||
      !mutateWithDescriptors(boulderDescriptors, "boulder_01", [](auto &item) {
        item.meshParts.front().sourceSha256.assign(64, '0');
      }) ||
      !mutate("forest_ground_04", [](auto &item) {
        item.materials.clear();
      }) ||
      !mutate("forest_ground_04", [](auto &item) {
        item.materials.front().materialIndex = 1;
      }) ||
      !mutate("forest_ground_04", [](auto &item) { ++item.gpuBytes; }) ||
      !mutate("xanderklinge", [](auto &item) { item.iblHandle = 0; }) ||
      !mutate("stony_dirt_path", [](auto &item) {
        item.materials.clear();
      }) ||
      !mutate("Ground108", [](auto &item) { item.sha256.assign(64, '0'); }))
    return 12;
  std::cout << "SHOWCASE_RENDER_ASSET_BINDINGS_CONTROLLED_RESOLVER=PASS "
               "descriptors=" << load.descriptorCount << " assets="
            << expectedVisible.size()
            << " pathRenderOnly=true fallbackAtomic=true hash="
            << load.descriptorFingerprint << " identity="
            << load.assetIdentityFingerprint << " layers="
            << load.terrainLayerDescriptors[0] << '/'
            << load.terrainLayerDescriptors[1] << '/'
            << load.terrainLayerDescriptors[2] << '\n';
  return 0;
}
