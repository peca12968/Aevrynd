#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace Aevrynd {

enum class VolumetricAtmospherePhase3D : std::uint32_t {
  Day,
  Dusk,
  Night,
};

struct VolumetricAtmospherePassSettings3D {
  bool enabled{true};
  VolumetricAtmospherePhase3D phase{VolumetricAtmospherePhase3D::Day};
  float resolutionScale{.5f};
  float historyWeight{.88f};
  float timeSeconds{};
  float cloudCoverage{.48f};
  float cloudDensity{1.08f};
  float cloudErosion{.28f};
  float cloudWindSpeed{18.f};
  float lightShaftIntensity{.68f};
  float lightShaftAnisotropy{.72f};
  float sunDirectionX{.35f};
  float sunDirectionY{.82f};
  float sunDirectionZ{-.44f};
  std::uint32_t temporalSampleIndex{};
  bool resetHistory{};
};

struct VolumetricAtmospherePassStats3D {
  std::uint64_t dispatches{};
  std::uint64_t skippedFrames{};
  std::uint64_t historyResets{};
  std::uint64_t resourceCreates{};
  std::uint64_t resourceReleases{};
  std::uint64_t dispatchedThreadGroups{};
  std::uint32_t outputWidth{};
  std::uint32_t outputHeight{};
  std::size_t residentBytes{};
  bool historyValid{};
};

// 以低解析率計算雲層與定向光束；輸出保留為 NON_PIXEL_SHADER_RESOURCE。
// Record 會綁定自己的 descriptor heap，整合端後續繪圖前必須恢復原 heap，
// 並可另建 SRV、在合成前轉換為 PIXEL_SHADER_RESOURCE。
class VolumetricAtmospherePassD3D12 final {
public:
  VolumetricAtmospherePassD3D12();
  ~VolumetricAtmospherePassD3D12();
  VolumetricAtmospherePassD3D12(VolumetricAtmospherePassD3D12 &&) noexcept;
  VolumetricAtmospherePassD3D12 &
  operator=(VolumetricAtmospherePassD3D12 &&) noexcept;
  VolumetricAtmospherePassD3D12(
      const VolumetricAtmospherePassD3D12 &) = delete;
  VolumetricAtmospherePassD3D12 &
  operator=(const VolumetricAtmospherePassD3D12 &) = delete;

  // 重新初始化或調整尺寸前，呼叫端必須等待使用舊資源的 GPU fence。
  [[nodiscard]] bool Initialize(ID3D12Device *device,
                                std::span<const std::byte> computeShader,
                                std::uint32_t outputWidth,
                                std::uint32_t outputHeight,
                                float resolutionScale = .5f);
  [[nodiscard]] bool Resize(std::uint32_t outputWidth,
                            std::uint32_t outputHeight,
                            float resolutionScale);
  [[nodiscard]] bool Record(ID3D12GraphicsCommandList *commandList,
                            const VolumetricAtmospherePassSettings3D &settings);
  void InvalidateHistory() noexcept;
  void Shutdown() noexcept;

  [[nodiscard]] ID3D12Resource *OutputResource() const noexcept;
  [[nodiscard]] const VolumetricAtmospherePassStats3D &Stats() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace Aevrynd
