#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <vector>

namespace Aevrynd {
enum class EnvironmentIblTextureKind3D : std::uint8_t {
  SpecularCube,
  IrradianceCube,
  BrdfLut
};
struct EnvironmentIblSubresource3D {
  std::uint32_t width{}, height{}, face{}, mip{};
  std::size_t offset{}, bytes{};
};
struct EnvironmentIblTextureData3D {
  EnvironmentIblTextureKind3D kind{};
  std::uint32_t width{}, height{}, mipCount{}, faceCount{};
  std::vector<std::byte> fileBytes;
  std::vector<EnvironmentIblSubresource3D> subresources;
  [[nodiscard]] bool Valid() const noexcept {
    return width && height && mipCount && faceCount && !fileBytes.empty() &&
           !subresources.empty();
  }
  [[nodiscard]] std::size_t PayloadBytes() const noexcept;
};
struct EnvironmentIblDefinition3D {
  std::filesystem::path specularCubePath, irradianceCubePath, brdfLutPath;
  float rotationRadians{};
};
struct EnvironmentIblValidatedData3D {
  EnvironmentIblTextureData3D specular, irradiance, brdf;
  std::size_t payloadBytes{};
  float rotationRadians{};
};
struct EnvironmentLightingHandle3D {
  std::uint64_t value{};
  explicit constexpr operator bool() const { return value != 0; }
  constexpr bool
  operator==(const EnvironmentLightingHandle3D &) const = default;
};
struct EnvironmentLightingPayloadLease3D {
  std::shared_ptr<const EnvironmentIblValidatedData3D> payload;
  float rotationRadians{};
  [[nodiscard]] explicit operator bool() const noexcept {
    return bool(payload);
  }
};
struct EnvironmentLightingMemoryStats3D {
  std::size_t residentInstances{}, residentReferences{}, payloadSets{},
      payloadBytes{}, sourceSets{}, instanceCacheEntries{},
      payloadCacheEntries{}, rawSourceEntries{}, canonicalSourceEntries{},
      retiredSlots{}, metadataBytes{}, reclaimableBytes{};
  // 舊欄位保留相容性；分別等同 residentInstances 與 payloadBytes。
  std::size_t residentSets{}, residentBytes{};
  std::uint64_t cacheHits{}, fastHits{}, fileReads{}, refreshes{}, creates{},
      destroys{}, trimCount{};
};
struct EnvironmentLightingStoreTestLimits3D {
  std::uint32_t slotReferences{(std::numeric_limits<std::uint32_t>::max)()},
      payloadInstances{(std::numeric_limits<std::uint32_t>::max)()};
  std::size_t residentInstances{(std::numeric_limits<std::size_t>::max)()},
      residentReferences{(std::numeric_limits<std::size_t>::max)()},
      payloadSets{(std::numeric_limits<std::size_t>::max)()},
      payloadBytes{(std::numeric_limits<std::size_t>::max)()};
  std::uint64_t eventCounter{(std::numeric_limits<std::uint64_t>::max)()};
};
class EnvironmentLightingStore3D {
public:
  EnvironmentLightingStore3D();
  ~EnvironmentLightingStore3D();
  EnvironmentLightingStore3D(EnvironmentLightingStore3D &&) noexcept;
  EnvironmentLightingStore3D &operator=(EnvironmentLightingStore3D &&) noexcept;
  EnvironmentLightingStore3D(const EnvironmentLightingStore3D &) = delete;
  EnvironmentLightingStore3D &
  operator=(const EnvironmentLightingStore3D &) = delete;
  [[nodiscard]] EnvironmentLightingHandle3D
  Create(const EnvironmentIblDefinition3D &definition);
  // 明確重新讀取來源；內容變更時切換至新 payload，既有 handle 仍有效。
  [[nodiscard]] EnvironmentLightingHandle3D
  Refresh(const EnvironmentIblDefinition3D &definition);
  void Destroy(EnvironmentLightingHandle3D handle);
  [[nodiscard]] bool Set(EnvironmentLightingHandle3D handle);
  [[nodiscard]] EnvironmentLightingHandle3D Active() const;
  [[nodiscard]] EnvironmentLightingPayloadLease3D
  Resolve(EnvironmentLightingHandle3D handle) const noexcept;
  [[nodiscard]] EnvironmentLightingMemoryStats3D MemoryStats() const;
  void Trim();
  void Shutdown();
  // 測試專用：以低上限驗證正式 UINT 上限的 fail-close 與飽和契約。
  void TestingSetLimits(const EnvironmentLightingStoreTestLimits3D &limits);
  [[nodiscard]] bool TestingSetFreeSlotGeneration(std::size_t index,
                                                  std::uint32_t generation);

private:
  [[nodiscard]] EnvironmentLightingHandle3D
  Acquire(const EnvironmentIblDefinition3D &definition, bool refresh);
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::uint64_t trimCount_{};
};

// 嚴格接受 DX10 DDS：兩張 BC6H UF16 cubemap 與一張 RG16_FLOAT BRDF LUT。
[[nodiscard]] bool
LoadAndValidateEnvironmentIbl3D(const EnvironmentIblDefinition3D &definition,
                                EnvironmentIblValidatedData3D &output);
} // namespace Aevrynd
