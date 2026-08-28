#include "Aevrynd/EnvironmentIbl3D.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#pragma comment(lib, "bcrypt.lib")

namespace Aevrynd {
namespace {
constexpr std::uint32_t DdsMagic = 0x20534444u, Dx10FourCc = 0x30315844u;
constexpr std::uint32_t DxgiR16G16Float = 34, DxgiBc6hUf16 = 95, Texture2D = 3,
                        CubeFlag = 4;
constexpr std::size_t HeaderBytes = 4 + 124 + 20,
                      MaximumFileBytes = 64u * 1024u * 1024u;
std::uint32_t Read32(const std::vector<std::byte> &bytes, std::size_t offset) {
  return std::bit_cast<std::uint32_t>(std::array<std::byte, 4>{
      bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]});
}
bool CheckedAdd(std::size_t left, std::size_t right, std::size_t &result) {
  if (right > (std::numeric_limits<std::size_t>::max)() - left)
    return false;
  result = left + right;
  return true;
}
bool ReadFile(const std::filesystem::path &path,
              std::vector<std::byte> &bytes) {
  std::error_code error;
  const auto count = std::filesystem::file_size(path, error);
  if (error || count < HeaderBytes || count > MaximumFileBytes)
    return false;
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return false;
  bytes.resize(std::size_t(count));
  input.read(reinterpret_cast<char *>(bytes.data()),
             std::streamsize(bytes.size()));
  return input && input.peek() == std::ifstream::traits_type::eof();
}
bool Parse(const std::filesystem::path &path, EnvironmentIblTextureKind3D kind,
           EnvironmentIblTextureData3D &result) {
  EnvironmentIblTextureData3D candidate{};
  candidate.kind = kind;
  if (!ReadFile(path, candidate.fileBytes))
    return false;
  const auto &bytes = candidate.fileBytes;
  if (Read32(bytes, 0) != DdsMagic || Read32(bytes, 4) != 124 ||
      Read32(bytes, 76) != 32 || Read32(bytes, 80) != 4 ||
      Read32(bytes, 84) != Dx10FourCc)
    return false;
  candidate.height = Read32(bytes, 12);
  candidate.width = Read32(bytes, 16);
  candidate.mipCount = Read32(bytes, 28);
  if (!candidate.mipCount)
    candidate.mipCount = 1;
  const std::uint32_t format = Read32(bytes, 128),
                      dimension = Read32(bytes, 132), misc = Read32(bytes, 136),
                      arraySize = Read32(bytes, 140);
  const bool cube = kind != EnvironmentIblTextureKind3D::BrdfLut;
  candidate.faceCount = cube ? 6u : 1u;
  if (!candidate.width || !candidate.height || candidate.width > 4096 ||
      candidate.height > 4096 || candidate.mipCount > 13 ||
      dimension != Texture2D || arraySize != 1)
    return false;
  if (cube) {
    if (format != DxgiBc6hUf16 || (misc & CubeFlag) == 0 ||
        candidate.width != candidate.height)
      return false;
    // Specular 與 irradiance cubemap 都必須包含一路到 1x1 的完整 mip chain。
    std::uint32_t expectedMipCount = 1;
    for (std::uint32_t extent = candidate.width; extent > 1; extent >>= 1)
      ++expectedMipCount;
    if (candidate.mipCount != expectedMipCount)
      return false;
  } else if (format != DxgiR16G16Float || (misc & CubeFlag) != 0 ||
             candidate.mipCount != 1)
    return false;
  if (kind == EnvironmentIblTextureKind3D::SpecularCube &&
      candidate.width > 1024)
    return false;
  if (kind == EnvironmentIblTextureKind3D::IrradianceCube &&
      candidate.width > 128)
    return false;
  if (kind == EnvironmentIblTextureKind3D::BrdfLut &&
      (candidate.width > 512 || candidate.height > 512))
    return false;
  std::size_t offset = HeaderBytes;
  candidate.subresources.reserve(std::size_t(candidate.faceCount) *
                                 candidate.mipCount);
  for (std::uint32_t face = 0; face < candidate.faceCount; ++face)
    for (std::uint32_t mip = 0; mip < candidate.mipCount; ++mip) {
      const std::uint32_t width = (std::max)(1u, candidate.width >> mip),
                          height = (std::max)(1u, candidate.height >> mip);
      const std::size_t size =
          cube ? std::size_t((width + 3) / 4) * ((height + 3) / 4) * 16u
               : std::size_t(width) * height * 4u;
      std::size_t next{};
      if (!size || !CheckedAdd(offset, size, next) || next > bytes.size())
        return false;
      candidate.subresources.push_back(
          {width, height, face, mip, offset, size});
      offset = next;
    }
  if (offset != bytes.size())
    return false;
  result = std::move(candidate);
  return true;
}
} // namespace
std::size_t EnvironmentIblTextureData3D::PayloadBytes() const noexcept {
  std::size_t total{};
  for (const auto &item : subresources)
    total += item.bytes;
  return total;
}
bool LoadAndValidateEnvironmentIbl3D(
    const EnvironmentIblDefinition3D &definition,
    EnvironmentIblValidatedData3D &output) {
  if (!std::isfinite(definition.rotationRadians))
    return false;
  EnvironmentIblValidatedData3D candidate{};
  if (!Parse(definition.specularCubePath,
             EnvironmentIblTextureKind3D::SpecularCube, candidate.specular) ||
      !Parse(definition.irradianceCubePath,
             EnvironmentIblTextureKind3D::IrradianceCube,
             candidate.irradiance) ||
      !Parse(definition.brdfLutPath, EnvironmentIblTextureKind3D::BrdfLut,
             candidate.brdf))
    return false;
  candidate.payloadBytes = candidate.specular.PayloadBytes() +
                           candidate.irradiance.PayloadBytes() +
                           candidate.brdf.PayloadBytes();
  if (candidate.payloadBytes > 16u * 1024u * 1024u)
    return false;
  constexpr float Tau = 6.2831853071795864769f;
  candidate.rotationRadians = std::fmod(definition.rotationRadians, Tau);
  if (candidate.rotationRadians < 0.f)
    candidate.rotationRadians += Tau;
  if (candidate.rotationRadians == 0.f)
    candidate.rotationRadians = 0.f;
  output = std::move(candidate);
  return true;
}

struct EnvironmentFingerprint3D {
  std::filesystem::path path;
  std::array<std::uint8_t, 32> sha{};
  std::uint64_t size{}, fileId{};
  bool operator==(const EnvironmentFingerprint3D &) const = default;
};
struct EnvironmentLightingStore3D::Impl {
  struct Payload {
    EnvironmentIblValidatedData3D data;
    std::array<EnvironmentFingerprint3D, 3> fingerprints{};
  };
  struct PayloadEntry {
    std::shared_ptr<const Payload> payload;
    std::uint32_t instances{};
  };
  struct Source {
    std::shared_ptr<const Payload> current;
  };
  struct SourceKey {
    std::array<std::wstring, 3> paths;
  };
  struct SourceLookup {
    std::array<std::wstring_view, 3> paths;
  };
  struct SourceHash {
    using is_transparent = void;
    static std::size_t Hash(const SourceLookup &key) noexcept {
      std::size_t hash = sizeof(std::size_t) == 8
                             ? std::size_t(14695981039346656037ull)
                             : std::size_t(2166136261u);
      constexpr std::size_t prime = sizeof(std::size_t) == 8
                                        ? std::size_t(1099511628211ull)
                                        : std::size_t(16777619u);
      for (const auto path : key.paths) {
        for (const auto value : path)
          hash = (hash ^ std::size_t(value)) * prime;
        hash = (hash ^ std::size_t(0xffffu)) * prime;
      }
      return hash;
    }
    std::size_t operator()(const SourceLookup &key) const noexcept {
      return Hash(key);
    }
    std::size_t operator()(const SourceKey &key) const noexcept {
      return Hash({{key.paths[0], key.paths[1], key.paths[2]}});
    }
  };
  struct SourceEqual {
    using is_transparent = void;
    static bool Equal(const SourceLookup &left,
                      const SourceLookup &right) noexcept {
      return left.paths == right.paths;
    }
    bool operator()(const SourceKey &left,
                    const SourceKey &right) const noexcept {
      return left.paths == right.paths;
    }
    bool operator()(const SourceKey &left,
                    const SourceLookup &right) const noexcept {
      return Equal({{left.paths[0], left.paths[1], left.paths[2]}}, right);
    }
    bool operator()(const SourceLookup &left,
                    const SourceKey &right) const noexcept {
      return (*this)(right, left);
    }
  };
  struct InstanceKey {
    const Payload *payload{};
    std::uint32_t rotationBits{};
    bool operator==(const InstanceKey &) const = default;
  };
  struct InstanceHash {
    std::size_t operator()(const InstanceKey &key) const noexcept {
      const auto pointer = reinterpret_cast<std::uintptr_t>(key.payload);
      return std::hash<std::uintptr_t>{}(
          pointer ^ (std::uintptr_t(key.rotationBits) << 1));
    }
  };
  static SourceLookup RawLookup(const EnvironmentIblDefinition3D &definition) {
    return {{{definition.specularCubePath.native(),
              definition.irradianceCubePath.native(),
              definition.brdfLutPath.native()}}};
  }
  static bool CanonicalKey(const EnvironmentIblDefinition3D &definition,
                           SourceKey &key) {
    std::error_code error;
    std::size_t index{};
    for (const auto *path : {&definition.specularCubePath,
                             &definition.irradianceCubePath,
                             &definition.brdfLutPath}) {
      const auto canonical = std::filesystem::weakly_canonical(*path, error);
      if (error)
        return false;
      key.paths[index++] = canonical.native();
    }
    return true;
  }
  static SourceKey OwnedKey(const SourceLookup &lookup) {
    return {{{std::wstring(lookup.paths[0]), std::wstring(lookup.paths[1]),
              std::wstring(lookup.paths[2])}}};
  }
  struct Slot {
    std::shared_ptr<const Payload> payload;
    std::uint32_t generation{1}, references{};
    float rotationRadians{};
    bool retired{};
  };
  std::vector<Slot> slots;
  // multimap 允許雜湊鍵碰撞，命中後仍會逐位元比對完整 payload。
  std::unordered_map<InstanceKey, std::uint32_t, InstanceHash> instances;
  std::unordered_map<SourceKey, std::shared_ptr<Source>, SourceHash,
                     SourceEqual>
      rawSources;
  std::unordered_map<SourceKey, std::shared_ptr<Source>, SourceHash,
                     SourceEqual>
      canonicalSources;
  std::unordered_multimap<std::wstring, PayloadEntry> payloads;
  EnvironmentLightingHandle3D active{};
  EnvironmentLightingMemoryStats3D stats{};
  EnvironmentLightingStoreTestLimits3D limits{};
};
namespace {
template <typename Value>
bool CanAdd(Value current, Value addition, Value limit) {
  return current <= limit && addition <= limit - current;
}
template <typename Value>
void SaturatingAdd(Value &value, Value addition, Value limit) {
  value = CanAdd(value, addition, limit) ? Value(value + addition) : limit;
}
bool FingerprintFile(const std::filesystem::path &source,
                     const std::vector<std::byte> &bytes,
                     EnvironmentFingerprint3D &out) {
  std::error_code error;
  out.path = std::filesystem::weakly_canonical(source, error);
  if (error)
    return false;
  out.size = bytes.size();
  HANDLE file =
      CreateFileW(out.path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  BY_HANDLE_FILE_INFORMATION info{};
  const bool got = GetFileInformationByHandle(file, &info);
  CloseHandle(file);
  if (!got)
    return false;
  out.fileId = (std::uint64_t(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
  BCRYPT_ALG_HANDLE algorithm{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0)
    return false;
  const NTSTATUS status = BCryptHash(
      algorithm, nullptr, 0,
      reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
      ULONG(bytes.size()), out.sha.data(), ULONG(out.sha.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return status >= 0;
}
std::uint64_t HashBytes(std::span<const std::byte> bytes) {
  std::uint64_t hash = 14695981039346656037ull;
  for (auto value : bytes)
    hash = (hash ^ std::to_integer<unsigned char>(value)) * 1099511628211ull;
  return hash;
}
std::wstring PayloadKey(const EnvironmentIblValidatedData3D &v) {
  std::wstring key;
  const auto add = [&](const EnvironmentIblTextureData3D &data) {
    key += std::to_wstring(HashBytes(data.fileBytes));
    key.push_back(L':');
    key += std::to_wstring(data.fileBytes.size());
    key.push_back(L';');
  };
  add(v.specular);
  add(v.irradiance);
  add(v.brdf);
  return key;
}
bool SameSubresource(const EnvironmentIblSubresource3D &left,
                     const EnvironmentIblSubresource3D &right) {
  return left.width == right.width && left.height == right.height &&
         left.face == right.face && left.mip == right.mip &&
         left.offset == right.offset && left.bytes == right.bytes;
}
bool SameTexture(const EnvironmentIblTextureData3D &left,
                 const EnvironmentIblTextureData3D &right) {
  return left.kind == right.kind && left.width == right.width &&
         left.height == right.height && left.mipCount == right.mipCount &&
         left.faceCount == right.faceCount &&
         left.fileBytes == right.fileBytes &&
         left.subresources.size() == right.subresources.size() &&
         std::equal(left.subresources.begin(), left.subresources.end(),
                    right.subresources.begin(), SameSubresource);
}
template <typename Payload>
bool SamePayload(const Payload &left,
                 const EnvironmentIblValidatedData3D &right) {
  return left.data.payloadBytes == right.payloadBytes &&
         SameTexture(left.data.specular, right.specular) &&
         SameTexture(left.data.irradiance, right.irradiance) &&
         SameTexture(left.data.brdf, right.brdf);
}
bool SameFingerprints(
    const std::array<EnvironmentFingerprint3D, 3> &left,
    const std::array<EnvironmentFingerprint3D, 3> &right) {
  for (std::size_t index = 0; index < left.size(); ++index)
    if (left[index].size != right[index].size ||
        left[index].sha != right[index].sha)
      return false;
  return true;
}
EnvironmentLightingHandle3D MakeHandle(std::uint32_t index,
                                       std::uint32_t generation) {
  return {std::uint64_t(generation) << 32 | (index + 1)};
}
bool Decode(EnvironmentLightingHandle3D h, std::uint32_t &index,
            std::uint32_t &generation) {
  index = std::uint32_t(h.value) - 1;
  generation = std::uint32_t(h.value >> 32);
  return h && generation;
}
bool NormalizeRotation(float input, float &output) {
  if (!std::isfinite(input))
    return false;
  constexpr float Tau = 6.2831853071795864769f;
  output = std::fmod(input, Tau);
  if (output < 0.f)
    output += Tau;
  if (output == 0.f)
    output = 0.f;
  return true;
}
} // namespace
EnvironmentLightingStore3D::EnvironmentLightingStore3D() = default;
EnvironmentLightingStore3D::~EnvironmentLightingStore3D() = default;
EnvironmentLightingStore3D::EnvironmentLightingStore3D(
    EnvironmentLightingStore3D &&) noexcept = default;
EnvironmentLightingStore3D &EnvironmentLightingStore3D::operator=(
    EnvironmentLightingStore3D &&) noexcept = default;
EnvironmentLightingHandle3D EnvironmentLightingStore3D::Create(
    const EnvironmentIblDefinition3D &definition) {
  return Acquire(definition, false);
}
EnvironmentLightingHandle3D EnvironmentLightingStore3D::Refresh(
    const EnvironmentIblDefinition3D &definition) {
  return Acquire(definition, true);
}
EnvironmentLightingHandle3D EnvironmentLightingStore3D::Acquire(
    const EnvironmentIblDefinition3D &definition, bool refresh) {
  try {
    float rotation{};
    if (!NormalizeRotation(definition.rotationRadians, rotation))
      return {};
    const auto rotationBits = std::bit_cast<std::uint32_t>(rotation);
    const auto rawLookup = Impl::RawLookup(definition);

    const auto acquireInstance = [&](const std::shared_ptr<const Impl::Payload>
                                         &payload,
                                     bool fast) {
      auto &state = *impl_;
      const Impl::InstanceKey key{payload.get(), rotationBits};
      if (const auto found = state.instances.find(key);
          found != state.instances.end()) {
        auto &slot = state.slots[found->second];
        if (!slot.references || slot.payload != payload ||
            slot.references >= state.limits.slotReferences ||
            state.stats.residentReferences >=
                state.limits.residentReferences)
          return EnvironmentLightingHandle3D{};
        ++slot.references;
        ++state.stats.residentReferences;
        SaturatingAdd(state.stats.cacheHits, std::uint64_t{1},
                      state.limits.eventCounter);
        if (fast)
          SaturatingAdd(state.stats.fastHits, std::uint64_t{1},
                        state.limits.eventCounter);
        return MakeHandle(found->second, slot.generation);
      }

      auto payloadEntry = state.payloads.end();
      for (auto found = state.payloads.begin(); found != state.payloads.end();
           ++found)
        if (found->second.payload == payload) {
          payloadEntry = found;
          break;
        }
      if (payloadEntry == state.payloads.end() ||
          payloadEntry->second.instances >= state.limits.payloadInstances ||
          state.stats.residentInstances >= state.limits.residentInstances ||
          state.stats.residentReferences >=
              state.limits.residentReferences)
        return EnvironmentLightingHandle3D{};

      std::size_t index = 0;
      for (; index < state.slots.size() &&
             (state.slots[index].references || state.slots[index].retired);
           ++index)
        ;
      if (index >= (std::numeric_limits<std::uint32_t>::max)())
        return EnvironmentLightingHandle3D{};
      Impl::Slot candidate{};
      static_assert(std::is_nothrow_move_constructible_v<Impl::Slot> &&
                    std::is_nothrow_move_assignable_v<Impl::Slot>);
      candidate.payload = payload;
      candidate.references = 1;
      candidate.rotationRadians = rotation;
      if (index < state.slots.size())
        candidate.generation = state.slots[index].generation;
      state.slots.reserve((std::max)(state.slots.size(), size_t(index + 1)));
      state.instances.reserve(state.instances.size() + 1);
      auto inserted = state.instances.end();
      try {
        inserted = state.instances.emplace(key, std::uint32_t(index)).first;
        if (index == state.slots.size())
          state.slots.emplace_back(std::move(candidate));
        else
          state.slots[index] = std::move(candidate);
      } catch (...) {
        if (inserted != state.instances.end())
          state.instances.erase(inserted);
        throw;
      }
      ++payloadEntry->second.instances;
      ++state.stats.residentInstances;
      ++state.stats.residentSets;
      ++state.stats.residentReferences;
      SaturatingAdd(state.stats.creates, std::uint64_t{1},
                    state.limits.eventCounter);
      if (fast)
        SaturatingAdd(state.stats.fastHits, std::uint64_t{1},
                      state.limits.eventCounter);
      return MakeHandle(std::uint32_t(index), state.slots[index].generation);
    };

    if (!refresh && impl_) {
      if (const auto source = impl_->rawSources.find(rawLookup);
          source != impl_->rawSources.end() && source->second->current)
        return acquireInstance(source->second->current, true);
    }

    Impl::SourceKey canonicalKey{};
    if (!Impl::CanonicalKey(definition, canonicalKey))
      return {};
    std::shared_ptr<Impl::Source> source;
    if (impl_) {
      // Refresh 必須依當下 canonical 目標選 Source，避免 CWD/reparse 改向後
      // 沿用 raw alias 舊 Source 而污染原 canonical source。
      if (const auto canonical = impl_->canonicalSources.find(canonicalKey);
          canonical != impl_->canonicalSources.end())
        source = canonical->second;
    }

    // Create 的 canonical alias 慢路徑只建 raw alias，不重新讀檔。
    if (!refresh && source && source->current) {
      auto &state = *impl_;
      const bool needsRawAlias = state.rawSources.find(rawLookup) ==
                                 state.rawSources.end();
      auto insertedRaw = state.rawSources.end();
      try {
        if (needsRawAlias) {
          const auto rawKey = Impl::OwnedKey(rawLookup);
          state.rawSources.reserve(state.rawSources.size() + 1);
          insertedRaw = state.rawSources.emplace(rawKey, source).first;
        }
        const auto handle = acquireInstance(source->current, false);
        if (!handle && insertedRaw != state.rawSources.end())
          state.rawSources.erase(insertedRaw);
        return handle;
      } catch (...) {
        if (insertedRaw != state.rawSources.end())
          state.rawSources.erase(insertedRaw);
        throw;
      }
    }

    EnvironmentIblValidatedData3D data;
    if (!LoadAndValidateEnvironmentIbl3D(definition, data))
      return {};
    std::array<EnvironmentFingerprint3D, 3> fingerprints{};
    if (!FingerprintFile(definition.specularCubePath, data.specular.fileBytes,
                         fingerprints[0]) ||
        !FingerprintFile(definition.irradianceCubePath,
                         data.irradiance.fileBytes, fingerprints[1]) ||
        !FingerprintFile(definition.brdfLutPath, data.brdf.fileBytes,
                         fingerprints[2]))
      return {};
    const auto payloadKey = PayloadKey(data);
    std::shared_ptr<const Impl::Payload> payload;
    if (impl_) {
      const auto [first, last] = impl_->payloads.equal_range(payloadKey);
      for (auto found = first; found != last; ++found)
        if (SameFingerprints(found->second.payload->fingerprints,
                             fingerprints) &&
            SamePayload(*found->second.payload, data)) {
          payload = found->second.payload;
          break;
        }
    }
    const bool createsPayload = !payload;
    if (createsPayload) {
      auto candidatePayload = std::make_shared<Impl::Payload>();
      candidatePayload->data = std::move(data);
      // rotation 屬於 instance；共享 payload 僅保存 texture triplet。
      candidatePayload->data.rotationRadians = 0.f;
      candidatePayload->fingerprints = fingerprints;
      payload = std::move(candidatePayload);
    }
    if (!impl_)
      impl_ = std::make_unique<Impl>();
    auto &state = *impl_;
    if (createsPayload &&
        (state.stats.payloadSets >= state.limits.payloadSets ||
         !CanAdd(state.stats.payloadBytes, payload->data.payloadBytes,
                 state.limits.payloadBytes)))
      return {};
    if (!source)
      source = std::make_shared<Impl::Source>();
    const bool needsRawAlias = state.rawSources.find(rawLookup) ==
                               state.rawSources.end();
    const bool needsCanonicalAlias =
        state.canonicalSources.find(canonicalKey) ==
        state.canonicalSources.end();
    const auto rawKey = needsRawAlias ? Impl::OwnedKey(rawLookup)
                                      : Impl::SourceKey{};
    if (createsPayload)
      state.payloads.reserve(state.payloads.size() + 1);
    if (needsRawAlias)
      state.rawSources.reserve(state.rawSources.size() + 1);
    if (needsCanonicalAlias)
      state.canonicalSources.reserve(state.canonicalSources.size() + 1);
    auto insertedPayload = state.payloads.end();
    auto insertedRaw = state.rawSources.end();
    auto insertedCanonical = state.canonicalSources.end();
    auto existingRaw = state.rawSources.find(rawLookup);
    try {
      if (createsPayload)
        insertedPayload = state.payloads.emplace(
            payloadKey, Impl::PayloadEntry{payload, 0});
      if (needsRawAlias)
        insertedRaw = state.rawSources.emplace(rawKey, source).first;
      if (needsCanonicalAlias)
        insertedCanonical =
            state.canonicalSources.emplace(canonicalKey, source).first;
      const auto handle = acquireInstance(payload, false);
      if (!handle) {
        if (insertedCanonical != state.canonicalSources.end())
          state.canonicalSources.erase(insertedCanonical);
        if (insertedRaw != state.rawSources.end())
          state.rawSources.erase(insertedRaw);
        if (insertedPayload != state.payloads.end())
          state.payloads.erase(insertedPayload);
        return EnvironmentLightingHandle3D{};
      }
      source->current = payload;
      // 所有可能失敗的配置都已完成，現在才以 noexcept shared_ptr assignment
      // 將 raw alias 原子重綁到當下 canonical Source。
      if (existingRaw != state.rawSources.end() && existingRaw->second != source)
        existingRaw->second = source;
      if (createsPayload) {
        ++state.stats.payloadSets;
        state.stats.payloadBytes += payload->data.payloadBytes;
        state.stats.residentBytes = state.stats.payloadBytes;
      }
      SaturatingAdd(state.stats.fileReads, std::uint64_t{3},
                    state.limits.eventCounter);
      if (refresh)
        SaturatingAdd(state.stats.refreshes, std::uint64_t{1},
                      state.limits.eventCounter);
      return handle;
    } catch (...) {
      if (insertedCanonical != state.canonicalSources.end())
        state.canonicalSources.erase(insertedCanonical);
      if (insertedRaw != state.rawSources.end())
        state.rawSources.erase(insertedRaw);
      if (insertedPayload != state.payloads.end())
        state.payloads.erase(insertedPayload);
      throw;
    }
  } catch (...) {
    return {};
  }
}
void EnvironmentLightingStore3D::Destroy(EnvironmentLightingHandle3D handle) {
  if (!impl_)
    return;
  std::uint32_t index{}, generation{};
  if (!Decode(handle, index, generation) || index >= impl_->slots.size())
    return;
  auto &slot = impl_->slots[index];
  if (slot.generation != generation || !slot.references)
    return;
  if (--slot.references) {
    --impl_->stats.residentReferences;
    return;
  }
  if (impl_->active == handle)
    impl_->active = {};
  impl_->instances.erase(
      Impl::InstanceKey{slot.payload.get(),
                        std::bit_cast<std::uint32_t>(slot.rotationRadians)});
  bool releasesPayload{};
  for (auto found = impl_->payloads.begin(); found != impl_->payloads.end();
       ++found)
    if (found->second.payload == slot.payload) {
      if (--found->second.instances == 0) {
        impl_->stats.payloadBytes -= found->second.payload->data.payloadBytes;
        --impl_->stats.payloadSets;
        impl_->payloads.erase(found);
        releasesPayload = true;
      }
      break;
    }
  if (releasesPayload) {
    for (auto found = impl_->rawSources.begin();
         found != impl_->rawSources.end();) {
      if (found->second->current == slot.payload)
        found = impl_->rawSources.erase(found);
      else
        ++found;
    }
    for (auto found = impl_->canonicalSources.begin();
         found != impl_->canonicalSources.end();) {
      if (found->second->current == slot.payload)
        found = impl_->canonicalSources.erase(found);
      else
        ++found;
    }
  }
  impl_->stats.residentBytes = impl_->stats.payloadBytes;
  --impl_->stats.residentInstances;
  --impl_->stats.residentSets;
  --impl_->stats.residentReferences;
  SaturatingAdd(impl_->stats.destroys, std::uint64_t{1},
                impl_->limits.eventCounter);
  slot.payload.reset();
  slot.rotationRadians = 0.f;
  if (slot.generation == (std::numeric_limits<std::uint32_t>::max)()) {
    slot.generation = 0;
    slot.retired = true;
  } else {
    ++slot.generation;
  }
}
bool EnvironmentLightingStore3D::Set(EnvironmentLightingHandle3D handle) {
  if (!impl_)
    return !handle;
  if (!handle) {
    impl_->active = {};
    return true;
  }
  std::uint32_t index{}, generation{};
  if (!Decode(handle, index, generation) || index >= impl_->slots.size() ||
      impl_->slots[index].generation != generation ||
      !impl_->slots[index].references)
    return false;
  impl_->active = handle;
  return true;
}
EnvironmentLightingHandle3D EnvironmentLightingStore3D::Active() const {
  return impl_ ? impl_->active : EnvironmentLightingHandle3D{};
}
EnvironmentLightingPayloadLease3D EnvironmentLightingStore3D::Resolve(
    EnvironmentLightingHandle3D handle) const noexcept {
  if (!impl_)
    return {};
  std::uint32_t index{}, generation{};
  if (!Decode(handle, index, generation) || index >= impl_->slots.size())
    return {};
  const auto &slot = impl_->slots[index];
  if (slot.retired || !slot.references || slot.generation != generation ||
      !slot.payload)
    return {};
  return {{slot.payload, &slot.payload->data}, slot.rotationRadians};
}
EnvironmentLightingMemoryStats3D
EnvironmentLightingStore3D::MemoryStats() const {
  if (!impl_)
    return {.trimCount = trimCount_};
  auto stats = impl_->stats;
  stats.trimCount = trimCount_;
  stats.sourceSets = impl_->canonicalSources.size();
  stats.instanceCacheEntries = impl_->instances.size();
  stats.payloadCacheEntries = impl_->payloads.size();
  stats.rawSourceEntries = impl_->rawSources.size();
  stats.canonicalSourceEntries = impl_->canonicalSources.size();
  for (const auto &slot : impl_->slots)
    SaturatingAdd(stats.retiredSlots, std::size_t(slot.retired),
                  (std::numeric_limits<std::size_t>::max)());
  const auto addMetadata = [&](std::size_t bytes) {
    SaturatingAdd(stats.metadataBytes, bytes,
                  (std::numeric_limits<std::size_t>::max)());
  };
  const auto addCount = [&](std::size_t count, std::size_t bytes) {
    addMetadata(count > (std::numeric_limits<std::size_t>::max)() / bytes
                    ? (std::numeric_limits<std::size_t>::max)()
                    : count * bytes);
  };
  addCount(impl_->slots.capacity(), sizeof(Impl::Slot));
  const auto addBuckets = [&](const auto &map) {
    addCount(map.bucket_count(), sizeof(void *));
    addCount(map.size(),
             sizeof(typename std::decay_t<decltype(map)>::value_type));
  };
  addBuckets(impl_->instances);
  addBuckets(impl_->rawSources);
  addBuckets(impl_->canonicalSources);
  addBuckets(impl_->payloads);
  for (const auto &[key, source] : impl_->rawSources) {
    (void)source;
    for (const auto &path : key.paths)
      addCount(path.capacity(), sizeof(wchar_t));
  }
  for (const auto &[key, source] : impl_->canonicalSources) {
    (void)source;
    for (const auto &path : key.paths)
      addCount(path.capacity(), sizeof(wchar_t));
  }
  addCount(stats.sourceSets, sizeof(Impl::Source));
  for (const auto &[key, entry] : impl_->payloads) {
    addCount(key.capacity(), sizeof(wchar_t));
    addMetadata(sizeof(Impl::Payload));
    addCount(entry.payload->data.specular.subresources.capacity(),
             sizeof(EnvironmentIblSubresource3D));
    addCount(entry.payload->data.irradiance.subresources.capacity(),
             sizeof(EnvironmentIblSubresource3D));
    addCount(entry.payload->data.brdf.subresources.capacity(),
             sizeof(EnvironmentIblSubresource3D));
  }
  stats.reclaimableBytes = stats.residentInstances ? 0 : stats.metadataBytes;
  return stats;
}
void EnvironmentLightingStore3D::Trim() {
  if (!impl_)
    return;
  if (impl_->stats.residentInstances)
    return;
  decltype(impl_->slots){}.swap(impl_->slots);
  decltype(impl_->instances){}.swap(impl_->instances);
  decltype(impl_->rawSources){}.swap(impl_->rawSources);
  decltype(impl_->canonicalSources){}.swap(impl_->canonicalSources);
  decltype(impl_->payloads){}.swap(impl_->payloads);
  SaturatingAdd(trimCount_, std::uint64_t{1},
                impl_->limits.eventCounter);
  if (!impl_->stats.residentInstances)
    impl_.reset();
}
void EnvironmentLightingStore3D::Shutdown() {
  if (!impl_)
    return;
  impl_->active = {};
  impl_->slots.clear();
  impl_->instances.clear();
  impl_->rawSources.clear();
  impl_->canonicalSources.clear();
  impl_->payloads.clear();
  impl_->stats.residentInstances = impl_->stats.residentReferences =
      impl_->stats.payloadSets = impl_->stats.payloadBytes =
          impl_->stats.residentSets = impl_->stats.residentBytes = 0;
  impl_.reset();
}
void EnvironmentLightingStore3D::TestingSetLimits(
    const EnvironmentLightingStoreTestLimits3D &limits) {
  if (!impl_)
    impl_ = std::make_unique<Impl>();
  impl_->limits = limits;
}
bool EnvironmentLightingStore3D::TestingSetFreeSlotGeneration(
    std::size_t index, std::uint32_t generation) {
  if (!impl_ || !generation || index >= impl_->slots.size())
    return false;
  auto &slot = impl_->slots[index];
  if (slot.references || slot.payload)
    return false;
  slot.generation = generation;
  slot.retired = false;
  return true;
}
} // namespace Aevrynd
