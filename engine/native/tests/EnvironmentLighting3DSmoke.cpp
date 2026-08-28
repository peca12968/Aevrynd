#include "Aevrynd/EnvironmentIbl3D.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>
using namespace Aevrynd;
namespace {std::atomic<long long> failAllocation{-1};std::atomic<std::uint64_t> allocationCount{};}
void *operator new(std::size_t bytes){++allocationCount;auto value=failAllocation.load();if(value>=0){if(value==0)throw std::bad_alloc{};failAllocation.store(value-1);}if(void* memory=std::malloc(bytes?bytes:1))return memory;throw std::bad_alloc{};}
void operator delete(void* memory)noexcept{std::free(memory);}void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}
static void Put(std::vector<std::byte> &b, size_t o, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[o + i] = std::byte(v >> (i * 8));
}
static bool Dds(const std::filesystem::path &p, uint32_t w, uint32_t h,
                uint32_t format, bool cube, uint32_t mips) {
  size_t payload{};
  for (uint32_t f = 0; f < (cube ? 6u : 1u); ++f)
    for (uint32_t m = 0; m < mips; ++m) {
      auto x = (std::max)(1u, w >> m), y = (std::max)(1u, h >> m);
      payload +=
          cube ? size_t((x + 3) / 4) * ((y + 3) / 4) * 16 : size_t(x) * y * 4;
    }
  std::vector<std::byte> b(148 + payload);
  Put(b, 0, 0x20534444);
  Put(b, 4, 124);
  Put(b, 12, h);
  Put(b, 16, w);
  Put(b, 28, mips);
  Put(b, 76, 32);
  Put(b, 80, 4);
  Put(b, 84, 0x30315844);
  Put(b, 128, format);
  Put(b, 132, 3);
  Put(b, 136, cube ? 4 : 0);
  Put(b, 140, 1);
  std::ofstream out(p, std::ios::binary);
  out.write(reinterpret_cast<const char *>(b.data()),
            std::streamsize(b.size()));
  return bool(out);
}
static void Patch32(const std::filesystem::path &p, size_t offset, uint32_t value) {
  std::fstream file(p, std::ios::binary | std::ios::in | std::ios::out);
  std::array<char, 4> bytes{};
  for (unsigned i = 0; i < 4; ++i)
    bytes[i] = char(value >> (i * 8));
  file.seekp(std::streamoff(offset));
  file.write(bytes.data(), bytes.size());
}
static bool CreateDirectoryAlias(const std::filesystem::path &link,
                                 const std::filesystem::path &target) {
  if (CreateSymbolicLinkW(link.c_str(), target.c_str(),
                          SYMBOLIC_LINK_FLAG_DIRECTORY | 0x2))
    return true;
  const std::wstring command = L"cmd.exe /d /c mklink /J \"" + link.native() +
                               L"\" \"" + target.native() + L"\" >nul";
  return _wsystem(command.c_str()) == 0;
}
static bool RetargetDirectoryAlias(const std::filesystem::path &link,
                                   const std::filesystem::path &target) {
  if (!RemoveDirectoryW(link.c_str()) &&
      GetLastError() != ERROR_FILE_NOT_FOUND &&
      GetLastError() != ERROR_PATH_NOT_FOUND)
    return false;
  return CreateDirectoryAlias(link, target);
}
static bool PatchPayloadByte(const std::filesystem::path &path, char value) {
  std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
  file.seekp(148);
  file.write(&value, 1);
  return bool(file);
}
static bool AtomicReplacePayloadByte(const std::filesystem::path &path,
                                     char value) {
  const auto replacement = path.parent_path() /
                           (path.filename().native() + L".replacement");
  std::error_code error;
  std::filesystem::remove(replacement, error);
  error.clear();
  std::filesystem::copy_file(path, replacement,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error || !PatchPayloadByte(replacement, value))
    return false;
  return MoveFileExW(replacement.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
class ScopedTempRoot {
public:
  explicit ScopedTempRoot(std::wstring_view prefix) : prefix_(prefix) {
    std::error_code error;
    base_ = std::filesystem::weakly_canonical(
        std::filesystem::temp_directory_path(), error);
    if (error)
      return;
    static std::atomic<std::uint64_t> nonceCounter{};
    const auto nonce =
        std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()) ^
        (++nonceCounter * 0x9e3779b97f4a7c15ull);
    path_ = base_ / (prefix_ + L"-pid-" +
                     std::to_wstring(GetCurrentProcessId()) + L"-nonce-" +
                     std::to_wstring(nonce));
    std::filesystem::create_directories(path_, error);
    valid_ = !error;
  }
  ~ScopedTempRoot() { Cleanup(); }
  ScopedTempRoot(const ScopedTempRoot &) = delete;
  ScopedTempRoot &operator=(const ScopedTempRoot &) = delete;
  [[nodiscard]] const std::filesystem::path &Path() const noexcept {
    return path_;
  }
  [[nodiscard]] bool Valid() const noexcept { return valid_; }
  bool Cleanup() noexcept {
    if (!valid_)
      return path_.empty();
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path_, error);
    if (error || canonical.parent_path() != base_ ||
        canonical.filename().native().rfind(prefix_, 0) != 0)
      return false;
    std::filesystem::remove_all(canonical, error);
    if (error)
      return false;
    valid_ = false;
    return true;
  }

private:
  std::filesystem::path base_, path_;
  std::wstring prefix_;
  bool valid_{};
};
int main() {
  ScopedTempRoot tempRoot(L"aevrynd-ibl-smoke");
  if (!tempRoot.Valid())
    return 92;
  const auto &root = tempRoot.Path();
  auto s = root / "s.dds", i = root / "i.dds", b = root / "b.dds";
  if (!Dds(s, 16, 16, 95, true, 5) || !Dds(i, 8, 8, 95, true, 4) ||
      !Dds(b, 16, 16, 34, false, 1))
    return 1;
  EnvironmentIblDefinition3D d{s, i, b, .25f};
  EnvironmentIblValidatedData3D parsed{};
  if (!LoadAndValidateEnvironmentIbl3D(d, parsed))
    return 29;
  const auto invalid = [&](const std::filesystem::path &path) {
    auto candidate = d;
    candidate.specularCubePath = path;
    return !LoadAndValidateEnvironmentIbl3D(candidate, parsed);
  };
  auto shortMip = root / "short.dds", longMip = root / "long.dds",
       irradianceShortMip = root / "irradiance-short-mip.dds",
       irradianceLongMip = root / "irradiance-long-mip.dds",
       nonsquare = root / "nonsquare.dds", wrongArray = root / "array.dds",
       wrongCaps = root / "caps.dds", trailing = root / "trailing.dds",
       truncated = root / "truncated.dds", oneExtra = root / "one-extra.dds";
  Dds(shortMip, 16, 16, 95, true, 4);Dds(longMip, 16, 16, 95, true, 6);
  Dds(irradianceShortMip, 8, 8, 95, true, 3);
  Dds(irradianceLongMip, 8, 8, 95, true, 5);
  Dds(nonsquare, 16, 8, 95, true, 5);std::filesystem::copy_file(s, wrongArray);Patch32(wrongArray, 140, 2);
  std::filesystem::copy_file(s, wrongCaps);Patch32(wrongCaps, 136, 0);
  std::filesystem::copy_file(s, trailing);{std::ofstream out(trailing,std::ios::binary|std::ios::app);out.put(0);}
  std::filesystem::copy_file(s, truncated);std::filesystem::resize_file(truncated,std::filesystem::file_size(truncated)-1);
  Dds(oneExtra,1,1,95,true,13);
  if (!invalid(shortMip)||!invalid(longMip)||!invalid(nonsquare)||!invalid(wrongArray)||!invalid(wrongCaps)||!invalid(trailing)||!invalid(truncated)||!invalid(oneExtra))return 13;
  const auto invalidIrradianceMip = [&](const std::filesystem::path &path) {
    auto candidate = d;
    candidate.irradianceCubePath = path;
    return !LoadAndValidateEnvironmentIbl3D(candidate, parsed);
  };
  if (!invalidIrradianceMip(irradianceShortMip) ||
      !invalidIrradianceMip(irradianceLongMip))
    return 72;
  EnvironmentLightingStore3D store;
  if (store.MemoryStats().residentSets)
    return 2;
  auto a = store.Create(d), same = store.Create(d);
  if (!a || same != a || store.MemoryStats().residentReferences != 2 ||
      store.MemoryStats().cacheHits != 1 || store.MemoryStats().fastHits != 1 ||
      store.MemoryStats().fileReads != 3)
    return 3;
  auto aliasDefinition = d;
  aliasDefinition.specularCubePath =
      std::filesystem::path(root.native() + L"\\.\\s.dds");
  aliasDefinition.irradianceCubePath =
      std::filesystem::path(root.native() + L"\\.\\i.dds");
  aliasDefinition.brdfLutPath =
      std::filesystem::path(root.native() + L"\\.\\b.dds");
  const auto beforeAlias = store.MemoryStats();
  const auto alias = store.Create(aliasDefinition);
  const auto aliasAgain = store.Create(aliasDefinition);
  const auto afterAlias = store.MemoryStats();
  if (alias != a || aliasAgain != a ||
      afterAlias.fileReads != beforeAlias.fileReads ||
      afterAlias.fastHits != beforeAlias.fastHits + 1 ||
      afterAlias.cacheHits != beforeAlias.cacheHits + 2)
    return 43;
  if (!store.Set(a))
    return 4;
  store.Destroy(a);
  store.Destroy(same);
  store.Destroy(alias);
  store.Destroy(aliasAgain);
  if (store.Set(a) || store.MemoryStats().residentSets)
    return 5;
  auto bad = d;
  bad.specularCubePath = root / "missing";
  if (store.Create(bad))
    return 6;
  auto fresh = store.Create(d);
  if (!fresh || fresh == a)
    return 7;
  EnvironmentLightingStore3D moved = std::move(store);
  if (!moved.Set(fresh))
    return 8;
  moved.Destroy(fresh);
  if (!moved.MemoryStats().reclaimableBytes)
    return 9;
  moved.Trim();
  if (moved.MemoryStats().reclaimableBytes ||
      moved.MemoryStats().trimCount != 1)
    return 10;
  moved.Shutdown();
  EnvironmentLightingStore3D rotations;
  auto zeroDefinition = d;
  zeroDefinition.rotationRadians = -0.f;
  const auto zero = rotations.Create(zeroDefinition);
  zeroDefinition.rotationRadians = 0.f;
  const auto positiveZero = rotations.Create(zeroDefinition);
  zeroDefinition.rotationRadians = 6.2831853071795864769f;
  const auto fullTurn = rotations.Create(zeroDefinition);
  zeroDefinition.rotationRadians = 1.f;
  const auto distinct = rotations.Create(zeroDefinition);
  if (!zero || positiveZero != zero || fullTurn != zero || !distinct ||
      distinct == zero || rotations.MemoryStats().residentSets != 2 ||
      rotations.MemoryStats().residentReferences != 4 ||
      rotations.MemoryStats().cacheHits != 2)
    return 11;
  rotations.Destroy(zero);
  rotations.Destroy(positiveZero);
  rotations.Destroy(fullTurn);
  rotations.Destroy(distinct);
  if (rotations.MemoryStats().residentSets ||
      rotations.MemoryStats().residentReferences)
    return 12;
  EnvironmentLightingStore3D rotationStress;
  std::vector<EnvironmentLightingHandle3D> rotationHandles;
  rotationHandles.reserve(64);
  for (std::uint32_t index = 0; index < 64; ++index) {
    auto rotationDefinition = d;
    rotationDefinition.rotationRadians = float(index) * .05f;
    const auto handle = rotationStress.Create(rotationDefinition);
    if (!handle || std::find(rotationHandles.begin(), rotationHandles.end(),
                             handle) != rotationHandles.end())
      return 25;
    rotationHandles.push_back(handle);
  }
  const auto rotationStats = rotationStress.MemoryStats();
  if (rotationStats.residentInstances != 64 ||
      rotationStats.residentReferences != 64 || rotationStats.payloadSets != 1 ||
      rotationStats.payloadBytes != parsed.payloadBytes ||
      rotationStats.sourceSets != 1 ||
      rotationStats.instanceCacheEntries != 64 ||
      rotationStats.payloadCacheEntries != 1 ||
      rotationStats.rawSourceEntries != 1 ||
      rotationStats.canonicalSourceEntries != 1 ||
      !rotationStats.metadataBytes ||
      rotationStats.residentSets != rotationStats.residentInstances ||
      rotationStats.residentBytes != rotationStats.payloadBytes)
    return 26;
  rotationStress.Trim();
  if (rotationStress.MemoryStats().trimCount != 0)
    return 30;
  rotationStress.Destroy(rotationHandles.front());
  const auto partialRotationStats = rotationStress.MemoryStats();
  if (partialRotationStats.residentInstances != 63 ||
      partialRotationStats.payloadSets != 1 ||
      partialRotationStats.payloadBytes != parsed.payloadBytes)
    return 31;
  for (std::size_t index = 1; index < rotationHandles.size(); ++index)
    rotationStress.Destroy(rotationHandles[index]);
  const auto releasedRotationStats = rotationStress.MemoryStats();
  if (releasedRotationStats.residentInstances ||
      releasedRotationStats.residentReferences ||
      releasedRotationStats.payloadSets || releasedRotationStats.payloadBytes ||
      releasedRotationStats.sourceSets ||
      releasedRotationStats.instanceCacheEntries ||
      releasedRotationStats.payloadCacheEntries ||
      releasedRotationStats.rawSourceEntries ||
      releasedRotationStats.canonicalSourceEntries ||
      releasedRotationStats.reclaimableBytes !=
          releasedRotationStats.metadataBytes)
    return 27;
  rotationStress.Trim();
  const auto trimmedRotationStats = rotationStress.MemoryStats();
  if (trimmedRotationStats.reclaimableBytes ||
      trimmedRotationStats.metadataBytes || trimmedRotationStats.trimCount != 1)
    return 28;
  EnvironmentLightingStore3D shutdownStore;
  auto shutdownDefinition = d;
  const auto shutdownFirst = shutdownStore.Create(shutdownDefinition);
  shutdownDefinition.rotationRadians += 1.f;
  const auto shutdownSecond = shutdownStore.Create(shutdownDefinition);
  if (!shutdownFirst || !shutdownSecond ||
      shutdownStore.MemoryStats().payloadSets != 1)
    return 32;
  shutdownStore.Shutdown();
  const auto shutdownStats = shutdownStore.MemoryStats();
  if (shutdownStore.Set(shutdownFirst) || shutdownStats.residentInstances ||
      shutdownStats.residentReferences || shutdownStats.payloadSets ||
      shutdownStats.payloadBytes)
    return 33;

  const auto sameStats = [](const EnvironmentLightingMemoryStats3D &left,
                            const EnvironmentLightingMemoryStats3D &right) {
    return left.residentInstances == right.residentInstances &&
           left.residentReferences == right.residentReferences &&
           left.payloadSets == right.payloadSets &&
           left.payloadBytes == right.payloadBytes &&
           left.sourceSets == right.sourceSets &&
           left.instanceCacheEntries == right.instanceCacheEntries &&
           left.payloadCacheEntries == right.payloadCacheEntries &&
           left.rawSourceEntries == right.rawSourceEntries &&
           left.canonicalSourceEntries == right.canonicalSourceEntries &&
           left.retiredSlots == right.retiredSlots &&
           left.reclaimableBytes == right.reclaimableBytes &&
           left.residentSets == right.residentSets &&
           left.residentBytes == right.residentBytes &&
           left.cacheHits == right.cacheHits && left.fastHits == right.fastHits &&
           left.fileReads == right.fileReads &&
           left.refreshes == right.refreshes && left.creates == right.creates &&
           left.destroys == right.destroys &&
           left.trimCount == right.trimCount;
  };
  EnvironmentLightingStore3D atomicStore;
  auto baselineDefinition=d;baselineDefinition.rotationRadians=.5f;
  const auto baseline=atomicStore.Create(baselineDefinition);if(!baseline||!atomicStore.Set(baseline))return 14;
  const auto baselineStats=atomicStore.MemoryStats();bool reachedSuccess=false;
  for(long long checkpoint=0;checkpoint<256;++checkpoint){auto candidateDefinition=d;candidateDefinition.rotationRadians=1.5f;failAllocation.store(checkpoint);EnvironmentLightingHandle3D created{};try{created=atomicStore.Create(candidateDefinition);}catch(...){failAllocation.store(-1);return 15;}failAllocation.store(-1);if(created){atomicStore.Destroy(created);reachedSuccess=true;break;}const auto state=atomicStore.MemoryStats();if(atomicStore.Active()!=baseline||state.residentInstances!=baselineStats.residentInstances||state.residentSets!=baselineStats.residentSets||state.residentReferences!=baselineStats.residentReferences||state.payloadSets!=baselineStats.payloadSets||state.payloadBytes!=baselineStats.payloadBytes||state.residentBytes!=baselineStats.residentBytes||state.cacheHits!=baselineStats.cacheHits||state.creates!=baselineStats.creates||!atomicStore.Set(baseline))return 16;}
  if(!reachedSuccess)return 17;atomicStore.Destroy(baseline);if(atomicStore.MemoryStats().residentSets)return 18;

  const auto changedSpecular = root / "s-atomic-content.dds";
  std::filesystem::copy_file(s, changedSpecular);
  {
    std::fstream file(changedSpecular,
                      std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(148);
    const char value = 1;
    file.write(&value, 1);
  }
  auto changedDefinition = d;
  changedDefinition.specularCubePath = changedSpecular;
  std::uint64_t newPayloadAllocations{};
  {
    EnvironmentLightingStore3D probe;
    const auto probeBaseline = probe.Create(d);
    if (!probeBaseline || !probe.Set(probeBaseline))
      return 34;
    const auto allocationStart = allocationCount.load();
    const auto probeChanged = probe.Create(changedDefinition);
    newPayloadAllocations = allocationCount.load() - allocationStart;
    if (!probeChanged || !newPayloadAllocations ||
        probe.MemoryStats().residentInstances != 2 ||
        probe.MemoryStats().payloadSets != 2)
      return 35;
  }
  for (std::uint64_t checkpoint = 0; checkpoint < newPayloadAllocations;
       ++checkpoint) {
    EnvironmentLightingStore3D sweep;
    const auto sweepBaseline = sweep.Create(d);
    if (!sweepBaseline || !sweep.Set(sweepBaseline))
      return 36;
    const auto before = sweep.MemoryStats();
    failAllocation.store(static_cast<long long>(checkpoint));
    EnvironmentLightingHandle3D failed{};
    try {
      failed = sweep.Create(changedDefinition);
    } catch (...) {
      failAllocation.store(-1);
      return 37;
    }
    failAllocation.store(-1);
    if (failed || sweep.Active() != sweepBaseline ||
        !sameStats(sweep.MemoryStats(), before) || !sweep.Set(sweepBaseline))
      return 38;

    // 失敗後同一內容必須可建立，且第二次取得須命中同一 instance。
    const auto recovered = sweep.Create(changedDefinition);
    const auto retained = sweep.Create(changedDefinition);
    const auto recoveredStats = sweep.MemoryStats();
    if (!recovered || retained != recovered ||
        recoveredStats.residentInstances != 2 ||
        recoveredStats.residentReferences != 3 ||
        recoveredStats.payloadSets != 2 ||
        recoveredStats.payloadBytes != before.payloadBytes * 2 ||
        recoveredStats.cacheHits != before.cacheHits + 1 ||
        !sweep.Set(sweepBaseline))
      return 39;
    sweep.Destroy(retained);
    sweep.Destroy(recovered);
    if (sweep.MemoryStats().residentInstances != 1 ||
        sweep.MemoryStats().residentReferences != 1 ||
        sweep.MemoryStats().payloadSets != 1 ||
        sweep.MemoryStats().payloadBytes != before.payloadBytes ||
        !sweep.Set(sweepBaseline))
      return 40;
    sweep.Destroy(sweepBaseline);
  }
  {
    // 成功路徑 allocation 數之後的 checkpoint 不得誤觸發。
    EnvironmentLightingStore3D boundary;
    const auto boundaryBaseline = boundary.Create(d);
    if (!boundaryBaseline)
      return 41;
    failAllocation.store(static_cast<long long>(newPayloadAllocations));
    const auto boundaryChanged = boundary.Create(changedDefinition);
    failAllocation.store(-1);
    if (!boundaryChanged || boundary.MemoryStats().residentInstances != 2 ||
        boundary.MemoryStats().payloadSets != 2)
      return 42;
  }
  const auto refreshSpecular = root / "s-refresh-atomic.dds";
  std::filesystem::copy_file(s, refreshSpecular);
  auto refreshDefinition = d;
  refreshDefinition.specularCubePath = refreshSpecular;
  std::uint64_t refreshAllocations{};
  {
    EnvironmentLightingStore3D probe;
    if (!PatchPayloadByte(refreshSpecular, 0))
      return 45;
    const auto old = probe.Create(refreshDefinition);
    if (!old || !probe.Set(old) || !PatchPayloadByte(refreshSpecular, 1))
      return 46;
    const auto allocationStart = allocationCount.load();
    const auto refreshed = probe.Refresh(refreshDefinition);
    refreshAllocations = allocationCount.load() - allocationStart;
    if (!refreshed || refreshed == old || !refreshAllocations ||
        probe.MemoryStats().payloadSets != 2)
      return 47;
  }
  for (std::uint64_t checkpoint = 0; checkpoint < refreshAllocations;
       ++checkpoint) {
    EnvironmentLightingStore3D sweep;
    if (!PatchPayloadByte(refreshSpecular, 0))
      return 48;
    const auto old = sweep.Create(refreshDefinition);
    if (!old || !sweep.Set(old) || !PatchPayloadByte(refreshSpecular, 1))
      return 49;
    const auto before = sweep.MemoryStats();
    failAllocation.store(static_cast<long long>(checkpoint));
    EnvironmentLightingHandle3D failed{};
    try {
      failed = sweep.Refresh(refreshDefinition);
    } catch (...) {
      failAllocation.store(-1);
      return 50;
    }
    failAllocation.store(-1);
    if (failed || sweep.Active() != old ||
        !sameStats(sweep.MemoryStats(), before) || !sweep.Set(old))
      return 51;
    const auto cachedOld = sweep.Create(refreshDefinition);
    if (cachedOld != old || sweep.MemoryStats().payloadSets != 1)
      return 52;
    sweep.Destroy(cachedOld);
    const auto recovered = sweep.Refresh(refreshDefinition);
    if (!recovered || recovered == old ||
        sweep.MemoryStats().residentInstances != 2 ||
        sweep.MemoryStats().payloadSets != 2 || !sweep.Set(old))
      return 53;
    sweep.Destroy(recovered);
    sweep.Destroy(old);
  }

  const auto targetA = root / "alias-target-a";
  const auto targetB = root / "alias-target-b";
  std::filesystem::create_directories(targetA);
  std::filesystem::create_directories(targetB);
  const auto writeTarget = [](const std::filesystem::path &target) {
    return Dds(target / "s.dds", 16, 16, 95, true, 5) &&
           Dds(target / "i.dds", 8, 8, 95, true, 4) &&
           Dds(target / "b.dds", 16, 16, 34, false, 1);
  };
  if (!writeTarget(targetA) || !writeTarget(targetB) ||
      !PatchPayloadByte(targetB / "s.dds", 1))
    return 54;
  const EnvironmentIblDefinition3D absoluteA{
      targetA / "s.dds", targetA / "i.dds", targetA / "b.dds", .75f};
  const EnvironmentIblDefinition3D absoluteB{
      targetB / "s.dds", targetB / "i.dds", targetB / "b.dds", .75f};

  // 相同 relative raw tuple 在 CWD 改變前沿用舊 cache，Refresh 後才重綁。
  const auto originalDirectory = std::filesystem::current_path();
  const bool relativePassed = [&] {
    EnvironmentLightingStore3D relativeStore;
    const EnvironmentIblDefinition3D relative{
        "s.dds", "i.dds", "b.dds", .75f};
    std::filesystem::current_path(targetA);
    const auto old = relativeStore.Create(relative);
    std::filesystem::current_path(targetB);
    const auto cachedOld = relativeStore.Create(relative);
    const auto refreshed = relativeStore.Refresh(relative);
    const auto cachedNew = relativeStore.Create(relative);
    const auto absoluteOld = relativeStore.Create(absoluteA);
    const auto absoluteNew = relativeStore.Create(absoluteB);
    const bool passed = old && cachedOld == old && refreshed &&
                        refreshed != old && cachedNew == refreshed &&
                        absoluteOld == old && absoluteNew == refreshed &&
                        relativeStore.MemoryStats().payloadSets == 2;
    relativeStore.Destroy(cachedNew);
    relativeStore.Destroy(old);
    relativeStore.Destroy(absoluteNew);
    relativeStore.Destroy(cachedOld);
    relativeStore.Destroy(refreshed);
    relativeStore.Destroy(absoluteOld);
    return passed && !relativeStore.MemoryStats().residentInstances &&
           !relativeStore.MemoryStats().payloadSets;
  }();
  std::filesystem::current_path(originalDirectory);
  if (!relativePassed)
    return 55;

  const auto directoryAlias = root / "source-alias";
  if (!CreateDirectoryAlias(directoryAlias, targetA))
    return 56;
  const EnvironmentIblDefinition3D reparseDefinition{
      directoryAlias / "s.dds", directoryAlias / "i.dds",
      directoryAlias / "b.dds", .75f};
  {
    EnvironmentLightingStore3D reparseStore;
    const auto old = reparseStore.Create(reparseDefinition);
    if (!old || !RetargetDirectoryAlias(directoryAlias, targetB))
      return 57;
    const auto cachedOld = reparseStore.Create(reparseDefinition);
    const auto refreshed = reparseStore.Refresh(reparseDefinition);
    const auto cachedNew = reparseStore.Create(reparseDefinition);
    const auto absoluteOld = reparseStore.Create(absoluteA);
    const auto absoluteNew = reparseStore.Create(absoluteB);
    if (cachedOld != old || !refreshed || refreshed == old ||
        cachedNew != refreshed || absoluteOld != old ||
        absoluteNew != refreshed ||
        reparseStore.MemoryStats().payloadSets != 2)
      return 58;
    reparseStore.Destroy(cachedNew);
    reparseStore.Destroy(old);
    reparseStore.Destroy(absoluteNew);
    reparseStore.Destroy(cachedOld);
    reparseStore.Destroy(refreshed);
    reparseStore.Destroy(absoluteOld);
    if (reparseStore.MemoryStats().residentInstances ||
        reparseStore.MemoryStats().payloadSets)
      return 59;
  }

  std::uint64_t rebindAllocations{};
  {
    if (!RetargetDirectoryAlias(directoryAlias, targetA))
      return 60;
    EnvironmentLightingStore3D probe;
    const auto old = probe.Create(reparseDefinition);
    if (!old || !RetargetDirectoryAlias(directoryAlias, targetB))
      return 61;
    const auto allocationStart = allocationCount.load();
    const auto refreshed = probe.Refresh(reparseDefinition);
    rebindAllocations = allocationCount.load() - allocationStart;
    if (!refreshed || refreshed == old || !rebindAllocations)
      return 62;
  }
  for (std::uint64_t checkpoint = 0; checkpoint < rebindAllocations;
       ++checkpoint) {
    if (!RetargetDirectoryAlias(directoryAlias, targetA))
      return 63;
    EnvironmentLightingStore3D sweep;
    const auto old = sweep.Create(reparseDefinition);
    if (!old || !sweep.Set(old) ||
        !RetargetDirectoryAlias(directoryAlias, targetB))
      return 64;
    const auto before = sweep.MemoryStats();
    failAllocation.store(static_cast<long long>(checkpoint));
    EnvironmentLightingHandle3D failed{};
    try {
      failed = sweep.Refresh(reparseDefinition);
    } catch (...) {
      failAllocation.store(-1);
      return 65;
    }
    failAllocation.store(-1);
    if (failed || sweep.Active() != old ||
        !sameStats(sweep.MemoryStats(), before))
      return 66;
    const auto cachedOld = sweep.Create(reparseDefinition);
    if (cachedOld != old)
      return 67;
    sweep.Destroy(cachedOld);
    const auto recovered = sweep.Refresh(reparseDefinition);
    const auto absoluteOld = sweep.Create(absoluteA);
    const auto absoluteNew = sweep.Create(absoluteB);
    if (!recovered || recovered == old || absoluteOld != old ||
        absoluteNew != recovered || sweep.MemoryStats().payloadSets != 2)
      return 68;
    sweep.Destroy(absoluteNew);
    sweep.Destroy(old);
    sweep.Destroy(recovered);
    sweep.Destroy(absoluteOld);
  }
  if (!RemoveDirectoryW(directoryAlias.c_str()))
    return 69;

  EnvironmentLightingStore3D changedStore;
  const auto oldContent = changedStore.Create(d);
  if (!oldContent || changedStore.MemoryStats().fileReads != 3)
    return 19;
  if (!AtomicReplacePayloadByte(s, 1))
    return 70;
  const auto cachedAfterChange = changedStore.Create(d);
  if (cachedAfterChange != oldContent ||
      changedStore.MemoryStats().payloadSets != 1 ||
      changedStore.MemoryStats().fileReads != 3)
    return 20;
  const auto newContent = changedStore.Refresh(d);
  if (!newContent || newContent == oldContent ||
      changedStore.MemoryStats().residentSets != 2 ||
      changedStore.MemoryStats().payloadSets != 2 ||
      changedStore.MemoryStats().payloadBytes != parsed.payloadBytes * 2 ||
      changedStore.MemoryStats().fileReads != 6 ||
      changedStore.MemoryStats().refreshes != 1)
    return 21;
  if (!AtomicReplacePayloadByte(s, 0))
    return 71;
  const auto cachedBeforeRefresh = changedStore.Create(d);
  if (cachedBeforeRefresh != newContent ||
      changedStore.MemoryStats().fileReads != 6)
    return 22;
  const auto restoredContent = changedStore.Refresh(d);
  if (restoredContent != oldContent ||
      changedStore.MemoryStats().payloadSets != 2 ||
      changedStore.MemoryStats().fileReads != 9 ||
      changedStore.MemoryStats().refreshes != 2)
    return 23;
  // A/B handle 交錯釋放，最後一個 instance 才能釋放各自 payload。
  changedStore.Destroy(newContent);
  changedStore.Destroy(oldContent);
  changedStore.Destroy(cachedBeforeRefresh);
  if (changedStore.MemoryStats().payloadSets != 1 ||
      changedStore.MemoryStats().payloadBytes != parsed.payloadBytes ||
      !changedStore.Set(restoredContent))
    return 24;
  changedStore.Destroy(restoredContent);
  changedStore.Destroy(cachedAfterChange);
  if (changedStore.MemoryStats().residentSets ||
      changedStore.MemoryStats().payloadSets ||
      changedStore.MemoryStats().payloadBytes)
    return 44;

  EnvironmentLightingStore3D generationStore;
  const auto firstGeneration = generationStore.Create(d);
  if (!firstGeneration)
    return 73;
  generationStore.Destroy(firstGeneration);
  if (!generationStore.TestingSetFreeSlotGeneration(
          0, (std::numeric_limits<std::uint32_t>::max)()))
    return 74;
  const auto maximumGeneration = generationStore.Create(d);
  if (!maximumGeneration || std::uint32_t(maximumGeneration.value >> 32) !=
                                (std::numeric_limits<std::uint32_t>::max)())
    return 75;
  generationStore.Destroy(maximumGeneration);
  if (generationStore.Resolve(maximumGeneration) ||
      generationStore.MemoryStats().retiredSlots != 1)
    return 76;
  const auto afterRetirement = generationStore.Create(d);
  if (!afterRetirement || afterRetirement == maximumGeneration ||
      std::uint32_t(afterRetirement.value) != 2 ||
      generationStore.MemoryStats().retiredSlots != 1)
    return 77;
  generationStore.Destroy(afterRetirement);
  generationStore.Trim();
  if (generationStore.MemoryStats().retiredSlots ||
      generationStore.MemoryStats().metadataBytes)
    return 78;

  EnvironmentLightingStore3D referenceLimitStore;
  const auto limited = referenceLimitStore.Create(d);
  if (!limited)
    return 79;
  EnvironmentLightingStoreTestLimits3D limits{};
  limits.slotReferences = 2;
  limits.residentReferences = 2;
  referenceLimitStore.TestingSetLimits(limits);
  const auto limitedRetain = referenceLimitStore.Create(d);
  const auto beforeReferenceOverflow = referenceLimitStore.MemoryStats();
  if (limitedRetain != limited || referenceLimitStore.Create(d) ||
      !sameStats(referenceLimitStore.MemoryStats(), beforeReferenceOverflow))
    return 80;
  referenceLimitStore.Destroy(limitedRetain);
  referenceLimitStore.Destroy(limited);

  EnvironmentLightingStore3D instanceLimitStore;
  const auto instanceBaseline = instanceLimitStore.Create(d);
  if (!instanceBaseline)
    return 81;
  limits = {};
  limits.residentInstances = 1;
  limits.payloadInstances = 1;
  instanceLimitStore.TestingSetLimits(limits);
  auto anotherRotationDefinition = d;
  anotherRotationDefinition.rotationRadians += 1.f;
  const auto beforeInstanceOverflow = instanceLimitStore.MemoryStats();
  if (instanceLimitStore.Create(anotherRotationDefinition) ||
      !sameStats(instanceLimitStore.MemoryStats(), beforeInstanceOverflow))
    return 82;
  instanceLimitStore.Destroy(instanceBaseline);

  const auto limitedSpecular = root / "s-payload-limit.dds";
  std::filesystem::copy_file(s, limitedSpecular);
  auto payloadLimitDefinition = d;
  payloadLimitDefinition.specularCubePath = limitedSpecular;
  EnvironmentLightingStore3D payloadLimitStore;
  const auto payloadBaseline = payloadLimitStore.Create(payloadLimitDefinition);
  if (!payloadBaseline || !PatchPayloadByte(limitedSpecular, 1))
    return 83;
  limits = {};
  limits.payloadSets = 1;
  limits.payloadBytes = payloadLimitStore.MemoryStats().payloadBytes;
  payloadLimitStore.TestingSetLimits(limits);
  const auto beforePayloadOverflow = payloadLimitStore.MemoryStats();
  if (payloadLimitStore.Refresh(payloadLimitDefinition) ||
      !sameStats(payloadLimitStore.MemoryStats(), beforePayloadOverflow) ||
      payloadLimitStore.Create(payloadLimitDefinition) != payloadBaseline)
    return 84;
  payloadLimitStore.Destroy(payloadBaseline);
  payloadLimitStore.Destroy(payloadBaseline);

  EnvironmentLightingStore3D saturatedStore;
  const auto saturated = saturatedStore.Create(d);
  if (!saturated)
    return 85;
  limits = {};
  limits.eventCounter = 4;
  saturatedStore.TestingSetLimits(limits);
  for (std::size_t index = 0; index < 12; ++index)
    if (saturatedStore.Create(d) != saturated)
      return 86;
  const auto saturatedRefresh = saturatedStore.Refresh(d);
  const auto saturatedStats = saturatedStore.MemoryStats();
  if (saturatedRefresh != saturated || saturatedStats.cacheHits != 4 ||
      saturatedStats.fastHits != 4 || saturatedStats.fileReads != 4 ||
      saturatedStats.refreshes != 1)
    return 87;
  for (std::size_t index = 0; index < 13; ++index)
    saturatedStore.Destroy(saturated);
  saturatedStore.Destroy(saturatedRefresh);

  EnvironmentLightingStore3D leaseStore;
  auto leaseDefinition = d;
  leaseDefinition.rotationRadians = 1.25f;
  const auto leaseHandle = leaseStore.Create(leaseDefinition);
  const auto lease = leaseStore.Resolve(leaseHandle);
  if (!lease || !lease.payload->specular.Valid() ||
      !lease.payload->irradiance.Valid() || !lease.payload->brdf.Valid() ||
      lease.payload->payloadBytes != parsed.payloadBytes ||
      lease.rotationRadians != 1.25f || leaseStore.Resolve({}))
    return 88;
  leaseStore.Destroy(leaseHandle);
  if (leaseStore.Resolve(leaseHandle) || !lease.payload->specular.Valid() ||
      lease.payload->payloadBytes != parsed.payloadBytes)
    return 89;

  EnvironmentLightingStore3D moveLeaseStore;
  const auto moveLeaseHandle = moveLeaseStore.Create(d);
  const auto moveLease = moveLeaseStore.Resolve(moveLeaseHandle);
  EnvironmentLightingStore3D movedLeaseStore = std::move(moveLeaseStore);
  if (!moveLease || moveLeaseStore.Resolve(moveLeaseHandle) ||
      !movedLeaseStore.Resolve(moveLeaseHandle))
    return 90;
  movedLeaseStore.Shutdown();
  if (movedLeaseStore.Resolve(moveLeaseHandle) ||
      !moveLease.payload->specular.Valid() ||
      moveLease.payload->payloadBytes != parsed.payloadBytes)
    return 91;
  return tempRoot.Cleanup() ? 0 : 93;
}
