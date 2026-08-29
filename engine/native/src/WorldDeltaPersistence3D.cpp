#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <system_error>
#include <unordered_map>

namespace Aevrynd {
struct WorldChunkDeltaStore3D::SaveOwnerState {
  SaveOwnerState(std::uint64_t value, SaveCompletionSink3D completionSink)
      : ownerNamespace(value), sink(completionSink) {}
  const std::uint64_t ownerNamespace;
  std::mutex mutex{};
  std::condition_variable idle{};
  std::uint64_t lastAcceptedToken{};
  std::deque<WorldDeltaSaveResult3D> completions{};
  std::deque<std::size_t> completionBytes{};
  std::size_t inFlight{};
  bool attached{true};
  SaveCompletionSink3D sink{};
};
struct WorldChunkDeltaStore3D::StoreCore {
  std::mutex mutex{};
  std::unordered_map<std::uint64_t, std::weak_ptr<SaveOwnerState>> owners{};
  WorldChunkDeltaStore3D *store{};
  std::uint64_t nextOwnerNamespace{1};
  bool ownerNamespacesExhausted{};
};
namespace {
constexpr std::array<std::byte, 8> Magic{
    std::byte{'A'}, std::byte{'E'}, std::byte{'W'}, std::byte{'D'},
    std::byte{'L'}, std::byte{'T'}, std::byte{'A'}, std::byte{'2'}};
constexpr std::array<std::byte, 8> LegacyMagic{
    std::byte{'A'}, std::byte{'E'}, std::byte{'W'}, std::byte{'D'},
    std::byte{'L'}, std::byte{'T'}, std::byte{'A'}, std::byte{'1'}};
constexpr std::uint32_t Version = 1;
constexpr std::size_t FrameHeaderBytes = 8 + 8 + 4 + 32 + 32;
constexpr std::size_t LegacyFrameHeaderBytes = 8 + 8 + 4;
constexpr std::size_t MaximumDiagnosticBytes = 256;
bool CheckedAdd(std::size_t left, std::size_t right,
                std::size_t &result) noexcept {
  if (right > (std::numeric_limits<std::size_t>::max)() - left)
    return false;
  result = left + right;
  return true;
}
template <class T> void Put(std::vector<std::byte> &out, T value) {
  static_assert(std::is_integral_v<T>);
  using U = std::make_unsigned_t<T>;
  U bits = std::bit_cast<U>(value);
  for (std::size_t i = 0; i < sizeof(T); ++i)
    out.push_back(std::byte((bits >> (i * 8)) & 0xff));
}
template <class T>
bool Get(std::span<const std::byte> in, std::size_t &at, T &value) {
  static_assert(std::is_integral_v<T>);
  if (at > in.size() || sizeof(T) > in.size() - at)
    return false;
  using U = std::make_unsigned_t<T>;
  U bits{};
  for (std::size_t i = 0; i < sizeof(T); ++i)
    bits |= U(std::to_integer<unsigned>(in[at++])) << (i * 8);
  value = std::bit_cast<T>(bits);
  return true;
}
void PutId(std::vector<std::byte> &out, StableObjectId3D id) {
  Put(out, id.high);
  Put(out, id.low);
}
bool GetId(std::span<const std::byte> in, std::size_t &at,
           StableObjectId3D &id) {
  return Get(in, at, id.high) && Get(in, at, id.low);
}
void PutGlobal(std::vector<std::byte> &out, GlobalPosition3D v) {
  Put(out, v.chunk.x);
  Put(out, v.chunk.y);
  Put(out, v.chunk.z);
  Put(out, std::bit_cast<std::uint32_t>(v.offset.x));
  Put(out, std::bit_cast<std::uint32_t>(v.offset.y));
  Put(out, std::bit_cast<std::uint32_t>(v.offset.z));
}
bool GetGlobal(std::span<const std::byte> in, std::size_t &at,
               GlobalPosition3D &v) {
  std::uint32_t x{}, y{}, z{};
  if (!Get(in, at, v.chunk.x) || !Get(in, at, v.chunk.y) ||
      !Get(in, at, v.chunk.z) || !Get(in, at, x) || !Get(in, at, y) ||
      !Get(in, at, z))
    return false;
  v.offset = {std::bit_cast<float>(x), std::bit_cast<float>(y),
              std::bit_cast<float>(z)};
  return std::isfinite(v.offset.x) && std::isfinite(v.offset.y) &&
         std::isfinite(v.offset.z);
}
bool Valid(const PersistentBuilding3D &b) {
  return b.id.high || b.id.low
             ? std::isfinite(b.rotation.x) && std::isfinite(b.rotation.y) &&
                   std::isfinite(b.rotation.z) && std::isfinite(b.scale.x) &&
                   std::isfinite(b.scale.y) && std::isfinite(b.scale.z) &&
                   b.scale.x > 0 && b.scale.y > 0 && b.scale.z > 0
             : false;
}
std::filesystem::path ChunkDirectory(const WorldDeltaStoreSettings3D &settings,
                                     const WorldDeltaChunkKey3D &key) {
  wchar_t name[192]{};
  swprintf_s(name, L"%016llx%016llx-%08x-%08x-%016llx-%016llx-%016llx",
             static_cast<unsigned long long>(key.world.high),
             static_cast<unsigned long long>(key.world.low), key.layer,
             key.chunkSizeRevision,
             static_cast<unsigned long long>(key.chunk.x),
             static_cast<unsigned long long>(key.chunk.y),
             static_cast<unsigned long long>(key.chunk.z));
  return settings.root / L"chunks" / std::wstring(name, 2) / name;
}
bool WriteAll(HANDLE file, std::span<const std::byte> bytes) {
  while (!bytes.empty()) {
    const DWORD count = DWORD((std::min)(
        bytes.size(), std::size_t((std::numeric_limits<DWORD>::max)())));
    DWORD wrote{};
    if (!WriteFile(file, bytes.data(), count, &wrote, nullptr) ||
        wrote != count)
      return false;
    bytes = bytes.subspan(wrote);
  }
  return true;
}
bool AtomicWrite(const std::filesystem::path &target,
                 std::span<const std::byte> bytes) {
  const auto temp = target.wstring() + L".tmp";
  HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  const bool ok = WriteAll(file, bytes) && FlushFileBuffers(file);
  CloseHandle(file);
  if (!ok) {
    DeleteFileW(temp.c_str());
    return false;
  }
  if (!MoveFileExW(temp.c_str(), target.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(temp.c_str());
    return false;
  }
  return true;
}
bool ReadBytes(const std::filesystem::path &path, std::vector<std::byte> &bytes,
               std::size_t limit) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    return false;
  if (size > limit)
    return false;
  bytes.resize(std::size_t(size));
  std::ifstream input(path, std::ios::binary);
  return bytes.empty() ||
         bool(input.read(reinterpret_cast<char *>(bytes.data()),
                         std::streamsize(bytes.size())));
}
bool ChainedDigest(const std::array<std::byte, 32> &parent,
                   std::span<const std::byte> payload,
                   std::array<std::byte, 32> &digest) {
  std::size_t total{};
  if (!CheckedAdd(parent.size(), payload.size(), total))
    return false;
  try {
    std::vector<std::byte> chained;
    chained.reserve(total);
    chained.insert(chained.end(), parent.begin(), parent.end());
    chained.insert(chained.end(), payload.begin(), payload.end());
    return WorldDeltaSha2563D(chained, digest);
  } catch (const std::bad_alloc &) {
    return false;
  }
}
std::vector<std::byte>
Frame(std::span<const std::byte> payload,
      const std::array<std::byte, 32> &parent = {}) {
  std::vector<std::byte> result;
  std::array<std::byte, 32> digest{};
  if (!ChainedDigest(parent, payload, digest))
    return result;
  result.insert(result.end(), Magic.begin(), Magic.end());
  Put(result, std::uint64_t(payload.size()));
  Put(result, WorldDeltaCrc32C3D(payload));
  result.insert(result.end(), parent.begin(), parent.end());
  result.insert(result.end(), digest.begin(), digest.end());
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}
bool ParseFrame(std::span<const std::byte> bytes,
                WorldChunkDeltaSnapshot3D &snapshot, std::size_t maximum,
                bool allowTail, std::size_t *validBytes = nullptr,
                std::array<std::byte, 32> *finalDigest = nullptr) {
  std::size_t at{};
  WorldChunkDeltaSnapshot3D latest{};
  std::array<std::byte, 32> previousDigest{};
  bool any{};
  while (at < bytes.size()) {
    const std::size_t start = at;
    if (bytes.size() - at < LegacyFrameHeaderBytes) {
      if (allowTail)
        break;
      return false;
    }
    const bool legacy = std::equal(LegacyMagic.begin(), LegacyMagic.end(),
                                   bytes.begin() + std::ptrdiff_t(at));
    const bool current = std::equal(Magic.begin(), Magic.end(),
                                    bytes.begin() + std::ptrdiff_t(at));
    if (!legacy && !current)
      return false;
    if (current && bytes.size() - start < FrameHeaderBytes) {
      if (allowTail)
        break;
      return false;
    }
    at += Magic.size();
    std::uint64_t length{};
    std::uint32_t crc{};
    if (!Get(bytes, at, length) || !Get(bytes, at, crc) || length > maximum)
      return false;
    std::array<std::byte, 32> parentDigest{}, storedDigest{}, actualDigest{};
    if (current) {
      if (parentDigest.size() > bytes.size() - at)
        return false;
      std::copy_n(bytes.begin() + std::ptrdiff_t(at), parentDigest.size(),
                  parentDigest.begin());
      at += parentDigest.size();
      if (storedDigest.size() > bytes.size() - at)
        return false;
      std::copy_n(bytes.begin() + std::ptrdiff_t(at), storedDigest.size(),
                  storedDigest.begin());
      at += storedDigest.size();
    }
    if (length > bytes.size() - at) {
      if (allowTail) {
        at = start;
        break;
      }
      return false;
    }
    const auto payload = bytes.subspan(at, std::size_t(length));
    if (WorldDeltaCrc32C3D(payload) != crc ||
        !ChainedDigest(parentDigest, payload, actualDigest) ||
        (current && ((any && parentDigest != previousDigest) ||
                     actualDigest != storedDigest)))
      return false;
    if (legacy)
      storedDigest = actualDigest;
    WorldChunkDeltaSnapshot3D decoded{};
    if (!DecodeWorldChunkDeltaSnapshot3D(payload, decoded, maximum))
      return false;
    if (any && decoded.lastSequence < latest.lastSequence)
      return false;
    latest = std::move(decoded);
    previousDigest = storedDigest;
    any = true;
    at += std::size_t(length);
  }
  if (validBytes)
    *validBytes = at;
  if (!any)
    return bytes.empty();
  if (finalDigest)
    *finalDigest = previousDigest;
  snapshot = std::move(latest);
  return true;
}
} // namespace

std::uint32_t WorldDeltaCrc32C3D(std::span<const std::byte> bytes) noexcept {
  std::uint32_t crc = ~0u;
  for (const std::byte byte : bytes) {
    crc ^= std::to_integer<std::uint8_t>(byte);
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0x82f63b78u & std::uint32_t(0 - int(crc & 1)));
  }
  return ~crc;
}
bool WorldDeltaSha2563D(std::span<const std::byte> bytes,
                       std::array<std::byte, 32> &digest) noexcept {
  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0)
    return false;
  bool succeeded = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0,
                                    0) >= 0;
  auto remaining = bytes;
  while (succeeded && !remaining.empty()) {
    const auto count = static_cast<ULONG>((std::min)(
        remaining.size(),
        std::size_t((std::numeric_limits<ULONG>::max)())));
    succeeded = BCryptHashData(
                    hash,
                    reinterpret_cast<PUCHAR>(
                        const_cast<std::byte *>(remaining.data())),
                    count, 0) >= 0;
    remaining = remaining.subspan(count);
  }
  succeeded = succeeded &&
              BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(digest.data()),
                               static_cast<ULONG>(digest.size()), 0) >= 0;
  if (hash)
    BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return succeeded;
}
StableObjectId3D DeriveStableObjectId3D(StableObjectId3D world,
                                        GlobalChunkCoordinate3D chunk,
                                        std::uint64_t revision,
                                        std::uint64_t type,
                                        std::uint64_t ordinal) noexcept {
  std::uint64_t a = world.high ^ 0x9e3779b97f4a7c15ull,
                b = world.low ^ 0xd6e8feb86659fd93ull;
  auto mix = [](std::uint64_t &state, std::uint64_t value) {
    state ^= value + 0x9e3779b97f4a7c15ull + (state << 6) + (state >> 2);
    state ^= state >> 30;
    state *= 0xbf58476d1ce4e5b9ull;
    state ^= state >> 27;
    state *= 0x94d049bb133111ebull;
    state ^= state >> 31;
  };
  for (auto v : {std::uint64_t(chunk.x), std::uint64_t(chunk.y),
                 std::uint64_t(chunk.z), revision, type, ordinal}) {
    mix(a, v);
    mix(b, v ^ a);
  }
  if (!(a | b))
    b = 1;
  return {a, b};
}

static bool EncodeSnapshotImpl(const WorldChunkDeltaSnapshot3D &s,
                               std::vector<std::byte> &out,
                               std::size_t maximum) {
  out.clear();
  try {
    out.reserve(
        (std::min)(maximum, std::size_t(256) + s.operations.size() * 128));
    out.insert(out.end(), Magic.begin(), Magic.end());
    Put(out, Version);
    PutId(out, s.key.world);
    Put(out, s.key.layer);
    Put(out, s.key.chunkSizeRevision);
    Put(out, s.key.chunk.x);
    Put(out, s.key.chunk.y);
    Put(out, s.key.chunk.z);
    Put(out, s.base.generatorId);
    Put(out, s.base.generatorVersion);
    Put(out, s.base.worldSeed);
    Put(out, s.base.contentRevision);
    out.insert(out.end(), s.base.contentHash.begin(), s.base.contentHash.end());
    Put(out, s.durableGeneration);
    Put(out, s.lastSequence);
    Put(out, std::uint64_t(s.operations.size()));
    std::uint64_t previous{};
    for (const auto &op : s.operations) {
      if (!op.sequence || op.sequence <= previous ||
          op.sequence > s.lastSequence ||
          !(op.operationId.high | op.operationId.low))
        return false;
      previous = op.sequence;
      Put(out, op.sequence);
      Put(out, op.dirtyGeneration);
      Put(out, op.serverTick);
      PutId(out, op.operationId);
      Put(out, std::uint8_t(op.payload.index() + 1));
      if (const auto *place = std::get_if<PlaceBuildingDelta3D>(&op.payload)) {
        if (!Valid(place->building))
          return false;
        PutId(out, place->building.id);
        Put(out, place->building.assetId);
        Put(out, place->building.materialId);
        PutGlobal(out, place->building.position);
        for (float value :
             {place->building.rotation.x, place->building.rotation.y,
              place->building.rotation.z, place->building.scale.x,
              place->building.scale.y, place->building.scale.z})
          Put(out, std::bit_cast<std::uint32_t>(value));
        Put(out, place->building.collisionPreset);
      } else if (const auto *removeBuilding =
                     std::get_if<RemoveBuildingDelta3D>(&op.payload))
        PutId(out, removeBuilding->id);
      else if (const auto *removeVegetation =
                   std::get_if<RemoveVegetationDelta3D>(&op.payload))
        PutId(out, removeVegetation->id);
      else {
        const auto &height = std::get<SetTerrainHeightDelta3D>(op.payload);
        if (!std::isfinite(height.height))
          return false;
        Put(out, height.x);
        Put(out, height.z);
        Put(out, std::bit_cast<std::uint32_t>(height.height));
      }
      if (out.size() > maximum)
        return false;
    }
    const std::uint32_t crc = WorldDeltaCrc32C3D(out);
    Put(out, crc);
    return out.size() <= maximum;
  } catch (const std::bad_alloc &) {
    out.clear();
    return false;
  }
}
bool EncodeWorldChunkDeltaSnapshot3D(const WorldChunkDeltaSnapshot3D &snapshot,
                                     std::vector<std::byte> &output,
                                     std::size_t maximum) {
  std::vector<std::byte> encoded;
  if (!EncodeSnapshotImpl(snapshot, encoded, maximum))
    return false;
  output.swap(encoded);
  return true;
}

bool DecodeWorldChunkDeltaSnapshot3D(std::span<const std::byte> in,
                                     WorldChunkDeltaSnapshot3D &result,
                                     std::size_t maximum) {
  if (in.size() > maximum || in.size() < Magic.size() + 4 + 4)
    return false;
  const std::uint32_t stored =
      std::uint32_t(std::to_integer<unsigned>(in[in.size() - 4])) |
      (std::uint32_t(std::to_integer<unsigned>(in[in.size() - 3])) << 8) |
      (std::uint32_t(std::to_integer<unsigned>(in[in.size() - 2])) << 16) |
      (std::uint32_t(std::to_integer<unsigned>(in[in.size() - 1])) << 24);
  if (WorldDeltaCrc32C3D(in.first(in.size() - 4)) != stored)
    return false;
  std::size_t at{};
  if (!std::equal(Magic.begin(), Magic.end(), in.begin()) &&
      !std::equal(LegacyMagic.begin(), LegacyMagic.end(), in.begin()))
    return false;
  at += Magic.size();
  std::uint32_t version{};
  WorldChunkDeltaSnapshot3D s{};
  std::uint64_t count{};
  if (!Get(in, at, version) || version != Version ||
      !GetId(in, at, s.key.world) || !Get(in, at, s.key.layer) ||
      !Get(in, at, s.key.chunkSizeRevision) || !s.key.chunkSizeRevision ||
      !Get(in, at, s.key.chunk.x) || !Get(in, at, s.key.chunk.y) ||
      !Get(in, at, s.key.chunk.z) || !Get(in, at, s.base.generatorId) ||
      !Get(in, at, s.base.generatorVersion) || !Get(in, at, s.base.worldSeed) ||
      !Get(in, at, s.base.contentRevision))
    return false;
  if (at > in.size() || s.base.contentHash.size() > in.size() - at)
    return false;
  std::copy_n(in.begin() + std::ptrdiff_t(at), s.base.contentHash.size(),
              s.base.contentHash.begin());
  at += s.base.contentHash.size();
  if (!Get(in, at, s.durableGeneration) || !Get(in, at, s.lastSequence) ||
      !Get(in, at, count) || count > (maximum / 49))
    return false;
  try {
    s.operations.reserve(std::size_t(count));
  } catch (...) {
    return false;
  }
  std::uint64_t previous{};
  for (std::uint64_t i = 0; i < count; ++i) {
    WorldChunkDeltaOperation3D op{};
    std::uint8_t type{};
    if (!Get(in, at, op.sequence) || !Get(in, at, op.dirtyGeneration) ||
        !Get(in, at, op.serverTick) || !GetId(in, at, op.operationId) ||
        !Get(in, at, type) || !op.sequence || op.sequence <= previous ||
        op.sequence > s.lastSequence ||
        !(op.operationId.high | op.operationId.low))
      return false;
    previous = op.sequence;
    if (type == 1) {
      PlaceBuildingDelta3D p{};
      std::uint32_t values[6]{};
      if (!GetId(in, at, p.building.id) || !Get(in, at, p.building.assetId) ||
          !Get(in, at, p.building.materialId) ||
          !GetGlobal(in, at, p.building.position))
        return false;
      for (auto &v : values)
        if (!Get(in, at, v))
          return false;
      p.building.rotation = {std::bit_cast<float>(values[0]),
                             std::bit_cast<float>(values[1]),
                             std::bit_cast<float>(values[2])};
      p.building.scale = {std::bit_cast<float>(values[3]),
                          std::bit_cast<float>(values[4]),
                          std::bit_cast<float>(values[5])};
      if (!Get(in, at, p.building.collisionPreset) || !Valid(p.building))
        return false;
      op.payload = p;
    } else if (type == 2) {
      RemoveBuildingDelta3D p{};
      if (!GetId(in, at, p.id))
        return false;
      op.payload = p;
    } else if (type == 3) {
      RemoveVegetationDelta3D p{};
      if (!GetId(in, at, p.id))
        return false;
      op.payload = p;
    } else if (type == 4) {
      SetTerrainHeightDelta3D p{};
      std::uint32_t height{};
      if (!Get(in, at, p.x) || !Get(in, at, p.z) || !Get(in, at, height))
        return false;
      p.height = std::bit_cast<float>(height);
      if (!std::isfinite(p.height))
        return false;
      op.payload = p;
    } else
      return false;
    s.operations.push_back(std::move(op));
  }
  if (at != in.size() - 4)
    return false;
  result = std::move(s);
  return true;
}

WorldChunkDeltaStore3D::WorldChunkDeltaStore3D(WorldDeltaStoreSettings3D s)
    : settings_(std::move(s)), core_(std::make_shared<StoreCore>()) {
  core_->store = this;
  std::size_t perLoadCompletion{}, requiredLoadCompletions{};
  const bool completionBudgetValid =
      CheckedAdd(sizeof(WorldDeltaLoadResult3D) + MaximumDiagnosticBytes,
                 settings_.maximumSnapshotBytes, perLoadCompletion) &&
      settings_.minimumConcurrentLoadCompletions &&
      perLoadCompletion <=
          (std::numeric_limits<std::size_t>::max)() /
              settings_.minimumConcurrentLoadCompletions &&
      (requiredLoadCompletions =
           perLoadCompletion * settings_.minimumConcurrentLoadCompletions,
       settings_.maximumCompletedResults >=
               settings_.minimumConcurrentLoadCompletions &&
           settings_.maximumCompletedBytes >= requiredLoadCompletions);
  if (settings_.enabled && !settings_.root.empty() &&
      settings_.maximumQueuedRequests && settings_.maximumQueuedBytes &&
      settings_.maximumSnapshotBytes && settings_.maximumCompletedResults &&
      settings_.maximumCompletedBytes && completionBudgetValid)
    worker_ = std::jthread([this](std::stop_token stop) { Worker(stop); });
  else
    settings_.enabled = false;
}
WorldChunkDeltaStore3D::SaveOwnerLease
WorldChunkDeltaStore3D::AcquireSaveOwner(SaveCompletionSink3D sink) {
  if (!core_ || (bool(sink.lifetime) != bool(sink.invoke)))
    return {};
  std::lock_guard lock(core_->mutex);
  if (core_->ownerNamespacesExhausted || !core_->nextOwnerNamespace)
    return {};
  const auto ownerNamespace = core_->nextOwnerNamespace;
  if (ownerNamespace == (std::numeric_limits<std::uint64_t>::max)())
    core_->ownerNamespacesExhausted = true;
  else
    ++core_->nextOwnerNamespace;
  std::shared_ptr<SaveOwnerState> state;
  try {
    state = std::make_shared<SaveOwnerState>(ownerNamespace, std::move(sink));
    core_->owners.emplace(ownerNamespace, state);
  } catch (...) {
    return {};
  }
  SaveOwnerLease lease;
  lease.core_ = core_;
  lease.state_ = std::move(state);
  return lease;
}
bool WorldChunkDeltaStore3D::SetNextSaveOwnerNamespaceForTesting(
    std::uint64_t nextNamespace) {
  if (!core_ || !nextNamespace)
    return false;
  std::lock_guard lock(core_->mutex);
  for (const auto &[unused, owner] : core_->owners)
    if (!owner.expired())
      return false;
  core_->nextOwnerNamespace = nextNamespace;
  core_->ownerNamespacesExhausted = false;
  return true;
}
WorldChunkDeltaStore3D::~WorldChunkDeltaStore3D() {
  {
    std::lock_guard lock(core_->mutex);
    core_->store = nullptr;
  }
  if (worker_.joinable()) {
    worker_.request_stop();
    wake_.notify_all();
    worker_.join();
  }
}
WorldChunkDeltaStore3D::SaveOwnerLease::~SaveOwnerLease() {
  if (core_ && state_) {
    std::lock_guard lock(core_->mutex);
    if (core_->store)
      core_->store->DetachSaveOwner(state_);
    core_->owners.erase(state_->ownerNamespace);
  }
}
WorldChunkDeltaStore3D::SaveOwnerLease::SaveOwnerLease(
    SaveOwnerLease &&other) noexcept
    : core_(std::move(other.core_)), state_(std::move(other.state_)) {}
WorldChunkDeltaStore3D::SaveOwnerLease &
WorldChunkDeltaStore3D::SaveOwnerLease::operator=(SaveOwnerLease &&other) noexcept {
  if (this == &other)
    return *this;
  this->~SaveOwnerLease();
  new (this) SaveOwnerLease(std::move(other));
  return *this;
}
std::uint64_t
WorldChunkDeltaStore3D::SaveOwnerLease::OwnerNamespace() const noexcept {
  return state_ ? state_->ownerNamespace : 0;
}
bool WorldChunkDeltaStore3D::SaveOwnerLease::EnqueueSave(
    std::uint64_t token, std::uint64_t epoch,
    const WorldChunkDeltaSnapshot3D &snapshot) const {
  if (!core_ || !state_)
    return false;
  std::lock_guard lock(core_->mutex);
  return core_->store &&
         core_->store->EnqueueSave(*this, token, epoch, snapshot);
}
bool WorldChunkDeltaStore3D::SaveOwnerLease::TryPopSave(
    WorldDeltaSaveResult3D &result) const {
  if (!core_ || !state_)
    return false;
  std::lock_guard lock(core_->mutex);
  return core_->store && core_->store->TryPopSave(*this, result);
}
void WorldChunkDeltaStore3D::SaveOwnerLease::WaitIdle() const {
  if (!core_ || !state_)
    return;
  std::unique_lock lock(core_->mutex);
  auto *store = core_->store;
  if (!store)
    return;
  store->WaitOwnerIdle(*this);
}
void WorldChunkDeltaStore3D::DetachSaveOwner(
    const std::shared_ptr<SaveOwnerState> &owner) noexcept {
  std::scoped_lock lock(mutex_, owner->mutex);
  owner->attached = false;
  for (const auto bytes : owner->completionBytes) {
    reservedCompletedResults_ = reservedCompletedResults_ ? reservedCompletedResults_ - 1 : 0;
    reservedCompletedBytes_ = reservedCompletedBytes_ >= bytes ? reservedCompletedBytes_ - bytes : 0;
    stats_.completedResults = stats_.completedResults ? stats_.completedResults - 1 : 0;
    stats_.completedBytes = stats_.completedBytes >= bytes ? stats_.completedBytes - bytes : 0;
  }
  owner->completions.clear();
  owner->completionBytes.clear();
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
}
bool WorldChunkDeltaStore3D::EnqueueLoad(std::uint64_t id, std::uint64_t epoch,
                                         WorldDeltaChunkKey3D key,
                                         WorldChunkBaseRevision3D base) {
  if (!id || !epoch || !key.chunkSizeRevision)
    return false;
  std::size_t completionBytes{};
  if (!CheckedAdd(sizeof(WorldDeltaLoadResult3D) + MaximumDiagnosticBytes,
                  settings_.maximumSnapshotBytes, completionBytes))
    return false;
  const std::size_t requestBytes = sizeof(Request);
  std::lock_guard lock(mutex_);
  std::size_t requestUsed{};
  if (!CheckedAdd(stats_.queuedBytes, activeBytes_, requestUsed) ||
      !settings_.enabled ||
      queue_.size() + active_ >= settings_.maximumQueuedRequests ||
      requestBytes > settings_.maximumQueuedBytes -
                         (std::min)(requestUsed,
                                    settings_.maximumQueuedBytes) ||
      reservedCompletedResults_ >= settings_.maximumCompletedResults ||
      completionBytes > settings_.maximumCompletedBytes -
                            (std::min)(reservedCompletedBytes_,
                                       settings_.maximumCompletedBytes)) {
    ++stats_.rejected;
    return false;
  }
  Request request{};
  request.kind = Request::Kind::Load;
  request.requestId = id;
  request.epoch = epoch;
  request.key = key;
  request.base = base;
  request.bytes = requestBytes;
  request.completionBytes = completionBytes;
  queue_.push_back(std::move(request));
  stats_.queuedBytes += requestBytes;
  stats_.peakQueuedBytes =
      (std::max)(stats_.peakQueuedBytes, stats_.queuedBytes + activeBytes_);
  ++reservedCompletedResults_;
  reservedCompletedBytes_ += completionBytes;
  stats_.queuedRequests = queue_.size();
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
  wake_.notify_one();
  return true;
}
bool WorldChunkDeltaStore3D::EnqueueSave(
    std::uint64_t id, std::uint64_t epoch,
    const WorldChunkDeltaSnapshot3D &snapshot) {
  std::vector<std::byte> encoded;
  if (!id || !epoch ||
      !EncodeWorldChunkDeltaSnapshot3D(snapshot, encoded,
                                       settings_.maximumSnapshotBytes)) {
    std::lock_guard lock(mutex_);
    ++stats_.rejected;
    return false;
  }
  std::lock_guard lock(mutex_);
  std::size_t requestBytes{}, completionBytes{}, requestUsed{};
  if (!CheckedAdd(sizeof(Request), encoded.size(), requestBytes) ||
      !CheckedAdd(sizeof(WorldDeltaSaveResult3D), MaximumDiagnosticBytes,
                  completionBytes) ||
      !CheckedAdd(stats_.queuedBytes, activeBytes_, requestUsed) ||
      !settings_.enabled ||
      queue_.size() + active_ >= settings_.maximumQueuedRequests ||
      requestUsed > settings_.maximumQueuedBytes ||
      requestBytes > settings_.maximumQueuedBytes - requestUsed ||
      reservedCompletedResults_ >= settings_.maximumCompletedResults ||
      reservedCompletedBytes_ > settings_.maximumCompletedBytes ||
      completionBytes >
          settings_.maximumCompletedBytes - reservedCompletedBytes_) {
    ++stats_.rejected;
    return false;
  }
  Request request{};
  request.kind = Request::Kind::Save;
  request.requestId = id;
  request.epoch = epoch;
  request.key = snapshot.key;
  request.snapshot = snapshot;
  request.bytes = requestBytes;
  request.completionBytes = completionBytes;
  queue_.push_back(std::move(request));
  stats_.queuedBytes += requestBytes;
  stats_.peakQueuedBytes =
      (std::max)(stats_.peakQueuedBytes, stats_.queuedBytes + activeBytes_);
  ++reservedCompletedResults_;
  reservedCompletedBytes_ += completionBytes;
  stats_.queuedRequests = queue_.size();
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
  wake_.notify_one();
  return true;
}
bool WorldChunkDeltaStore3D::EnqueueSave(
    const SaveOwnerLease &owner, std::uint64_t token, std::uint64_t epoch,
    const WorldChunkDeltaSnapshot3D &snapshot) {
  if (!owner.state_ || owner.core_.get() != core_.get() || !token || !epoch)
    return false;
  bool staleToken{};
  {
    std::lock_guard ownerLock(owner.state_->mutex);
    staleToken = token <= owner.state_->lastAcceptedToken;
  }
  if (staleToken) {
    std::lock_guard lock(mutex_);
    ++stats_.rejected;
    return false;
  }
  std::vector<std::byte> encoded;
  if (!EncodeWorldChunkDeltaSnapshot3D(snapshot, encoded,
                                       settings_.maximumSnapshotBytes)) {
    std::lock_guard lock(mutex_);
    ++stats_.rejected;
    return false;
  }
  Request request{};
  try {
    request.kind = Request::Kind::Save;
    request.requestId = token;
    request.epoch = epoch;
    request.key = snapshot.key;
    request.snapshot = snapshot;
    request.saveOwner = owner.state_;
  } catch (...) {
    std::lock_guard lock(mutex_);
    ++stats_.rejected;
    return false;
  }
  std::scoped_lock lock(mutex_, owner.state_->mutex);
  std::size_t requestBytes{}, completionBytes{}, requestUsed{};
  if (token <= owner.state_->lastAcceptedToken ||
      !CheckedAdd(sizeof(Request), encoded.size(), requestBytes) ||
      !CheckedAdd(sizeof(WorldDeltaSaveResult3D), MaximumDiagnosticBytes,
                  completionBytes) ||
      !CheckedAdd(stats_.queuedBytes, activeBytes_, requestUsed) ||
      !settings_.enabled ||
      queue_.size() + active_ >= settings_.maximumQueuedRequests ||
      requestUsed > settings_.maximumQueuedBytes ||
      requestBytes > settings_.maximumQueuedBytes - requestUsed ||
      reservedCompletedResults_ >= settings_.maximumCompletedResults ||
      reservedCompletedBytes_ > settings_.maximumCompletedBytes ||
      completionBytes > settings_.maximumCompletedBytes - reservedCompletedBytes_) {
    ++stats_.rejected;
    return false;
  }
  request.bytes = requestBytes;
  request.completionBytes = completionBytes;
  try {
    queue_.push_back(std::move(request));
  } catch (...) {
    ++stats_.rejected;
    return false;
  }
  owner.state_->lastAcceptedToken = token;
  ++owner.state_->inFlight;
  stats_.queuedBytes += requestBytes;
  stats_.peakQueuedBytes =
      (std::max)(stats_.peakQueuedBytes, stats_.queuedBytes + activeBytes_);
  ++reservedCompletedResults_;
  reservedCompletedBytes_ += completionBytes;
  stats_.queuedRequests = queue_.size();
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
  wake_.notify_one();
  return true;
}
bool WorldChunkDeltaStore3D::TryPopLoad(WorldDeltaLoadResult3D &r) {
  std::lock_guard lock(mutex_);
  if (loads_.empty())
    return false;
  r = std::move(loads_.front());
  loads_.pop_front();
  const auto bytes = loadCompletionBytes_.front();
  loadCompletionBytes_.pop_front();
  reservedCompletedResults_ = reservedCompletedResults_ ? reservedCompletedResults_ - 1 : 0;
  reservedCompletedBytes_ = reservedCompletedBytes_ >= bytes ? reservedCompletedBytes_ - bytes : 0;
  stats_.completedResults = stats_.completedResults ? stats_.completedResults - 1 : 0;
  stats_.completedBytes = stats_.completedBytes >= bytes ? stats_.completedBytes - bytes : 0;
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
  return true;
}
bool WorldChunkDeltaStore3D::TryPopSave(WorldDeltaSaveResult3D &r) {
  std::lock_guard lock(mutex_);
  if (saves_.empty())
    return false;
  r = std::move(saves_.front());
  saves_.pop_front();
  const auto bytes = saveCompletionBytes_.front();
  saveCompletionBytes_.pop_front();
  reservedCompletedResults_ = reservedCompletedResults_ ? reservedCompletedResults_ - 1 : 0;
  reservedCompletedBytes_ = reservedCompletedBytes_ >= bytes ? reservedCompletedBytes_ - bytes : 0;
  stats_.completedResults = stats_.completedResults ? stats_.completedResults - 1 : 0;
  stats_.completedBytes = stats_.completedBytes >= bytes ? stats_.completedBytes - bytes : 0;
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
  return true;
}
bool WorldChunkDeltaStore3D::TryPopSave(const SaveOwnerLease &owner,
                                        WorldDeltaSaveResult3D &r) {
  if (!owner.state_ || owner.core_.get() != core_.get())
    return false;
  std::scoped_lock lock(mutex_, owner.state_->mutex);
  if (owner.state_->completions.empty())
    return false;
  r = std::move(owner.state_->completions.front());
  owner.state_->completions.pop_front();
  const auto bytes = owner.state_->completionBytes.front();
  owner.state_->completionBytes.pop_front();
  reservedCompletedResults_ = reservedCompletedResults_ ? reservedCompletedResults_ - 1 : 0;
  reservedCompletedBytes_ = reservedCompletedBytes_ >= bytes ? reservedCompletedBytes_ - bytes : 0;
  stats_.completedResults = stats_.completedResults ? stats_.completedResults - 1 : 0;
  stats_.completedBytes = stats_.completedBytes >= bytes ? stats_.completedBytes - bytes : 0;
  stats_.reservedCompletedResults = reservedCompletedResults_;
  stats_.reservedCompletedBytes = reservedCompletedBytes_;
  return true;
}
void WorldChunkDeltaStore3D::WaitOwnerIdle(const SaveOwnerLease &owner) {
  if (!owner.state_ || owner.core_.get() != core_.get())
    return;
  std::unique_lock lock(owner.state_->mutex);
  owner.state_->idle.wait(lock, [&] { return owner.state_->inFlight == 0; });
}
void WorldChunkDeltaStore3D::Worker(std::stop_token stop) {
  for (;;) {
    Request request{};
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, stop, [&] { return !queue_.empty(); });
      if (queue_.empty()) {
        if (stop.stop_requested())
          break;
        continue;
      }
      request = std::move(queue_.front());
      queue_.pop_front();
      stats_.queuedBytes = stats_.queuedBytes >= request.bytes
                               ? stats_.queuedBytes - request.bytes
                               : 0;
      stats_.queuedRequests = queue_.size();
      ++active_;
      activeBytes_ += request.bytes;
      stats_.activeRequests = active_;
      stats_.activeBytes = activeBytes_;
    }
    const auto directory = ChunkDirectory(settings_, request.key),
               snapshotPath = directory / L"snapshot.aewdelta",
               journalPath = directory / L"journal.aewdelta";
    if (request.kind == Request::Kind::Save) {
      WorldDeltaSaveResult3D result{request.requestId, request.epoch,
                                    request.snapshot.durableGeneration,
                                    request.key};
      SaveCompletionSink3D completionSink{};
      std::vector<std::byte> payload;
      try {
        std::filesystem::create_directories(directory);
        if (EncodeWorldChunkDeltaSnapshot3D(request.snapshot, payload,
                                            settings_.maximumSnapshotBytes)) {
          std::array<std::byte, 32> parentDigest{};
          if (std::filesystem::exists(snapshotPath)) {
            std::vector<std::byte> previousBytes;
            WorldChunkDeltaSnapshot3D previous{};
            std::size_t snapshotLimit{};
            if (!CheckedAdd(settings_.maximumSnapshotBytes,
                            FrameHeaderBytes, snapshotLimit) ||
                !ReadBytes(snapshotPath, previousBytes, snapshotLimit) ||
                !ParseFrame(previousBytes, previous,
                            settings_.maximumSnapshotBytes, false, nullptr,
                            &parentDigest)) {
              result.diagnostic = "previous committed snapshot is corrupt";
            }
          }
          if (result.diagnostic.empty()) {
            const auto framed = Frame(payload, parentDigest);
            if (!framed.empty() && AtomicWrite(snapshotPath, framed)) {
              result.durable = true;
              if (!AtomicWrite(journalPath, framed))
                result.diagnostic = "snapshot committed; journal mirror failed";
            } else {
              result.diagnostic = "snapshot atomic commit failed";
            }
          }
        }
      } catch (const std::exception &error) {
        result.diagnostic = error.what();
      }
      {
        std::lock_guard lock(mutex_);
        bool retainedCompletion = true;
        ++stats_.saves;
        if (result.durable)
          ++stats_.durableSaves;
        if (request.saveOwner) {
          std::lock_guard ownerLock(request.saveOwner->mutex);
          completionSink = request.saveOwner->sink;
          if (request.saveOwner->attached) {
            try {
              request.saveOwner->completionBytes.push_back(
                  request.completionBytes);
              try {
                request.saveOwner->completions.push_back(result);
              } catch (...) {
                request.saveOwner->completionBytes.pop_back();
                throw;
              }
            } catch (...) {
              retainedCompletion = false;
            }
          } else {
            retainedCompletion = false;
          }
          if (!retainedCompletion) {
            reservedCompletedResults_ = reservedCompletedResults_ ? reservedCompletedResults_ - 1 : 0;
            reservedCompletedBytes_ = reservedCompletedBytes_ >= request.completionBytes ? reservedCompletedBytes_ - request.completionBytes : 0;
            stats_.reservedCompletedResults = reservedCompletedResults_;
            stats_.reservedCompletedBytes = reservedCompletedBytes_;
          }
        } else {
          try {
            saveCompletionBytes_.push_back(request.completionBytes);
            try {
              saves_.push_back(std::move(result));
            } catch (...) {
              saveCompletionBytes_.pop_back();
              throw;
            }
          } catch (...) {
            retainedCompletion = false;
            reservedCompletedResults_ = reservedCompletedResults_ ? reservedCompletedResults_ - 1 : 0;
            reservedCompletedBytes_ = reservedCompletedBytes_ >= request.completionBytes ? reservedCompletedBytes_ - request.completionBytes : 0;
            stats_.reservedCompletedResults = reservedCompletedResults_;
            stats_.reservedCompletedBytes = reservedCompletedBytes_;
          }
        }
        if (retainedCompletion) {
          ++stats_.completedResults;
          stats_.completedBytes += request.completionBytes;
          stats_.peakCompletedBytes =
              (std::max)(stats_.peakCompletedBytes, stats_.completedBytes);
        }
      }
      if (completionSink.invoke)
        completionSink.invoke(completionSink.lifetime.get(), result);
      if (request.saveOwner) {
        std::lock_guard ownerLock(request.saveOwner->mutex);
        --request.saveOwner->inFlight;
        request.saveOwner->idle.notify_all();
      }
    } else {
      WorldDeltaLoadResult3D result{request.requestId, request.epoch,
                                    request.key};
      bool observedCorruption{}, baseMismatch{}, keyMismatch{};
      try {
        enum class CandidateState { Missing, Valid, Corrupt, IoError };
        struct Candidate {
          CandidateState state{CandidateState::Missing};
          WorldChunkDeltaSnapshot3D snapshot{};
          std::array<std::byte, 32> digest{};
        };
        auto quarantine = [&](const std::filesystem::path &path,
                              std::wstring_view reason) {
          for (unsigned attempt = 0; attempt < 16; ++attempt) {
            const auto destination =
                directory /
                (path.stem().wstring() + L".corrupt." + std::wstring(reason) +
                 L"." + std::to_wstring(GetTickCount64()) + L"." +
                 std::to_wstring(attempt));
            std::error_code moveError;
            std::filesystem::rename(path, destination, moveError);
            if (!moveError)
              return;
            if (!std::filesystem::exists(path))
              return;
          }
        };
        auto readCandidate = [&](const std::filesystem::path &path,
                                 bool allowTail) {
          Candidate candidate;
          std::error_code existsError;
          if (!std::filesystem::exists(path, existsError)) {
            candidate.state = existsError ? CandidateState::IoError
                                          : CandidateState::Missing;
            return candidate;
          }
          std::size_t fileLimit{};
          if (!CheckedAdd(settings_.maximumSnapshotBytes, FrameHeaderBytes,
                          fileLimit)) {
            candidate.state = CandidateState::IoError;
            return candidate;
          }
          std::vector<std::byte> bytes;
          if (!ReadBytes(path, bytes, fileLimit)) {
            candidate.state = CandidateState::Corrupt;
            observedCorruption = true;
            quarantine(path, L"size-or-read");
            return candidate;
          }
          if (bytes.empty()) {
            candidate.state = CandidateState::Missing;
            return candidate;
          }
          std::size_t valid{};
          if (!ParseFrame(bytes, candidate.snapshot,
                          settings_.maximumSnapshotBytes, allowTail, &valid,
                          &candidate.digest)) {
            candidate.state = CandidateState::Corrupt;
            observedCorruption = true;
            quarantine(path, L"frame");
            return candidate;
          }
          if (allowTail && valid < bytes.size()) {
            std::filesystem::resize_file(path, valid);
            std::lock_guard lock(mutex_);
            ++stats_.tornTails;
            stats_.truncatedBytes += bytes.size() - valid;
          }
          if (candidate.snapshot.key != request.key) {
            candidate.state = CandidateState::Corrupt;
            keyMismatch = observedCorruption = true;
            quarantine(path, L"key");
            candidate.snapshot = {};
            return candidate;
          }
          if (!(candidate.snapshot.base == request.base)) {
            candidate.state = CandidateState::Corrupt;
            baseMismatch = observedCorruption = true;
            quarantine(path, L"base");
            candidate.snapshot = {};
            return candidate;
          }
          candidate.state = CandidateState::Valid;
          return candidate;
        };

        Candidate snapshot = readCandidate(snapshotPath, false);
        Candidate journal = readCandidate(journalPath, true);
        if (snapshot.state == CandidateState::Valid &&
            journal.state == CandidateState::Valid &&
            (journal.snapshot.durableGeneration >
                 snapshot.snapshot.durableGeneration ||
             (journal.snapshot.durableGeneration ==
                  snapshot.snapshot.durableGeneration &&
              journal.digest != snapshot.digest))) {
          observedCorruption = true;
          quarantine(snapshotPath, L"commit-disagreement");
          quarantine(journalPath, L"commit-disagreement");
          snapshot.state = journal.state = CandidateState::Corrupt;
        }
        if (keyMismatch || baseMismatch) {
          result.status = baseMismatch ? WorldDeltaLoadStatus3D::BaseMismatch
                                       : WorldDeltaLoadStatus3D::Corrupt;
          result.diagnostic = baseMismatch ? "base identity mismatch"
                                           : "chunk key mismatch";
        } else if (snapshot.state == CandidateState::Valid) {
          result.snapshot = std::move(snapshot.snapshot);
          result.status = WorldDeltaLoadStatus3D::Loaded;
          if (journal.state == CandidateState::Corrupt)
            result.diagnostic = "recovered committed snapshot";
        } else if (journal.state == CandidateState::Valid) {
          result.snapshot = std::move(journal.snapshot);
          result.status = WorldDeltaLoadStatus3D::Loaded;
          if (snapshot.state == CandidateState::Corrupt)
            result.diagnostic = "recovered journal mirror";
        } else if (snapshot.state == CandidateState::IoError ||
                   journal.state == CandidateState::IoError) {
          result.status = WorldDeltaLoadStatus3D::IoError;
          result.diagnostic = "delta file query failed";
        } else if (observedCorruption) {
          result.status = WorldDeltaLoadStatus3D::Corrupt;
          result.diagnostic = "no valid committed delta";
        } else {
          result.status = WorldDeltaLoadStatus3D::Clean;
        }
      } catch (const std::exception &error) {
        result.status = WorldDeltaLoadStatus3D::IoError;
        result.diagnostic = error.what();
      }
      {
        std::lock_guard lock(mutex_);
        ++stats_.loads;
        if (observedCorruption)
          ++stats_.corruptions;
        loads_.push_back(std::move(result));
        loadCompletionBytes_.push_back(request.completionBytes);
        ++stats_.completedResults;
        stats_.completedBytes += request.completionBytes;
        stats_.peakCompletedBytes =
            (std::max)(stats_.peakCompletedBytes, stats_.completedBytes);
      }
    }
    {
      std::lock_guard lock(mutex_);
      --active_;
      activeBytes_ = activeBytes_ >= request.bytes
                         ? activeBytes_ - request.bytes
                         : 0;
      stats_.activeRequests = active_;
      stats_.activeBytes = activeBytes_;
      if (queue_.empty() && !active_)
        idle_.notify_all();
    }
  }
}
void WorldChunkDeltaStore3D::WaitIdle() {
  std::unique_lock lock(mutex_);
  idle_.wait(lock, [&] { return queue_.empty() && !active_; });
}
void WorldChunkDeltaStore3D::Reset() {
  (void)TryReset();
}
bool WorldChunkDeltaStore3D::TryReset() {
  std::unique_lock coreLock(core_->mutex);
  for (const auto &[unused, owner] : core_->owners)
    if (!owner.expired()) {
      std::lock_guard lock(mutex_);
      ++stats_.rejected;
      return false;
    }
  WaitIdle();
  std::lock_guard lock(mutex_);
  queue_.clear();
  loads_.clear();
  saves_.clear();
  loadCompletionBytes_.clear();
  saveCompletionBytes_.clear();
  reservedCompletedResults_ = reservedCompletedBytes_ = 0;
  stats_.queuedBytes = stats_.queuedRequests = 0;
  stats_.completedResults = stats_.completedBytes = 0;
  stats_.reservedCompletedResults = stats_.reservedCompletedBytes = 0;
  if (++resetEpoch_ == 0)
    resetEpoch_ = 1;
  return true;
}
std::size_t WorldChunkDeltaStore3D::TrimUnusedMemory() {
  WaitIdle();
  std::lock_guard lock(mutex_);
  std::deque<Request>(queue_).swap(queue_);
  std::deque<WorldDeltaLoadResult3D>(loads_).swap(loads_);
  std::deque<WorldDeltaSaveResult3D>(saves_).swap(saves_);
  std::deque<std::size_t>(loadCompletionBytes_).swap(loadCompletionBytes_);
  std::deque<std::size_t>(saveCompletionBytes_).swap(saveCompletionBytes_);
  return 0;
}
WorldDeltaStoreStats3D WorldChunkDeltaStore3D::Stats() const {
  std::lock_guard lock(mutex_);
  auto result = stats_;
  result.retainedBytes = stats_.queuedBytes + activeBytes_ +
                         stats_.completedBytes;
  return result;
}
bool WorldChunkDeltaStore3D::RecoverFileForTesting(
    const std::filesystem::path &path, WorldChunkDeltaSnapshot3D &snapshot,
    std::size_t maximum) {
  constexpr std::size_t TestingFrameAllowance = 2;
  if (maximum > ((std::numeric_limits<std::size_t>::max)() -
                 TestingFrameAllowance * FrameHeaderBytes) /
                    TestingFrameAllowance)
    return false;
  const std::size_t readLimit =
      TestingFrameAllowance * (maximum + FrameHeaderBytes);
  std::vector<std::byte> bytes;
  if (!ReadBytes(path, bytes, readLimit))
    return false;
  return ParseFrame(bytes, snapshot, maximum, true);
}
} // namespace Aevrynd
