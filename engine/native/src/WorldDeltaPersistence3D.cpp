#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <system_error>

namespace Aevrynd {
namespace {
constexpr std::array<std::byte, 8> Magic{
    std::byte{'A'}, std::byte{'E'}, std::byte{'W'}, std::byte{'D'},
    std::byte{'L'}, std::byte{'T'}, std::byte{'A'}, std::byte{'1'}};
constexpr std::uint32_t Version = 1;
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
std::vector<std::byte> Frame(std::span<const std::byte> payload) {
  std::vector<std::byte> result;
  result.insert(result.end(), Magic.begin(), Magic.end());
  Put(result, std::uint64_t(payload.size()));
  Put(result, WorldDeltaCrc32C3D(payload));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}
bool ParseFrame(std::span<const std::byte> bytes,
                WorldChunkDeltaSnapshot3D &snapshot, std::size_t maximum,
                bool allowTail, std::size_t *validBytes = nullptr) {
  std::size_t at{};
  WorldChunkDeltaSnapshot3D latest{};
  bool any{};
  while (at < bytes.size()) {
    const std::size_t start = at;
    if (bytes.size() - at < Magic.size() + 12) {
      if (allowTail)
        break;
      return false;
    }
    if (!std::equal(Magic.begin(), Magic.end(),
                    bytes.begin() + std::ptrdiff_t(at)))
      return false;
    at += Magic.size();
    std::uint64_t length{};
    std::uint32_t crc{};
    if (!Get(bytes, at, length) || !Get(bytes, at, crc) || length > maximum)
      return false;
    if (length > bytes.size() - at) {
      if (allowTail) {
        at = start;
        break;
      }
      return false;
    }
    const auto payload = bytes.subspan(at, std::size_t(length));
    if (WorldDeltaCrc32C3D(payload) != crc)
      return false;
    WorldChunkDeltaSnapshot3D decoded{};
    if (!DecodeWorldChunkDeltaSnapshot3D(payload, decoded, maximum))
      return false;
    if (any && decoded.lastSequence < latest.lastSequence)
      return false;
    latest = std::move(decoded);
    any = true;
    at += std::size_t(length);
  }
  if (validBytes)
    *validBytes = at;
  if (!any)
    return bytes.empty();
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
  if (!std::equal(Magic.begin(), Magic.end(), in.begin()))
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
    : settings_(std::move(s)) {
  if (settings_.enabled && !settings_.root.empty() &&
      settings_.maximumQueuedRequests && settings_.maximumQueuedBytes &&
      settings_.maximumSnapshotBytes)
    worker_ = std::jthread([this](std::stop_token stop) { Worker(stop); });
  else
    settings_.enabled = false;
}
WorldChunkDeltaStore3D::~WorldChunkDeltaStore3D() {
  if (worker_.joinable()) {
    worker_.request_stop();
    wake_.notify_all();
    worker_.join();
  }
}
bool WorldChunkDeltaStore3D::EnqueueLoad(std::uint64_t id, std::uint64_t epoch,
                                         WorldDeltaChunkKey3D key,
                                         WorldChunkBaseRevision3D base) {
  if (!id || !epoch || !key.chunkSizeRevision)
    return false;
  std::lock_guard lock(mutex_);
  if (!settings_.enabled || queue_.size() >= settings_.maximumQueuedRequests) {
    ++stats_.rejected;
    return false;
  }
  queue_.push_back({Request::Kind::Load, id, epoch, key, base, {}, {}});
  stats_.queuedRequests = queue_.size();
  wake_.notify_one();
  return true;
}
bool WorldChunkDeltaStore3D::EnqueueSave(
    std::uint64_t id, std::uint64_t epoch,
    const WorldChunkDeltaSnapshot3D &snapshot) {
  std::vector<std::byte> encoded;
  if (!id || !epoch ||
      !EncodeWorldChunkDeltaSnapshot3D(snapshot, encoded,
                                       settings_.maximumSnapshotBytes))
    return false;
  std::lock_guard lock(mutex_);
  if (!settings_.enabled || queue_.size() >= settings_.maximumQueuedRequests ||
      encoded.size() > settings_.maximumQueuedBytes - stats_.queuedBytes) {
    ++stats_.rejected;
    return false;
  }
  Request request{};
  request.kind = Request::Kind::Save;
  request.requestId = id;
  request.epoch = epoch;
  request.key = snapshot.key;
  request.snapshot = snapshot;
  request.bytes = encoded.size();
  queue_.push_back(std::move(request));
  stats_.queuedBytes += encoded.size();
  stats_.peakQueuedBytes =
      (std::max)(stats_.peakQueuedBytes, stats_.queuedBytes);
  stats_.queuedRequests = queue_.size();
  wake_.notify_one();
  return true;
}
bool WorldChunkDeltaStore3D::TryPopLoad(WorldDeltaLoadResult3D &r) {
  std::lock_guard lock(mutex_);
  if (loads_.empty())
    return false;
  r = std::move(loads_.front());
  loads_.pop_front();
  return true;
}
bool WorldChunkDeltaStore3D::TryPopSave(WorldDeltaSaveResult3D &r) {
  std::lock_guard lock(mutex_);
  if (saves_.empty())
    return false;
  r = std::move(saves_.front());
  saves_.pop_front();
  return true;
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
    }
    const auto directory = ChunkDirectory(settings_, request.key),
               snapshotPath = directory / L"snapshot.aewdelta",
               journalPath = directory / L"journal.aewdelta";
    if (request.kind == Request::Kind::Save) {
      WorldDeltaSaveResult3D result{request.requestId, request.epoch,
                                    request.snapshot.durableGeneration,
                                    request.key};
      std::vector<std::byte> payload;
      try {
        std::filesystem::create_directories(directory);
        if (EncodeWorldChunkDeltaSnapshot3D(request.snapshot, payload,
                                            settings_.maximumSnapshotBytes)) {
          const auto framed = Frame(payload);
          HANDLE journal = CreateFileW(journalPath.c_str(), FILE_APPEND_DATA,
                                       FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
          if (journal != INVALID_HANDLE_VALUE) {
            const bool appended =
                WriteAll(journal, framed) && FlushFileBuffers(journal);
            CloseHandle(journal);
            result.durable = appended && AtomicWrite(snapshotPath, framed);
            if (!result.durable)
              result.diagnostic = "durable write failed";
          } else
            result.diagnostic = "journal open failed";
        }
      } catch (const std::exception &error) {
        result.diagnostic = error.what();
      }
      {
        std::lock_guard lock(mutex_);
        ++stats_.saves;
        if (result.durable)
          ++stats_.durableSaves;
        saves_.push_back(std::move(result));
      }
    } else {
      WorldDeltaLoadResult3D result{request.requestId, request.epoch,
                                    request.key};
      std::vector<std::byte> bytes;
      try {
        if (!std::filesystem::exists(snapshotPath) &&
            !std::filesystem::exists(journalPath))
          result.status = WorldDeltaLoadStatus3D::Clean;
        else if (ReadBytes(journalPath, bytes,
                           settings_.maximumSnapshotBytes * 4)) {
          std::size_t valid{};
          if (ParseFrame(bytes, result.snapshot, settings_.maximumSnapshotBytes,
                         true, &valid)) {
            if (valid < bytes.size()) {
              std::filesystem::resize_file(journalPath, valid);
              std::lock_guard lock(mutex_);
              ++stats_.tornTails;
              stats_.truncatedBytes += bytes.size() - valid;
            }
            if (result.snapshot.base == request.base) {
              result.status = WorldDeltaLoadStatus3D::Loaded;
            } else {
              result.status = WorldDeltaLoadStatus3D::BaseMismatch;
              result.snapshot = {};
            }
          } else {
            result.status = WorldDeltaLoadStatus3D::Corrupt;
            result.diagnostic = "journal corruption";
            const auto quarantine =
                directory /
                (L"journal.corrupt." + std::to_wstring(GetTickCount64()));
            std::error_code ignored;
            std::filesystem::rename(journalPath, quarantine, ignored);
            // journal 中段損毀時保留證據，並只回復已原子發布的最後 snapshot。
            std::vector<std::byte> snapshotBytes;
            WorldChunkDeltaSnapshot3D fallback{};
            if (ReadBytes(snapshotPath, snapshotBytes,
                          settings_.maximumSnapshotBytes + 20) &&
                ParseFrame(snapshotBytes, fallback,
                           settings_.maximumSnapshotBytes, false) &&
                fallback.base == request.base) {
              result.snapshot = std::move(fallback);
              result.status = WorldDeltaLoadStatus3D::Loaded;
              result.diagnostic = "recovered atomic snapshot after journal corruption";
            }
          }
        } else
          result.status = WorldDeltaLoadStatus3D::IoError;
      } catch (const std::exception &error) {
        result.status = WorldDeltaLoadStatus3D::IoError;
        result.diagnostic = error.what();
      }
      {
        std::lock_guard lock(mutex_);
        ++stats_.loads;
        if (result.status == WorldDeltaLoadStatus3D::Corrupt ||
            result.diagnostic.find("journal corruption") != std::string::npos)
          ++stats_.corruptions;
        loads_.push_back(std::move(result));
      }
    }
    {
      std::lock_guard lock(mutex_);
      --active_;
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
  WaitIdle();
  std::lock_guard lock(mutex_);
  queue_.clear();
  loads_.clear();
  saves_.clear();
  stats_.queuedBytes = stats_.queuedRequests = 0;
  if (++resetEpoch_ == 0)
    resetEpoch_ = 1;
}
std::size_t WorldChunkDeltaStore3D::TrimUnusedMemory() {
  WaitIdle();
  std::lock_guard lock(mutex_);
  const auto before = queue_.size() * sizeof(Request) +
                      loads_.size() * sizeof(WorldDeltaLoadResult3D) +
                      saves_.size() * sizeof(WorldDeltaSaveResult3D);
  std::deque<Request>(queue_).swap(queue_);
  std::deque<WorldDeltaLoadResult3D>(loads_).swap(loads_);
  std::deque<WorldDeltaSaveResult3D>(saves_).swap(saves_);
  stats_.trimmedBytes += before;
  return before;
}
WorldDeltaStoreStats3D WorldChunkDeltaStore3D::Stats() const {
  std::lock_guard lock(mutex_);
  auto result = stats_;
  result.retainedBytes = queue_.size() * sizeof(Request) +
                         loads_.size() * sizeof(WorldDeltaLoadResult3D) +
                         saves_.size() * sizeof(WorldDeltaSaveResult3D);
  return result;
}
bool WorldChunkDeltaStore3D::RecoverFileForTesting(
    const std::filesystem::path &path, WorldChunkDeltaSnapshot3D &snapshot,
    std::size_t maximum) {
  std::vector<std::byte> bytes;
  if (!ReadBytes(path, bytes, maximum * 4))
    return false;
  return ParseFrame(bytes, snapshot, maximum, true);
}
} // namespace Aevrynd
