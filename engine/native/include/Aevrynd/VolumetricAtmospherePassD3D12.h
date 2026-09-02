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

#if defined(AEVRYND_INTERNAL_RENDERER_TEST_HOOKS) && AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
// 僅供 internal fixture 驗證 pattern 身分；production ABI 與 shader 路徑完全不含此契約。
[[nodiscard]] constexpr std::uint64_t VolumetricAtmosphereUpscaleFixturePatternHash3D(
    std::uint32_t width, std::uint32_t height) noexcept {
  std::uint64_t hash = 14695981039346656037ull;
  const auto mix = [&](std::uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte) {
      hash ^= std::uint8_t(value >> (byte * 8));
      hash *= 1099511628211ull;
    }
  };
  mix(width); mix(height); mix(0x3ca3d70au); mix(0x3d23d70au);
  mix(0x3da3d70au); mix(0x3df5c28fu); mix(0x3eb851ecu);
  mix(0x3e3851ecu); mix(0x3d75c28fu); mix(0x3f1eb852u);
  return hash;
}
#endif

// 64x64 deterministic blue-noise-like tile 將低頻能量推向 Nyquist；shader 再與 R2 時序組合。
[[nodiscard]] constexpr std::uint8_t
VolumetricBlueNoiseR8Sample3D(std::uint32_t x, std::uint32_t y) noexcept {
  x &= 63u;
  y &= 63u;
  std::uint32_t value = x * 0x8da6b343u ^ y * 0xd8163841u ^ 0x9e3779b9u;
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  // 棋盤半區分層保留直方圖，同時抑制純 hash tile 的低頻群聚。
  return std::uint8_t((value & 127u) + (((x + y) & 1u) << 7u));
}

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
  float inverseViewProjection[16]{1, 0, 0, 0, 0, 1, 0, 0,
                                  0, 0, 1, 0, 0, 0, 0, 1};
  float previousViewProjection[16]{1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};
  float cameraPositionX{};
  float cameraPositionY{};
  float cameraPositionZ{};
  float viewportOriginX{};
  float viewportOriginY{};
  float viewportWidth{1.f};
  float viewportHeight{1.f};
  float cloudBaseAltitude{1200.f};
  float cloudThickness{1800.f};
  bool projectionOrthographic{};
  bool previousProjectionOrthographic{};
  // 呼叫端必須在重用此 slice 前等待對應 frame fence。
  std::uint32_t frameSlice{};
  std::uint32_t temporalSampleIndex{};
  bool resetHistory{};
  // 僅在 internal renderer test build 中生效；production 會固定走零成本路徑。
  bool enableTemporalDiagnostics{};
  float maximumTemporalDeltaSeconds{.5f};
  bool useExternalTemporalSampleIndex{};
  std::uint32_t cloudSampleCount{16};
  float cloudMaximumDistance{12000.f};
  std::uint32_t cloudBaseNoiseSeed{0x9e3779b9u};
  std::uint32_t cloudDetailNoiseSeed{0x243f6a88u};
  float cloudBaseNoiseRotation{.37f};
  float cloudDetailNoiseRotation{-1.11f};
  bool cloudShadowsEnabled{true};
  float cloudShadowStrength{.72f};
  std::uint32_t cloudShadowSampleCount{4};
  std::uint32_t lightShaftSampleCount{4};
  float lightShaftMaximumDistance{320.f};
  float lightShaftShadowStrength{.72f};
  float heightFogDensity{.0035f};
  float heightFogFalloff{.09f};
  float heightFogBaseHeight{-2.f};
  float heightFogMaximumOpacity{.76f};
  float sunRadianceR{1.f};
  float sunRadianceG{.96f};
  float sunRadianceB{.88f};
  float sunRadianceIntensity{1.f};
  float ambientRadianceR{.12f};
  float ambientRadianceG{.17f};
  float ambientRadianceB{.24f};
#if defined(AEVRYND_INTERNAL_RENDERER_TEST_HOOKS) && AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  // 只存在於 internal renderer test ABI；production struct 沒有 mutation 欄位。
  std::uint32_t testMutation{};
#endif
};

struct VolumetricAtmospherePassStats3D {
  std::uint64_t dispatches{};
  std::uint64_t skippedFrames{};
  std::uint64_t historyResets{};
  std::uint64_t resourceCreates{};
  std::uint64_t resourceReleases{};
  std::uint64_t dispatchedThreadGroups{};
  std::uint64_t temporalReprojectionFrames{};
  std::uint64_t temporalResetFrames{};
  std::uint64_t temporalAcceptedPixels{};
  std::uint64_t temporalRejectedPixels{};
  std::uint64_t temporalInvalidPixels{};
  std::uint64_t temporalDiagnosticFrames{};
  std::uint32_t outputWidth{};
  std::uint32_t outputHeight{};
  std::size_t residentBytes{};
  bool historyValid{};
  bool temporalDiagnosticsAvailable{};
  std::uint32_t appliedCloudSampleCount{};
  std::uint32_t appliedLightShaftSampleCount{};
  std::uint32_t appliedCloudShadowSampleCount{};
  float appliedCloudMaximumDistance{};
  float appliedLightShaftMaximumDistance{};
  float appliedCloudShadowStrength{};
  float appliedLightShaftShadowStrength{};
  std::uint64_t appliedSettingsFingerprint{};
  std::size_t blueNoiseResidentBytes{};
  std::uint64_t temporalTimeJumpResets{};
  std::uint64_t temporalTopologyResets{};
#if defined(AEVRYND_INTERNAL_RENDERER_TEST_HOOKS) && AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  std::uint32_t appliedTestMutation{};
  std::uint64_t appliedFixturePatternHash{};
  std::uint32_t appliedFixtureWidth{}, appliedFixtureHeight{};
  bool appliedResetHistory{}, appliedHistoryValid{};
  std::uint32_t appliedTemporalSampleIndex{};
#endif
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
  [[nodiscard]] bool Record(ID3D12GraphicsCommandList *commandList,
                            ID3D12Resource *sceneDepth,
                            const VolumetricAtmospherePassSettings3D &settings);
  // 對應 frame fence 完成後收集 GPU 實際分支計數；同一 slice 僅收集一次。
  [[nodiscard]] bool CollectTemporalDiagnostics(std::uint32_t frameSlice);
  void InvalidateHistory() noexcept;
  void Shutdown() noexcept;

  [[nodiscard]] ID3D12Resource *OutputResource() const noexcept;
  [[nodiscard]] const VolumetricAtmospherePassStats3D &Stats() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace Aevrynd
