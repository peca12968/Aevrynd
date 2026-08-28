#include "Aevrynd/EnvironmentIbl3D.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <string_view>
#include <vector>

using namespace Aevrynd;

namespace {
std::atomic<std::uint64_t> allocationCount{};

void Put(std::vector<std::byte> &bytes, std::size_t offset,
         std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index)
    bytes[offset + index] = std::byte(value >> (index * 8));
}

bool WriteDds(const std::filesystem::path &path, std::uint32_t width,
              std::uint32_t height, std::uint32_t format, bool cube,
              std::uint32_t mipCount) {
  std::size_t payloadBytes{};
  for (std::uint32_t face = 0; face < (cube ? 6u : 1u); ++face)
    for (std::uint32_t mip = 0; mip < mipCount; ++mip) {
      const auto mipWidth = (std::max)(1u, width >> mip);
      const auto mipHeight = (std::max)(1u, height >> mip);
      payloadBytes += cube ? std::size_t((mipWidth + 3) / 4) *
                                 ((mipHeight + 3) / 4) * 16
                           : std::size_t(mipWidth) * mipHeight * 4;
    }
  std::vector<std::byte> bytes(148 + payloadBytes);
  Put(bytes, 0, 0x20534444);
  Put(bytes, 4, 124);
  Put(bytes, 12, height);
  Put(bytes, 16, width);
  Put(bytes, 28, mipCount);
  Put(bytes, 76, 32);
  Put(bytes, 80, 4);
  Put(bytes, 84, 0x30315844);
  Put(bytes, 128, format);
  Put(bytes, 132, 3);
  Put(bytes, 136, cube ? 4 : 0);
  Put(bytes, 140, 1);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               std::streamsize(bytes.size()));
  return bool(output);
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
} // namespace

void *operator new(std::size_t bytes) {
  ++allocationCount;
  if (void *memory = std::malloc(bytes ? bytes : 1))
    return memory;
  throw std::bad_alloc{};
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }

int main() {
  constexpr std::size_t RotationCount = 64;
  constexpr std::size_t SameHitCount = 10000;
  ScopedTempRoot tempRoot(L"aevrynd-environment-lighting-benchmark");
  if (!tempRoot.Valid())
    return 10;
  const auto &root = tempRoot.Path();
  const auto specular = root / "specular.dds";
  const auto irradiance = root / "irradiance.dds";
  const auto brdf = root / "brdf.dds";
  if (!WriteDds(specular, 16, 16, 95, true, 5) ||
      !WriteDds(irradiance, 8, 8, 95, true, 4) ||
      !WriteDds(brdf, 16, 16, 34, false, 1))
    return 1;

  EnvironmentLightingStore3D store;
  EnvironmentIblDefinition3D definition{specular, irradiance, brdf, 0.f};
  const auto warmHandle = store.Create(definition);
  if (!warmHandle || store.MemoryStats().fileReads != 3)
    return 2;
  std::vector<EnvironmentLightingHandle3D> rotations;
  rotations.reserve(RotationCount);
  const auto rotationFileReads = store.MemoryStats().fileReads;
  const auto rotationAllocationStart = allocationCount.load();
  const auto rotationStart = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < RotationCount; ++index) {
    definition.rotationRadians = float(index + 1) * .05f;
    const auto handle = store.Create(definition);
    if (!handle || std::find(rotations.begin(), rotations.end(), handle) !=
                       rotations.end())
      return 3;
    rotations.push_back(handle);
  }
  const auto rotationEnd = std::chrono::steady_clock::now();
  const auto rotationAllocations =
      allocationCount.load() - rotationAllocationStart;
  const auto rotationStats = store.MemoryStats();
  if (rotationStats.residentInstances != RotationCount + 1 ||
      rotationStats.payloadSets != 1 || !rotationStats.payloadBytes ||
      rotationStats.fileReads != rotationFileReads)
    return 4;

  definition.rotationRadians = 0.f;
  const auto sameAllocationStart = allocationCount.load();
  const auto sameStart = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < SameHitCount; ++index)
    if (store.Create(definition) != warmHandle)
      return 5;
  const auto sameEnd = std::chrono::steady_clock::now();
  const auto sameAllocations = allocationCount.load() - sameAllocationStart;
  const auto hitStats = store.MemoryStats();
  if (hitStats.residentReferences != RotationCount + SameHitCount + 1 ||
      hitStats.payloadSets != 1 ||
      hitStats.payloadBytes != rotationStats.payloadBytes ||
      hitStats.fileReads != rotationStats.fileReads)
    return 6;

  const auto rotationMicroseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(rotationEnd -
                                                            rotationStart)
          .count();
  const auto sameHitMicroseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(sameEnd - sameStart)
          .count();
  // 正式 fast-path 閘門：warm Create 不得配置或讀檔，平均至多 5 微秒。
  if (rotationMicroseconds > 10000 || sameAllocations != 0 ||
      hitStats.fileReads != rotationStats.fileReads ||
      sameHitMicroseconds > 50000)
    return 7;
  std::cout << "rotations=" << RotationCount
            << " rotationUs=" << rotationMicroseconds
            << " rotationAllocations=" << rotationAllocations
            << " payloadSets=" << rotationStats.payloadSets
            << " payloadBytes=" << rotationStats.payloadBytes
            << " sameHits=" << SameHitCount
            << " sameHitUs=" << sameHitMicroseconds
            << " sameHitAllocations=" << sameAllocations << '\n';

  for (std::size_t index = 0; index < SameHitCount; ++index)
    store.Destroy(warmHandle);
  for (const auto handle : rotations)
    store.Destroy(handle);
  store.Destroy(warmHandle);
  if (store.MemoryStats().residentInstances ||
      store.MemoryStats().residentReferences || store.MemoryStats().payloadSets ||
      store.MemoryStats().payloadBytes)
    return 8;
  store.Trim();
  if (store.MemoryStats().reclaimableBytes)
    return 9;
  return tempRoot.Cleanup() ? 0 : 11;
}
