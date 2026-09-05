#include "ShowcaseRenderAssetBindings3D.h"

#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/EnvironmentIbl3D.h>
#include <Aevrynd/StaticMeshAssetLibrary3D.h>

#include <Windows.h>
#include <bcrypt.h>
#include <winternl.h>

#include <algorithm>
#include <bit>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
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
    if (descriptor.materialAsset == "poly-haven/forest_ground_04/cooked-pbr")
      return "forest_ground_04";
    if (descriptor.materialAsset == "ambientcg/Ground108/cooked-pbr")
      return "Ground108";
    if (descriptor.materialAsset == "poly-haven/stony_dirt_path/cooked-pbr")
      return "stony_dirt_path";
    return {};
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

std::uint32_t AssetIdentityBit(std::string_view id) noexcept {
  constexpr std::array<std::string_view, 9> Required{
      "forest_ground_04", "Ground108", "stony_dirt_path",
      "coastal_cliff_01", "boulder_01", "island_tree_01",
      "dead_tree_trunk_02", "RoofingTiles014B", "Bricks100"};
  const auto found = std::ranges::find(Required, id);
  return found == Required.end()
             ? 0u
             : 1u << std::uint32_t(std::distance(Required.begin(), found));
}

std::array<std::size_t, 3> TerrainLayerDescriptors(
    std::span<const ShowcaseSceneDescriptor3D> descriptors) noexcept {
  std::array<std::size_t, 3> result{};
  for (const auto &descriptor : descriptors) {
    if (descriptor.kind != ShowcaseSceneKind3D::Terrain &&
        descriptor.kind != ShowcaseSceneKind3D::Grass)
      continue;
    const auto id = AssetFor(descriptor);
    if (id == "forest_ground_04") ++result[0];
    else if (id == "Ground108") ++result[1];
    else if (id == "stony_dirt_path") ++result[2];
  }
  return result;
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

ShowcaseHlodDescriptorPolicy3D
ResolveShowcaseHlodDescriptorPolicy3D(ShowcaseSceneKind3D kind) noexcept {
  switch (kind) {
  case ShowcaseSceneKind3D::Rock:
    return ShowcaseHlodDescriptorPolicy3D::AggregateOpaque;
  case ShowcaseSceneKind3D::Terrain:
    return ShowcaseHlodDescriptorPolicy3D::DedicatedTerrain;
  case ShowcaseSceneKind3D::Water:
    return ShowcaseHlodDescriptorPolicy3D::DedicatedWater;
  case ShowcaseSceneKind3D::Tree:
  case ShowcaseSceneKind3D::Grass:
    return ShowcaseHlodDescriptorPolicy3D::DedicatedVegetation;
  case ShowcaseSceneKind3D::Building:
    return ShowcaseHlodDescriptorPolicy3D::DedicatedBuilding;
  }
  return ShowcaseHlodDescriptorPolicy3D::FailClosed;
}

namespace {
void HashShowcaseAggregateBytes(std::uint64_t &hash, const void *data,
                                std::size_t bytes) noexcept {
  const auto *items = static_cast<const std::uint8_t *>(data);
  for (std::size_t index{}; index < bytes; ++index) {
    hash ^= items[index];
    hash *= 1099511628211ull;
  }
}

std::uint64_t ShowcaseAggregateMeshIdentity(const Aevrynd::Mesh3D &mesh) noexcept {
  std::uint64_t hash = 1469598103934665603ull;
  HashShowcaseAggregateBytes(hash, mesh.vertices.data(),
                             mesh.vertices.size() * sizeof(Aevrynd::Vertex3D));
  HashShowcaseAggregateBytes(hash, mesh.indices.data(),
                             mesh.indices.size() * sizeof(std::uint32_t));
  return hash ? hash : 1;
}

constexpr std::uint64_t MaximumAggregateCacheBytes = 512ull * 1024ull * 1024ull;

bool CheckedAggregateByteSize(const Aevrynd::Mesh3D &mesh,
                              std::uint64_t &vertexBytes,
                              std::uint64_t &indexBytes,
                              std::uint64_t &fileBytes) noexcept {
  constexpr std::uint64_t headerBytes = sizeof(std::uint32_t) * 4;
  if (!mesh.Valid() || mesh.vertices.size() >
          (std::numeric_limits<std::uint32_t>::max)() ||
      mesh.indices.size() > (std::numeric_limits<std::uint32_t>::max)())
    return false;
  vertexBytes = std::uint64_t(mesh.vertices.size()) * sizeof(Aevrynd::Vertex3D);
  indexBytes = std::uint64_t(mesh.indices.size()) * sizeof(std::uint32_t);
  if (vertexBytes > MaximumAggregateCacheBytes - headerBytes ||
      indexBytes > MaximumAggregateCacheBytes - headerBytes - vertexBytes)
    return false;
  fileBytes = headerBytes + vertexBytes + indexBytes;
  return true;
}

std::array<std::uint8_t, 32>
AggregateDigest(const Aevrynd::Mesh3D &mesh) noexcept {
  std::array<std::uint8_t, 32> digest{};
  std::uint64_t vertexBytes{}, indexBytes{}, fileBytes{};
  if (!CheckedAggregateByteSize(mesh, vertexBytes, indexBytes, fileBytes))
    return digest;
  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0)
    return digest;
  const auto close = [&] {
    if (hash)
      BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
  };
  if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
    close();
    return digest;
  }
  const std::array<std::uint64_t, 2> counts{mesh.vertices.size(),
                                           mesh.indices.size()};
  const auto add = [&](const void *bytes, std::uint64_t count) {
    const auto *cursor = static_cast<const std::uint8_t *>(bytes);
    while (count) {
      const auto part = static_cast<ULONG>((std::min)(
          count, std::uint64_t{(std::numeric_limits<ULONG>::max)()}));
      if (BCryptHashData(hash, const_cast<PUCHAR>(cursor), part, 0) < 0)
        return false;
      cursor += part;
      count -= part;
    }
    return true;
  };
  const bool success = add(counts.data(), sizeof(counts)) &&
      add(mesh.vertices.data(), vertexBytes) &&
      add(mesh.indices.data(), indexBytes) &&
      BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0;
  close();
  if (!success)
    digest = {};
  return digest;
}

class WinHandle final {
public:
  WinHandle() = default;
  explicit WinHandle(HANDLE value) noexcept : value_(value) {}
  ~WinHandle() { Reset(); }
  WinHandle(const WinHandle &) = delete;
  WinHandle &operator=(const WinHandle &) = delete;
  WinHandle(WinHandle &&other) noexcept : value_(other.Release()) {}
  WinHandle &operator=(WinHandle &&other) noexcept {
    if (this != &other) {
      Reset();
      value_ = other.Release();
    }
    return *this;
  }
  [[nodiscard]] HANDLE Get() const noexcept { return value_; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return value_ && value_ != INVALID_HANDLE_VALUE;
  }
  HANDLE Release() noexcept {
    const auto value = value_;
    value_ = nullptr;
    return value;
  }
  void Reset(HANDLE value = nullptr) noexcept {
    if (*this)
      CloseHandle(value_);
    value_ = value;
  }
private:
  HANDLE value_{};
};

using NtCreateFilePointer = NTSTATUS(NTAPI *)(
    PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER,
    ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
using NtSetInformationFilePointer = NTSTATUS(NTAPI *)(
    HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

NtCreateFilePointer ResolveNtCreateFile() noexcept {
  const auto module = GetModuleHandleW(L"ntdll.dll");
  return module ? reinterpret_cast<NtCreateFilePointer>(
                      GetProcAddress(module, "NtCreateFile"))
                : nullptr;
}

NtSetInformationFilePointer ResolveNtSetInformationFile() noexcept {
  const auto module = GetModuleHandleW(L"ntdll.dll");
  return module ? reinterpret_cast<NtSetInformationFilePointer>(
                      GetProcAddress(module, "NtSetInformationFile"))
                : nullptr;
}

bool IsUnlinkedRegularFile(HANDLE handle) noexcept {
  FILE_ATTRIBUTE_TAG_INFO tag{};
  BY_HANDLE_FILE_INFORMATION information{};
  return GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag,
                                      sizeof(tag)) &&
      !(tag.FileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
                              FILE_ATTRIBUTE_REPARSE_POINT)) &&
      GetFileInformationByHandle(handle, &information) &&
      information.nNumberOfLinks == 1;
}

bool IsPlainDirectory(HANDLE handle) noexcept {
  FILE_ATTRIBUTE_TAG_INFO tag{};
  return GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag,
                                      sizeof(tag)) &&
      (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
      !(tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

WinHandle OpenRelative(HANDLE parent, std::wstring_view name,
                       ACCESS_MASK access, ULONG disposition, ULONG options,
                       NTSTATUS &status,
                       ULONG shareAccess = FILE_SHARE_READ |
                                           FILE_SHARE_WRITE) noexcept {
  status = static_cast<NTSTATUS>(0xc0000001L);
  const auto ntCreateFile = ResolveNtCreateFile();
  if (!ntCreateFile || name.empty() || name.size() > 32767)
    return {};
  UNICODE_STRING unicode{};
  unicode.Buffer = const_cast<PWSTR>(name.data());
  unicode.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
  unicode.MaximumLength = unicode.Length;
  OBJECT_ATTRIBUTES attributes{};
  InitializeObjectAttributes(&attributes, &unicode, OBJ_CASE_INSENSITIVE,
                             parent, nullptr);
  IO_STATUS_BLOCK io{};
  HANDLE handle{};
  status = ntCreateFile(&handle, access, &attributes, &io, nullptr,
                        FILE_ATTRIBUTE_NORMAL,
                        shareAccess, disposition,
                        options | FILE_OPEN_REPARSE_POINT |
                            FILE_SYNCHRONOUS_IO_NONALERT,
                        nullptr, 0);
  return status >= 0 ? WinHandle(handle) : WinHandle{};
}

bool WriteAll(HANDLE handle, const void *data, std::uint64_t bytes) noexcept {
  const auto *cursor = static_cast<const std::uint8_t *>(data);
  while (bytes) {
    const DWORD request = static_cast<DWORD>((std::min)(
        bytes, std::uint64_t{(std::numeric_limits<DWORD>::max)()}));
    DWORD written{};
    if (!WriteFile(handle, cursor, request, &written, nullptr) ||
        written != request)
      return false;
    cursor += request;
    bytes -= request;
  }
  return true;
}

bool ReadAll(HANDLE handle, void *data, std::uint64_t bytes) noexcept {
  auto *cursor = static_cast<std::uint8_t *>(data);
  while (bytes) {
    const DWORD request = static_cast<DWORD>((std::min)(
        bytes, std::uint64_t{(std::numeric_limits<DWORD>::max)()}));
    DWORD read{};
    if (!ReadFile(handle, cursor, request, &read, nullptr) || read != request)
      return false;
    cursor += request;
    bytes -= request;
  }
  return true;
}

bool VerifyPinnedMesh(HANDLE handle,
                      const std::array<std::uint8_t, 32> &expected) noexcept {
  struct Header { std::uint32_t magic{}, version{}, vertices{}, indices{}; };
  LARGE_INTEGER zero{};
  if (!SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN))
    return false;
  Header header{};
  if (!ReadAll(handle, &header, sizeof(header)) || header.magic != 0x334d4541u ||
      header.version != 1 || !header.vertices || !header.indices ||
      header.indices % 3)
    return false;
  Aevrynd::Mesh3D mesh;
  try {
    mesh.vertices.resize(header.vertices);
    mesh.indices.resize(header.indices);
  } catch (...) {
    return false;
  }
  std::uint64_t vertexBytes{}, indexBytes{}, fileBytes{};
  if (!CheckedAggregateByteSize(mesh, vertexBytes, indexBytes, fileBytes))
    return false;
  LARGE_INTEGER actual{};
  if (!GetFileSizeEx(handle, &actual) || actual.QuadPart < 0 ||
      std::uint64_t(actual.QuadPart) != fileBytes ||
      !ReadAll(handle, mesh.vertices.data(), vertexBytes) ||
      !ReadAll(handle, mesh.indices.data(), indexBytes) || !mesh.Valid())
    return false;
  return AggregateDigest(mesh) == expected;
}

std::wstring DigestName(const std::array<std::uint8_t, 32> &digest) {
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring result = L"proxy-";
  result.reserve(6 + digest.size() * 2 + 7);
  for (const auto byte : digest) {
    result.push_back(hex[byte >> 4]);
    result.push_back(hex[byte & 15]);
  }
  result += L".aemesh";
  return result;
}

class PinnedAggregateCacheFile final {
public:
  ~PinnedAggregateCacheFile() { Remove(); }
  [[nodiscard]] const std::filesystem::path &Path() const noexcept {
    return path_;
  }
  bool Open(const std::filesystem::path &assetRoot,
            const Aevrynd::Mesh3D &mesh,
            const std::array<std::uint8_t, 32> &digest) noexcept {
    Remove();
    try {
      if (assetRoot.empty() || digest == std::array<std::uint8_t, 32>{})
        return false;
      root_.Reset(CreateFileW(assetRoot.c_str(),
          FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
              FILE_TRAVERSE | FILE_READ_ATTRIBUTES,
          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
      if (!root_ || !IsPlainDirectory(root_.Get()))
        return false;
      NTSTATUS status{};
      cacheParent_ = OpenRelative(root_.Get(), L".aevrynd-runtime-cache",
          FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
              FILE_TRAVERSE | FILE_READ_ATTRIBUTES | DELETE | SYNCHRONIZE,
          FILE_OPEN_IF, FILE_DIRECTORY_FILE, status);
      if (!cacheParent_ || !IsPlainDirectory(cacheParent_.Get()))
        return false;
      cacheDirectory_ = OpenRelative(cacheParent_.Get(), L"hlod-aggregate-proxy",
          FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
              FILE_TRAVERSE | FILE_READ_ATTRIBUTES | DELETE | SYNCHRONIZE,
          FILE_OPEN_IF, FILE_DIRECTORY_FILE, status);
      if (!cacheDirectory_ || !IsPlainDirectory(cacheDirectory_.Get()))
        return false;
      const auto finalName = DigestName(digest);
      file_ = OpenRelative(cacheDirectory_.Get(), finalName,
          FILE_GENERIC_READ | SYNCHRONIZE, FILE_OPEN,
          FILE_NON_DIRECTORY_FILE, status, FILE_SHARE_READ);
      if (file_) {
        if (!IsUnlinkedRegularFile(file_.Get()) ||
            !VerifyPinnedMesh(file_.Get(), digest))
          return false;
      } else {
        constexpr NTSTATUS NameNotFound = static_cast<NTSTATUS>(0xc0000034L);
        if (status != NameNotFound)
          return false;
        std::array<std::uint8_t, 16> nonce{};
        static std::atomic<std::uint64_t> sequence{};
        if (BCryptGenRandom(nullptr, nonce.data(), ULONG(nonce.size()),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
          return false;
        std::wostringstream temporaryName;
        temporaryName << L".tmp-" << GetCurrentProcessId() << L'-'
                      << sequence.fetch_add(1, std::memory_order_relaxed);
        for (const auto byte : nonce)
          temporaryName << std::hex << std::setw(2) << std::setfill(L'0')
                        << unsigned(byte);
        file_ = OpenRelative(cacheDirectory_.Get(), temporaryName.str(),
            FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE | SYNCHRONIZE,
            FILE_CREATE, FILE_NON_DIRECTORY_FILE, status, FILE_SHARE_READ);
        if (!file_ || !IsUnlinkedRegularFile(file_.Get()))
          return false;
        deleteOnClose_ = true;
        struct Header { std::uint32_t magic{}, version{}, vertices{}, indices{}; };
        std::uint64_t vertexBytes{}, indexBytes{}, fileBytes{};
        if (!CheckedAggregateByteSize(mesh, vertexBytes, indexBytes, fileBytes))
          return false;
        const Header header{0x334d4541u, 1u,
                            static_cast<std::uint32_t>(mesh.vertices.size()),
                            static_cast<std::uint32_t>(mesh.indices.size())};
        if (!WriteAll(file_.Get(), &header, sizeof(header)) ||
            !WriteAll(file_.Get(), mesh.vertices.data(), vertexBytes) ||
            !WriteAll(file_.Get(), mesh.indices.data(), indexBytes) ||
            !FlushFileBuffers(file_.Get()) ||
            !VerifyPinnedMesh(file_.Get(), digest))
          return false;
        const auto renameBytes = finalName.size() * sizeof(wchar_t);
        std::vector<std::byte> storage(offsetof(FILE_RENAME_INFO, FileName) +
                                       renameBytes);
        auto *rename = reinterpret_cast<FILE_RENAME_INFO *>(storage.data());
        rename->ReplaceIfExists = TRUE;
        rename->RootDirectory = cacheDirectory_.Get();
        rename->FileNameLength = static_cast<DWORD>(renameBytes);
        std::memcpy(rename->FileName, finalName.data(), renameBytes);
        IO_STATUS_BLOCK renameIo{};
        const auto ntSetInformationFile = ResolveNtSetInformationFile();
        const auto renameStatus = ntSetInformationFile
            ? ntSetInformationFile(file_.Get(), &renameIo, rename,
                                   ULONG(storage.size()),
                                   static_cast<FILE_INFORMATION_CLASS>(10))
            : static_cast<NTSTATUS>(0xc0000001L);
        if (renameStatus < 0 ||
            !IsUnlinkedRegularFile(file_.Get()) ||
            !VerifyPinnedMesh(file_.Get(), digest))
          return false;
        // Rename 需要 DELETE 權限；完成後立即丟棄該 handle，再以唯讀且拒絕
        // write/delete sharing 的相對開啟重新 pin，供正式 loader 消費。
        file_.Reset();
        deleteOnClose_ = false;
        file_ = OpenRelative(cacheDirectory_.Get(), finalName,
            FILE_GENERIC_READ | SYNCHRONIZE, FILE_OPEN,
            FILE_NON_DIRECTORY_FILE, status, FILE_SHARE_READ);
        if (!file_ || !IsUnlinkedRegularFile(file_.Get()) ||
            !VerifyPinnedMesh(file_.Get(), digest))
          return false;
      }
      path_ = assetRoot / L".aevrynd-runtime-cache" /
              L"hlod-aggregate-proxy" / finalName;
      return true;
    } catch (...) {
      Remove();
      return false;
    }
  }
  void Remove() noexcept {
    if (file_ && deleteOnClose_) {
      FILE_DISPOSITION_INFO disposition{TRUE};
      SetFileInformationByHandle(file_.Get(), FileDispositionInfo,
                                 &disposition, sizeof(disposition));
    }
    file_.Reset();
    cacheDirectory_.Reset();
    cacheParent_.Reset();
    root_.Reset();
    path_.clear();
    deleteOnClose_ = false;
  }
private:
  WinHandle root_, cacheParent_, cacheDirectory_, file_;
  std::filesystem::path path_;
  bool deleteOnClose_{};
};

std::optional<Aevrynd::PreparedHlodProxyAsset3D>
PrepareShowcaseAggregateProxy3D(
    const Aevrynd::StaticMeshAssetLibrary3D &library,
    Aevrynd::StableObjectId3D worldId,
    Aevrynd::GlobalChunkCoordinate3D chunk,
    Aevrynd::HlodClusterId3D clusterId,
    Aevrynd::StaticMeshAssetDefinition3D definition,
    const Aevrynd::Mesh3D &aggregate,
    std::uint64_t expectedContentIdentity,
    const std::array<std::uint8_t, 32> &expectedDigest) noexcept {
  static std::mutex cacheMutex;
  std::scoped_lock lock(cacheMutex);
  try {
    if (!aggregate.Valid() || !expectedContentIdentity ||
        expectedDigest == std::array<std::uint8_t, 32>{} ||
        AggregateDigest(aggregate) != expectedDigest)
      return std::nullopt;
    PinnedAggregateCacheFile cache;
    if (!cache.Open(library.AssetRoot(), aggregate, expectedDigest))
      return std::nullopt;
    definition.meshPath = cache.Path();
    definition.generateLods = false;
    definition.cpuRetention = Aevrynd::MeshLodCpuRetention3D::None;
    auto prepared = library.PrepareHlodProxyCpu3D(
        worldId, chunk, clusterId, 1, std::move(definition));
    if (!prepared || !Aevrynd::ValidatePreparedHlodProxyDescriptor3D(*prepared) ||
        prepared->prepared.assetKey.contentIdentity != expectedContentIdentity ||
        AggregateDigest(prepared->prepared.cpuMeshes[0]) != expectedDigest)
      return std::nullopt;
    return prepared;
  } catch (...) {
    return std::nullopt;
  }
}
} // namespace

std::array<std::uint8_t, 32>
ShowcaseAggregateProxyContentDigest3D(const Aevrynd::Mesh3D &mesh) noexcept {
  return AggregateDigest(mesh);
}

bool ExerciseShowcaseAggregateProxyCache3DForTesting(
    const std::filesystem::path &assetRoot, const Aevrynd::Mesh3D &mesh,
    const ShowcaseAggregateProxyReceipt3D &receipt) noexcept {
  if (!ValidateShowcaseAggregateProxyMesh3D(mesh, receipt))
    return false;
  PinnedAggregateCacheFile cache;
  if (!cache.Open(assetRoot, mesh, receipt.aggregateContentDigest))
    return false;
  WinHandle hostileWrite(CreateFileW(
      cache.Path().c_str(), GENERIC_WRITE | DELETE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (hostileWrite || DeleteFileW(cache.Path().c_str()))
    return false;
  return true;
}

bool ValidateShowcaseAggregateProxyMesh3D(
    const Aevrynd::Mesh3D &mesh,
    const ShowcaseAggregateProxyReceipt3D &receipt) noexcept {
  const auto digest = AggregateDigest(mesh);
  return mesh.Valid() && receipt.Complete() && receipt.sourceInstances >= 2 &&
      receipt.aggregateVertices == mesh.vertices.size() &&
      receipt.aggregateTriangles == mesh.indices.size() / 3 &&
      receipt.sourceVertices >= receipt.aggregateVertices &&
      receipt.sourceTriangles > receipt.aggregateTriangles &&
      receipt.aggregateTriangles < receipt.sourceTriangles &&
      receipt.sourceContractIdentity && receipt.aggregateContentIdentity &&
      ShowcaseAggregateMeshIdentity(mesh) == receipt.aggregateContentIdentity &&
      digest != std::array<std::uint8_t, 32>{} &&
      digest == receipt.aggregateContentDigest && receipt.materialCompatible &&
      receipt.immutableIdentityValidated;
}

bool BuildShowcaseAggregateProxyMesh3D(
    std::span<const ShowcaseAggregateProxySource3D> sources,
    Aevrynd::Mesh3D &mesh,
    ShowcaseAggregateProxyReceipt3D &receipt) noexcept {
  mesh = {};
  receipt = {};
  if (sources.size() < 2)
    return false;
  try {
    std::vector<const ShowcaseAggregateProxySource3D *> ordered;
    ordered.reserve(sources.size());
    for (const auto &source : sources) {
      const auto finite = [](Aevrynd::Vec3 value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
            std::isfinite(value.z);
      };
      if (!source.mesh || !source.mesh->Valid() || !source.objectId ||
          !source.generation || !source.meshContentIdentity ||
          !source.materialIdentity || !source.resourceOwnerIdentity ||
          source.meshContentIdentity !=
              ShowcaseAggregateMeshIdentity(*source.mesh) ||
          !finite(source.transform.position) ||
          !finite(source.transform.rotation) || !finite(source.transform.scale) ||
          source.transform.scale.x <= 0.f || source.transform.scale.y <= 0.f ||
          source.transform.scale.z <= 0.f)
        return false;
      ordered.push_back(&source);
    }
    std::ranges::sort(ordered, [](const auto *left, const auto *right) {
      return left->objectId != right->objectId
                 ? left->objectId < right->objectId
                 : left->generation < right->generation;
    });
    for (std::size_t index = 1; index < ordered.size(); ++index)
      if (ordered[index - 1]->objectId == ordered[index]->objectId)
        return false;

    const auto materialIdentity = ordered.front()->materialIdentity;
    std::size_t vertices{}, indices{};
    for (const auto *source : ordered) {
      if (source->materialIdentity != materialIdentity ||
          source->mesh->vertices.size() >
              (std::numeric_limits<std::uint32_t>::max)() - vertices ||
          source->mesh->vertices.size() >
              (std::numeric_limits<std::size_t>::max)() - vertices ||
          source->mesh->indices.size() >
              (std::numeric_limits<std::size_t>::max)() - indices)
        return false;
      vertices += source->mesh->vertices.size();
      indices += source->mesh->indices.size();
    }
    constexpr std::size_t MaximumTrianglesPerSource = 6000;
    if (sources.size() >
        (std::numeric_limits<std::uint32_t>::max)() /
            (MaximumTrianglesPerSource * 3))
      return false;
    const auto maximumAggregateTriangles =
        sources.size() * MaximumTrianglesPerSource;
    mesh.vertices.reserve(maximumAggregateTriangles * 3);
    mesh.indices.reserve(maximumAggregateTriangles * 3);
    std::uint64_t contract = 1469598103934665603ull;
    for (const auto *source : ordered) {
      HashShowcaseAggregateBytes(contract, &source->objectId,
                                 sizeof(source->objectId));
      HashShowcaseAggregateBytes(contract, &source->generation,
                                 sizeof(source->generation));
      HashShowcaseAggregateBytes(contract, &source->meshContentIdentity,
                                 sizeof(source->meshContentIdentity));
      HashShowcaseAggregateBytes(contract, &source->materialIdentity,
                                 sizeof(source->materialIdentity));
      HashShowcaseAggregateBytes(contract, &source->resourceOwnerIdentity,
                                 sizeof(source->resourceOwnerIdentity));
      const auto matrix = source->transform.Matrix();
      const auto normalMatrix = Aevrynd::CreateNormalMatrix(matrix);
      const auto sourceTriangles = source->mesh->indices.size() / 3;
      if (sourceTriangles < 2)
        return false;
      const auto selectedTriangles = (std::min)(
          MaximumTrianglesPerSource, (std::max)(std::size_t{1},
                                                sourceTriangles / 2));
      for (std::size_t selected{}; selected < selectedTriangles; ++selected) {
        const auto triangle = selected * sourceTriangles / selectedTriangles;
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t corner{}; corner < 3; ++corner) {
          const auto &input = source->mesh->vertices[
              source->mesh->indices[triangle * 3 + corner]];
          const auto position = Aevrynd::TransformPoint(
              matrix, {input.x, input.y, input.z});
          const auto packed = Aevrynd::UnpackNormalSnorm8(input.normal);
          const auto normal = Aevrynd::TransformNormal(
              normalMatrix, {packed.x, packed.y, packed.z});
          if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
              !std::isfinite(position.z) || Aevrynd::Length(normal) < .0001f)
            return false;
          mesh.vertices.push_back(
              {position.x, position.y, position.z,
               Aevrynd::PackNormalSnorm8(normal, packed.w), input.color,
               input.u, input.v});
        }
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2});
      }
    }
    receipt.sourceContractIdentity = contract ? contract : 1;
    receipt.aggregateContentIdentity = ShowcaseAggregateMeshIdentity(mesh);
    receipt.aggregateContentDigest = AggregateDigest(mesh);
    receipt.sourceInstances = ordered.size();
    receipt.sourceVertices = vertices;
    receipt.sourceTriangles = indices / 3;
    receipt.aggregateVertices = mesh.vertices.size();
    receipt.aggregateTriangles = mesh.indices.size() / 3;
    receipt.materialCompatible = true;
    receipt.immutableIdentityValidated = true;
    if (!ValidateShowcaseAggregateProxyMesh3D(mesh, receipt)) {
      mesh = {};
      receipt = {};
      return false;
    }
    return receipt.Complete();
  } catch (...) {
    mesh = {};
    receipt = {};
    return false;
  }
}

std::string_view ResolveShowcasePhotorealAssetId3D(
    const ShowcaseSceneDescriptor3D &descriptor) noexcept {
  return AssetFor(descriptor);
}

std::uint64_t ShowcaseAssetIdentityFingerprint3D(
    std::span<const ShowcaseSceneDescriptor3D> descriptors) noexcept {
  std::uint64_t hash = 14695981039346656037ull;
  const auto add = [&hash](const void *data, std::size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t byte = 0; byte < size; ++byte) {
      hash ^= bytes[byte];
      hash *= 1099511628211ull;
    }
  };
  std::size_t index{};
  for (const auto &descriptor : descriptors) {
    const auto id = AssetFor(descriptor);
    add(&index, sizeof(index));
    const auto bytes = std::uint64_t(id.size());
    add(&bytes, sizeof(bytes));
    add(id.data(), id.size());
    ++index;
  }
  return hash ? hash : 1;
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
  const std::uint64_t assetIdentityFingerprint =
      ShowcaseAssetIdentityFingerprint3D(descriptors);
  const auto terrainLayerDescriptors = TerrainLayerDescriptors(descriptors);
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
  std::uint32_t assetIdentityMask{};
  for (std::size_t index = 0; index < descriptors.size(); ++index) {
    const auto id = ResolveShowcasePhotorealAssetId3D(descriptors[index]);
    assetIdentityMask |= AssetIdentityBit(id);
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
    if (std::ranges::find(visible, path.assetId) == visible.end())
      visible.emplace_back(path.assetId);
    assetIdentityMask |= AssetIdentityBit(path.assetId);
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
    receipt.assetIdentityFingerprint = assetIdentityFingerprint;
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
  receipt.assetIdentityFingerprint = assetIdentityFingerprint;
  receipt.descriptorCount = descriptors.size();
  receipt.resolvedCount = bindings_.size() + 1u;
  receipt.admittedAssets = ownedAssets_.size();
  receipt.terrainLayerDescriptors = terrainLayerDescriptors;
  receipt.requiredAssetIdentityMask = assetIdentityMask;
  receipt.collisionStableIdsUnchanged =
      DescriptorFingerprint(descriptors) == descriptorFingerprint;
  receipt.descriptorOrderDeterministic =
      ShowcaseAssetIdentityFingerprint3D(descriptors) ==
      assetIdentityFingerprint;
  receipt.assetIdentitiesExact = assetIdentityMask == 0x1ffu &&
      std::ranges::all_of(terrainLayerDescriptors,
                          [](std::size_t count) { return count > 0; });
  receipt.photorealActive = receipt.collisionStableIdsUnchanged &&
      receipt.descriptorOrderDeterministic && receipt.assetIdentitiesExact;
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

bool ShowcaseRenderAssetBindings3D::BuildProductionHlodChunk3D(
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    Aevrynd::StableObjectId3D worldId,
    Aevrynd::GlobalChunkCoordinate3D chunk,
    std::shared_ptr<Aevrynd::TerrainVegetationChunkPayload3D> &payload,
    ShowcaseProductionHlodBuildReceipt3D &receipt) const {
  receipt = {};
  payload.reset();
  if (!production_ || !production_->active || !worldId || descriptors.empty())
    return false;

  struct SourceCandidate {
    std::size_t descriptorIndex{}, leaseIndex{};
    std::string assetId;
    Aevrynd::Transform3D transform{};
  };
  std::vector<SourceCandidate> candidates;
  receipt.inputDescriptors = descriptors.size();
  receipt.descriptorFingerprint = DescriptorFingerprint(descriptors);
  constexpr std::array DescriptorKinds{
      ShowcaseSceneKind3D::Terrain, ShowcaseSceneKind3D::Water,
      ShowcaseSceneKind3D::Tree, ShowcaseSceneKind3D::Grass,
      ShowcaseSceneKind3D::Rock, ShowcaseSceneKind3D::Building};
  receipt.descriptorPolicyHash = 1469598103934665603ull;
  for (const auto kind : DescriptorKinds) {
    const auto policy = ResolveShowcaseHlodDescriptorPolicy3D(kind);
    if (policy == ShowcaseHlodDescriptorPolicy3D::FailClosed)
      return false;
    const auto kindValue = static_cast<std::uint8_t>(kind);
    const auto policyValue = static_cast<std::uint8_t>(policy);
    receipt.coveredDescriptorKindMask |= std::uint8_t(1u << kindValue);
    HashShowcaseAggregateBytes(receipt.descriptorPolicyHash, &kindValue,
                               sizeof(kindValue));
    HashShowcaseAggregateBytes(receipt.descriptorPolicyHash, &policyValue,
                               sizeof(policyValue));
  }
  receipt.allDescriptorKindsCovered = receipt.coveredDescriptorKindMask == 0x3fu;
  try {
    candidates.reserve(descriptors.size());
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
      const auto &descriptor = descriptors[index];
      switch (ResolveShowcaseHlodDescriptorPolicy3D(descriptor.kind)) {
      case ShowcaseHlodDescriptorPolicy3D::DedicatedTerrain:
        ++receipt.excludedTerrain;
        continue;
      case ShowcaseHlodDescriptorPolicy3D::DedicatedWater:
        ++receipt.excludedWater;
        continue;
      case ShowcaseHlodDescriptorPolicy3D::DedicatedVegetation:
        if (descriptor.kind == ShowcaseSceneKind3D::Tree)
          ++receipt.excludedTrees;
        else
          ++receipt.excludedGrass;
        continue;
      case ShowcaseHlodDescriptorPolicy3D::DedicatedBuilding:
        ++receipt.excludedBuildings;
        continue;
      case ShowcaseHlodDescriptorPolicy3D::AggregateOpaque:
        break;
      case ShowcaseHlodDescriptorPolicy3D::FailClosed:
        return false;
      }
      const std::string assetId(ResolveShowcasePhotorealAssetId3D(descriptor));
      const std::string expectedMaterial =
          "poly-haven/" + assetId + "/cooked-pbr";
      const auto leases = production_->assetLeases.find(assetId);
      const auto bounds = production_->assetBounds.find(assetId);
      if (assetId.empty() || leases == production_->assetLeases.end() ||
          leases->second.size() != 1 || bounds == production_->assetBounds.end() ||
          !bounds->second.valid || descriptor.materialAsset != expectedMaterial)
        return false;
      Aevrynd::Transform3D normalized{};
      if (!NormalizeShowcaseAssetTransform3D(
              bounds->second.bounds,
              {descriptor.position, {0, descriptor.yaw, 0}, descriptor.scale},
              normalized))
        return false;
      candidates.push_back({index, leases->second.front(), assetId, normalized});
    }
  } catch (...) {
    return false;
  }
  if (candidates.size() < 2)
    return false;
  std::ranges::sort(candidates, [](const auto &left, const auto &right) {
    return left.assetId != right.assetId ? left.assetId < right.assetId
                                         : left.descriptorIndex < right.descriptorIndex;
  });

  constexpr std::size_t SourcesPerCluster = 5;
  std::vector<std::vector<Aevrynd::HlodSourceBuildInput3D>> sourceGroups;
  std::vector<Aevrynd::HlodClusterBuildInput3D> builds;
  std::vector<Aevrynd::TerrainVegetationHlodCluster3D> manifests;
  try {
    sourceGroups.reserve((candidates.size() + SourcesPerCluster - 1) /
                         SourcesPerCluster);
    builds.reserve(sourceGroups.capacity());
    manifests.reserve(sourceGroups.capacity());
  } catch (...) {
    return false;
  }

  std::size_t begin{};
  std::uint64_t clusterOrdinal{1};
  std::uint64_t aggregateContentIdentity{1469598103934665603ull};
  std::uint64_t aggregateResourceContractIdentity{1469598103934665603ull};
  std::size_t aggregateVertices{}, aggregateTriangles{};
  bool aggregateIdentitiesValidated{true};
  while (begin < candidates.size()) {
    std::size_t assetEnd = begin + 1;
    while (assetEnd < candidates.size() &&
           candidates[assetEnd].assetId == candidates[begin].assetId)
      ++assetEnd;
    for (std::size_t groupBegin = begin; groupBegin < assetEnd;) {
      const std::size_t remaining = assetEnd - groupBegin;
      std::size_t count = (std::min)(SourcesPerCluster, remaining);
      // 預先縮短當前群組，避免下一組只剩一個無合併價值的來源。
      if (remaining > SourcesPerCluster &&
          remaining - SourcesPerCluster == 1)
        --count;
      if (count < 2)
        return false;

      const auto leaseIndex = candidates[groupBegin].leaseIndex;
      if (leaseIndex >= production_->leases.size() ||
          leaseIndex >= production_->manifest.MeshParts().size())
        return false;
      const auto &lease = production_->leases[leaseIndex];
      const auto &part = production_->manifest.MeshParts()[leaseIndex];
      const auto *material = production_->manifest.FindMaterial(part.materialId);
      if (!lease || !material || part.lodPaths[2].empty() ||
          lease.Generation() > (std::numeric_limits<std::uint32_t>::max)())
        return false;
      const auto &renderable = lease.Renderable();
      if (renderable.lodLevelCount != 3)
        return false;
      const auto &sourceMesh = renderable.lodLevels[0].mesh;
      const auto &sourceMaterial = renderable.lodLevels[0].material.textures
                                       ? renderable.lodLevels[0].material
                                       : renderable.nearMaterial;
      const auto sourceShadow =
          Aevrynd::HlodShadowSemanticFromRenderable3D(renderable);
      if (!sourceMesh || !sourceMaterial.textures || !sourceShadow)
        return false;

      Aevrynd::Vec3 clusterCenter{};
      for (std::size_t offset = 0; offset < count; ++offset)
        clusterCenter = clusterCenter + candidates[groupBegin + offset].transform.position;
      clusterCenter = clusterCenter * (1.f / float(count));
      const std::uint64_t clusterHigh =
          (worldId.high ^ 0x484c4f445f524f43ull) &
          ~Aevrynd::HlodProxyIdentityDomainBit3D;
      const Aevrynd::HlodClusterId3D clusterId{
          clusterHigh ? clusterHigh : 0x484c4f44524f434bull, clusterOrdinal++};

      Aevrynd::StaticMeshAssetDefinition3D proxyDefinition{};
      proxyDefinition.meshPath = part.lodPaths[2];
      proxyDefinition.material = material->Definition();
      proxyDefinition.generateLods = false;
      proxyDefinition.cpuRetention = Aevrynd::MeshLodCpuRetention3D::None;
      auto representative = production_->meshes.PrepareHlodProxyCpu3D(
          worldId, chunk, clusterId, 1, proxyDefinition);
      if (!representative ||
          !Aevrynd::ValidatePreparedHlodProxyDescriptor3D(*representative) ||
          representative->prepared.levelCount != 1 ||
          !representative->prepared.cpuMeshes[0].Valid())
        return false;

      std::vector<Aevrynd::HlodSourceBuildInput3D> sources;
      std::vector<ShowcaseAggregateProxySource3D> aggregateSources;
      Aevrynd::TerrainVegetationHlodCluster3D manifest{};
      try {
        sources.reserve(count);
        aggregateSources.reserve(count);
        manifest.sources.reserve(count);
      } catch (...) {
        return false;
      }
      manifest.id = clusterId;
      manifest.generation = 1;
      float clusterRadius{};
      std::uint32_t sourceTriangles{};
      for (std::size_t offset = 0; offset < count; ++offset) {
        const auto &candidate = candidates[groupBegin + offset];
        if (candidate.leaseIndex != leaseIndex)
          return false;
        Aevrynd::HlodSourceBuildInput3D source{};
        source.reference.globalObjectId = {
            worldId.high & ~Aevrynd::HlodProxyIdentityDomainBit3D,
            candidate.descriptorIndex + 1};
        if (!source.reference.globalObjectId.high)
          source.reference.globalObjectId.high = 0x47414d45504c4159ull;
        source.generation = 1;
        source.meshHandle = sourceMesh.handle.value;
        source.materialHandle = sourceMaterial.textures.value;
        source.meshGeneration = static_cast<std::uint32_t>(lease.Generation());
        source.materialGeneration = source.meshGeneration;
        source.localBoundsCenter = sourceMesh.bounds.center;
        source.localBoundsExtents = sourceMesh.bounds.extents;
        source.position = candidate.transform.position - clusterCenter;
        source.rotation = candidate.transform.rotation;
        source.scale = candidate.transform.scale;
        source.shadow = *sourceShadow;
        source.meshContentPrimary = sourceMesh.handle.contentPrimary;
        source.meshContentSecondary = sourceMesh.handle.contentSecondary;
        source.materialSemanticIdentity = sourceMaterial.textures.semanticIdentity;
        source.resourceOwnerIdentity = renderable.resourceOwnerIdentity;
        aggregateSources.push_back(
            {&representative->prepared.cpuMeshes[0],
             {source.position, source.rotation, source.scale},
             source.reference.globalObjectId, source.generation,
             representative->prepared.assetKey.contentIdentity,
             source.materialSemanticIdentity, source.resourceOwnerIdentity});
        const auto transform = Aevrynd::CreateTransformMatrix(
            source.position, source.rotation, source.scale);
        for (unsigned corner = 0; corner < 8; ++corner) {
          const Aevrynd::Vec3 local = source.localBoundsCenter + Aevrynd::Vec3{
              source.localBoundsExtents.x * (corner & 1 ? 1.f : -1.f),
              source.localBoundsExtents.y * (corner & 2 ? 1.f : -1.f),
              source.localBoundsExtents.z * (corner & 4 ? 1.f : -1.f)};
          clusterRadius = (std::max)(
              clusterRadius, Aevrynd::Length(Aevrynd::TransformPoint(transform, local)));
        }
        const auto triangleCount = sourceMesh.indexCount / 3;
        if (triangleCount > (std::numeric_limits<std::uint32_t>::max)())
          return false;
        const auto triangles = static_cast<std::uint32_t>(triangleCount);
        if (triangles > (std::numeric_limits<std::uint32_t>::max)() -
                            sourceTriangles)
          return false;
        sourceTriangles += triangles;
        manifest.sources.push_back(
            {source.reference, source.generation, part.id,
             {chunk, candidate.transform.position}, candidate.transform.rotation,
             candidate.transform.scale});
        sources.push_back(source);
      }
      Aevrynd::Mesh3D aggregateMesh;
      ShowcaseAggregateProxyReceipt3D aggregateReceipt{};
      if (!BuildShowcaseAggregateProxyMesh3D(aggregateSources, aggregateMesh,
                                             aggregateReceipt) ||
          !ValidateShowcaseAggregateProxyMesh3D(aggregateMesh,
                                                aggregateReceipt))
        return false;
      auto proxy = PrepareShowcaseAggregateProxy3D(
          production_->meshes, worldId, chunk, clusterId, proxyDefinition,
          aggregateMesh, aggregateReceipt.aggregateContentIdentity,
          aggregateReceipt.aggregateContentDigest);
      if (!proxy ||
          !Aevrynd::ValidatePreparedHlodProxyDescriptor3D(*proxy) ||
          proxy->prepared.assetKey.contentIdentity !=
              aggregateReceipt.aggregateContentIdentity)
        return false;
      aggregateIdentitiesValidated = aggregateIdentitiesValidated &&
          aggregateReceipt.immutableIdentityValidated;
      HashShowcaseAggregateBytes(aggregateContentIdentity,
                                 &aggregateReceipt.aggregateContentIdentity,
                                 sizeof(aggregateReceipt.aggregateContentIdentity));
      HashShowcaseAggregateBytes(
          aggregateResourceContractIdentity,
          &aggregateReceipt.sourceContractIdentity,
          sizeof(aggregateReceipt.sourceContractIdentity));
      if (aggregateReceipt.aggregateVertices >
              (std::numeric_limits<std::size_t>::max)() - aggregateVertices ||
          aggregateReceipt.aggregateTriangles >
              (std::numeric_limits<std::size_t>::max)() - aggregateTriangles)
        return false;
      aggregateVertices += aggregateReceipt.aggregateVertices;
      aggregateTriangles += aggregateReceipt.aggregateTriangles;
      manifest.proxy = std::move(*proxy);
      clusterRadius = (std::max)(clusterRadius, .001f);
      const auto proxyTriangles =
          std::uint32_t(manifest.proxy.prepared.cpuMeshes[0].indices.size() / 3);
      if (!proxyTriangles || sourceTriangles < proxyTriangles)
        return false;
      const auto &proxyRenderable = manifest.proxy.prepared;
      const auto proxyShadow = Aevrynd::CanonicalHlodShadowSemantic3D(*sourceShadow);
      const auto instanceScale = candidates[groupBegin].transform.scale;
      const float maximumInstanceScale = (std::max)(
          std::abs(instanceScale.x),
          (std::max)(std::abs(instanceScale.y), std::abs(instanceScale.z)));
      if (!std::isfinite(maximumInstanceScale) || maximumInstanceScale <= 0.f)
        return false;
      // clusterRadius 已是世界空間；geometricError 會再乘 instance scale。
      // 遠帶最後一組 boulder 作固定過渡代表，並轉回 local 尺度，避免來源資產
      // 的原始單位或河岸重新分布讓整個世界完全跳過 transition。
      constexpr float TransitionRepresentativeWorldError = 14.f;
      const bool transitionRepresentative =
          candidates[groupBegin].assetId == "boulder_01" &&
          remaining == SourcesPerCluster;
      const float minimumWorldGeometricError =
          transitionRepresentative
              ? TransitionRepresentativeWorldError
              : 0.f;
      Aevrynd::HlodClusterBuildInput3D build{};
      build.asset = {clusterId, 1, {}, clusterRadius,
                     (std::max)(clusterRadius * .05f,
                                minimumWorldGeometricError /
                                    maximumInstanceScale),
                     .8f, 1.2f,
                     static_cast<std::uint32_t>(sources.size()),
                     Aevrynd::HlodCompatibility3D::OpaqueCompatible};
      build.resource = {
          proxyRenderable.assetKey.contentIdentity, sourceMaterial.textures.value,
          manifest.proxy.reservedPhysicalBytes, 1, 1, sourceTriangles,
          proxyTriangles, proxyShadow};
      build.instance = {clusterId, chunk, clusterCenter,
                        instanceScale,
                        candidates[groupBegin].transform.rotation};
      sourceGroups.push_back(std::move(sources));
      build.sources = sourceGroups.back();
      builds.push_back(build);
      manifests.push_back(std::move(manifest));
      groupBegin += count;
    }
    begin = assetEnd;
  }
  if (builds.empty())
    return false;
  auto data = Aevrynd::BuildHlodData3D(builds);
  if (!data)
    return false;
  auto manifest = Aevrynd::BuildTerrainVegetationHlodManifest3D(manifests, *data);
  if (!manifest)
    return false;
  bool resourceContractsValidated =
      data->Resources().size() == manifests.size() &&
      manifest->Clusters().size() == manifests.size();
  for (std::size_t index{}; resourceContractsValidated &&
       index < manifests.size(); ++index) {
    const auto contracts = data->SourceResourceContracts(index);
    resourceContractsValidated =
        contracts.size() == manifests[index].sources.size() &&
        data->Resources()[index].proxyMeshHandle ==
            manifests[index].proxy.prepared.assetKey.contentIdentity &&
        Aevrynd::ValidatePreparedHlodProxyDescriptor3D(
            manifests[index].proxy) &&
        std::ranges::all_of(
            contracts, Aevrynd::ValidHlodSourceResourceContract3D);
  }
  if (!resourceContractsValidated)
    return false;
  auto result = std::make_shared<Aevrynd::TerrainVegetationChunkPayload3D>();
  result->globalChunk = chunk;
  result->heightField = Aevrynd::HeightField3D({{}, 2, 2, 1.f});
  result->heightField.Fill(0.f);
  result->terrain = result->heightField.CreateMesh();
  if (!result->terrain.Valid())
    return false;
  result->hlod = std::move(data);
  result->hlodManifest = std::move(manifest);
  std::uint64_t retainedCpuBytes{};
  const auto addRetainedBytes = [&](std::size_t count,
                                    std::size_t stride) noexcept {
    constexpr auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    if (!stride || count > maximum / stride)
      return false;
    const auto bytes = std::uint64_t(count) * std::uint64_t(stride);
    if (bytes > maximum - retainedCpuBytes)
      return false;
    retainedCpuBytes += bytes;
    return true;
  };
  const auto addMeshRetainedBytes = [&](const Aevrynd::Mesh3D &mesh) noexcept {
    return addRetainedBytes(mesh.vertices.size(), sizeof(Aevrynd::Vertex3D)) &&
        addRetainedBytes(mesh.indices.size(), sizeof(std::uint32_t));
  };
  if (!addMeshRetainedBytes(result->terrain))
    return false;
  for (const auto &cluster : manifests)
    for (std::uint32_t level = 0; level < cluster.proxy.prepared.levelCount; ++level)
      if (!addMeshRetainedBytes(cluster.proxy.prepared.cpuMeshes[level]))
        return false;
  result->cpuBytes = retainedCpuBytes;
  result->conservativeGpuBytes = result->cpuBytes;
  receipt.eligibleRockDescriptors = candidates.size();
  receipt.sourceInstances = candidates.size();
  receipt.clusters = builds.size();
  receipt.hlodDataHash = result->hlod->ContentHash();
  receipt.hlodManifestHash = result->hlodManifest->ContentHash();
  receipt.aggregateProxyContentIdentity =
      aggregateContentIdentity ? aggregateContentIdentity : 1;
  receipt.aggregateResourceContractIdentity =
      aggregateResourceContractIdentity ? aggregateResourceContractIdentity : 1;
  receipt.aggregateProxyVertices = aggregateVertices;
  receipt.aggregateProxyTriangles = aggregateTriangles;
  receipt.usedProductionAssetLeases = true;
  receipt.representativeLowLodProxy = false;
  receipt.aggregateProxyBuilt = aggregateVertices && aggregateTriangles;
  receipt.aggregateResourceContractsValidated = resourceContractsValidated;
  receipt.aggregateIdentityValidated = aggregateIdentitiesValidated;
  if (!receipt.Complete())
    return false;
  payload = std::move(result);
  return true;
}

Aevrynd::StaticMeshAssetLibrary3D *
ShowcaseRenderAssetBindings3D::ProductionMeshLibrary3D() noexcept {
  return production_ && production_->active ? &production_->meshes : nullptr;
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
