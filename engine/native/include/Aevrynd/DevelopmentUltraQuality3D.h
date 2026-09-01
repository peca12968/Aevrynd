#pragma once

#include <cstddef>
#include <cstdint>

struct IDXGIAdapter3;

namespace Aevrynd {

enum class DevelopmentUltraResolutionTier3D : std::uint8_t {
  FullHd,
  QuadHd,
};

enum class DevelopmentUltraQualityMode3D : std::uint8_t {
  High,
  DevelopmentUltra,
};

enum class DevelopmentUltraLockMode3D : std::uint8_t {
  Adaptive,
  QualityLocked,
};

enum class DevelopmentUltraAdmissionReason3D : std::uint8_t {
  Admitted,
  InvalidProfile,
  TelemetryUnavailable,
  TelemetryStale,
  PendingGpuReleases,
  ArithmeticOverflow,
  EngineBudgetExceeded,
  AdapterHeadroomExceeded,
  RuntimeApplicationNotIntegrated,
};

enum class DevelopmentUltraAction3D : std::uint8_t {
  Hold,
  Raise,
  Lower,
  EmergencyLower,
};

struct DevelopmentUltraResolutionSettings3D {
  std::uint32_t width{};
  std::uint32_t height{};
  float volumetricResolutionScale{};
  std::uint32_t cloudSampleCount{};
  std::uint32_t cloudShadowSampleCount{};
  std::uint32_t lightShaftSampleCount{};
  float vegetationDensityScale{};
  friend constexpr bool operator==(
      const DevelopmentUltraResolutionSettings3D &,
      const DevelopmentUltraResolutionSettings3D &) = default;
};

struct DevelopmentUltraFrameGate3D {
  std::uint32_t targetFramesPerSecond{60};
  float minimumGpuBudgetRatio{.80f};
  float maximumGpuBudgetRatio{.90f};
  float maximumP99BudgetRatio{.95f};
  float maximumFrameBudgetRatio{1.f};
  std::uint32_t sustainedWindowCount{3};
  friend constexpr bool operator==(const DevelopmentUltraFrameGate3D &,
                                   const DevelopmentUltraFrameGate3D &) =
      default;
};

struct DevelopmentUltraTransitionPolicy3D {
  bool waitForFenceBeforeApply{true};
  bool requirePendingReleasesZero{true};
  bool trimTransientMemoryOnExit{true};
  std::uint32_t stableFramesBeforeCommit{3};
  std::uint32_t maximumFlushesPerTransition{1};
  friend constexpr bool operator==(
      const DevelopmentUltraTransitionPolicy3D &,
      const DevelopmentUltraTransitionPolicy3D &) = default;
};

struct DevelopmentUltraQuality3D {
  DevelopmentUltraQualityMode3D mode{
      DevelopmentUltraQualityMode3D::DevelopmentUltra};
  DevelopmentUltraLockMode3D lockMode{
      DevelopmentUltraLockMode3D::QualityLocked};
  DevelopmentUltraResolutionTier3D tier{
      DevelopmentUltraResolutionTier3D::FullHd};
  DevelopmentUltraResolutionSettings3D resolution{};
  std::uint32_t shadowCascadeCount{4};
  std::uint32_t shadowResolution{4096};
  std::size_t textureBudgetBytes{2ull * 1024ull * 1024ull * 1024ull};
  std::size_t meshBudgetBytes{1ull * 1024ull * 1024ull * 1024ull};
  std::size_t maximumHardVramBytes{10ull * 1024ull * 1024ull * 1024ull};
  float dedicatedVramFraction{.70f};
  DevelopmentUltraFrameGate3D frameGate{};
  DevelopmentUltraTransitionPolicy3D transition{};
  bool runtimeApplicationEnabled{};
  friend constexpr bool operator==(const DevelopmentUltraQuality3D &,
                                   const DevelopmentUltraQuality3D &) =
      default;
};

struct DxgiVideoMemorySegment3D {
  bool querySucceeded{};
  std::uint64_t budgetBytes{};
  std::uint64_t currentUsageBytes{};
  std::uint64_t availableForReservationBytes{};
  std::uint64_t currentReservationBytes{};
};

struct DxgiVideoMemoryTelemetry3D {
  bool adapterFound{};
  bool hardwareAdapter{};
  std::uint32_t adapterLuidLow{};
  std::int32_t adapterLuidHigh{};
  std::uint64_t dedicatedVideoMemoryBytes{};
  DxgiVideoMemorySegment3D local{};
  DxgiVideoMemorySegment3D nonLocal{};
  std::uint64_t sampledAtSteadyNanoseconds{};
  std::uint64_t ageNanoseconds{};
  std::uint64_t queryEpoch{};
};

struct DevelopmentUltraAdmissionInput3D {
  std::size_t currentEngineResidentBytes{};
  std::size_t additionalRequiredBytes{};
  std::size_t pendingGpuReleaseBytes{};
  std::uint64_t maximumTelemetryAgeNanoseconds{2'000'000'000ull};
};

struct DevelopmentUltraAdmission3D {
  bool admitted{};
  DevelopmentUltraAdmissionReason3D reason{
      DevelopmentUltraAdmissionReason3D::InvalidProfile};
  std::size_t hardVramBudgetBytes{};
  std::size_t projectedEngineResidentBytes{};
  std::uint64_t adapterHeadroomBytes{};
  std::uint64_t requiredAdapterHeadroomBytes{};
};

struct DevelopmentUltraFrameSample3D {
  std::uint64_t gpuP95Nanoseconds{};
  std::uint64_t gpuP99Nanoseconds{};
  std::uint64_t gpuMaximumNanoseconds{};
};

struct DevelopmentUltraGpuTruthSequence3D {
  std::uint64_t lastSubmittedFrameSerial{};
  std::uint64_t lastCompletedFrameSerial{};
  std::uint64_t lastTimingSampleSerial{};
  std::uint32_t stableFrames{};
};

struct DevelopmentUltraHysteresis3D {
  std::uint32_t consecutiveUnderBudgetWindows{};
  std::uint32_t consecutiveOverBudgetWindows{};
  DevelopmentUltraAction3D lastAction{DevelopmentUltraAction3D::Hold};
};

struct DevelopmentUltraReceipt3D {
  DevelopmentUltraQualityMode3D requested{
      DevelopmentUltraQualityMode3D::DevelopmentUltra};
  DevelopmentUltraQualityMode3D actual{DevelopmentUltraQualityMode3D::High};
  DevelopmentUltraAdmissionReason3D reason{
      DevelopmentUltraAdmissionReason3D::RuntimeApplicationNotIntegrated};
  std::uint64_t requestedFingerprint{};
  std::uint64_t normalizedFingerprint{};
  std::uint64_t actualFingerprint{};
  std::uint64_t epoch{};
  std::uint64_t downgradeCount{};
  bool requestActive{};
  bool noOp{};
  bool rendererApplied{};
  bool transitionPolicyApplied{};
  bool shadowResourcesApplied{};
  bool volumetricResolutionApplied{};
  bool cloudSamplesApplied{};
  bool cloudShadowSamplesApplied{};
  bool lightShaftSamplesApplied{};
  bool vegetationDensityApplied{};
  bool textureBudgetApplied{};
  bool meshBudgetApplied{};
};

[[nodiscard]] DevelopmentUltraQuality3D MakeDevelopmentUltraQuality3D(
    DevelopmentUltraResolutionTier3D tier) noexcept;
[[nodiscard]] DevelopmentUltraQuality3D NormalizeDevelopmentUltraQuality3D(
    DevelopmentUltraQuality3D profile) noexcept;
[[nodiscard]] bool IsCanonicalDevelopmentUltraQuality3D(
    const DevelopmentUltraQuality3D &profile) noexcept;
[[nodiscard]] std::uint64_t FingerprintDevelopmentUltraQuality3D(
    const DevelopmentUltraQuality3D &profile) noexcept;
[[nodiscard]] std::size_t DevelopmentUltraHardVramBudgetBytes(
    const DevelopmentUltraQuality3D &profile,
    std::uint64_t dedicatedVideoMemoryBytes) noexcept;
[[nodiscard]] DxgiVideoMemoryTelemetry3D QueryDxgiVideoMemoryTelemetry3D(
    IDXGIAdapter3 &adapter, std::uint64_t queryEpoch = 0) noexcept;
[[nodiscard]] DxgiVideoMemoryTelemetry3D
QueryPrimaryDxgiVideoMemoryTelemetry3D(std::uint64_t queryEpoch = 0) noexcept;
[[nodiscard]] DevelopmentUltraAdmission3D EvaluateDevelopmentUltraAdmission3D(
    const DevelopmentUltraQuality3D &profile,
    const DxgiVideoMemoryTelemetry3D &telemetry,
    const DevelopmentUltraAdmissionInput3D &input) noexcept;
[[nodiscard]] DevelopmentUltraAction3D UpdateDevelopmentUltraHysteresis3D(
    const DevelopmentUltraQuality3D &profile,
    const DevelopmentUltraFrameSample3D &sample,
    DevelopmentUltraHysteresis3D &state) noexcept;
[[nodiscard]] DevelopmentUltraReceipt3D StageDevelopmentUltraReceipt3D(
    const DevelopmentUltraQuality3D &profile,
    const DevelopmentUltraAdmission3D &admission, std::uint64_t epoch,
    std::uint64_t downgradeCount = 0) noexcept;
[[nodiscard]] std::uint64_t NextDevelopmentUltraEpoch3D(
    std::uint64_t current) noexcept;
[[nodiscard]] std::uint64_t AddDevelopmentUltraDowngradeCount3D(
    std::uint64_t current, std::uint64_t amount = 1) noexcept;
void RecordDevelopmentUltraAction3D(DevelopmentUltraReceipt3D &receipt,
                                    DevelopmentUltraAction3D action) noexcept;
[[nodiscard]] bool AdvanceDevelopmentUltraGpuTruthSequence3D(
    DevelopmentUltraGpuTruthSequence3D &sequence,
    std::uint64_t submittedFrameSerial,
    std::uint64_t completedFrameSerial,
    std::uint64_t timingSampleSerial, bool framePassed,
    std::uint32_t requiredStableFrames) noexcept;
[[nodiscard]] bool DevelopmentUltraPostApplyGate3D(
    bool rendererApplySucceeded,
    std::size_t pendingGpuReleaseBytes) noexcept;

} // namespace Aevrynd
