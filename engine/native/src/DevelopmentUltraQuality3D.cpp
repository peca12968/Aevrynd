#include <Aevrynd/DevelopmentUltraQuality3D.h>

#include <Windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Aevrynd {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::size_t GiB = 1024ull * 1024ull * 1024ull;

std::uint64_t SteadyNanoseconds() noexcept {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

template <typename T>
void HashValue(std::uint64_t &hash, const T &value) noexcept {
  static_assert(std::is_trivially_copyable_v<T>);
  const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  for (const std::byte byte : bytes) {
    hash ^= std::to_integer<std::uint8_t>(byte);
    hash *= 1099511628211ull;
  }
}

bool AddWithoutOverflow(std::size_t left, std::size_t right,
                        std::size_t &result) noexcept {
  if (right > (std::numeric_limits<std::size_t>::max)() - left) {
    return false;
  }
  result = left + right;
  return true;
}

bool KnownAdmissionReason(DevelopmentUltraAdmissionReason3D reason) noexcept {
  switch (reason) {
  case DevelopmentUltraAdmissionReason3D::Admitted:
  case DevelopmentUltraAdmissionReason3D::InvalidProfile:
  case DevelopmentUltraAdmissionReason3D::TelemetryUnavailable:
  case DevelopmentUltraAdmissionReason3D::TelemetryStale:
  case DevelopmentUltraAdmissionReason3D::PendingGpuReleases:
  case DevelopmentUltraAdmissionReason3D::ArithmeticOverflow:
  case DevelopmentUltraAdmissionReason3D::EngineBudgetExceeded:
  case DevelopmentUltraAdmissionReason3D::AdapterHeadroomExceeded:
  case DevelopmentUltraAdmissionReason3D::RuntimeApplicationNotIntegrated:
    return true;
  default:
    return false;
  }
}

bool KnownAction(DevelopmentUltraAction3D action) noexcept {
  switch (action) {
  case DevelopmentUltraAction3D::Hold:
  case DevelopmentUltraAction3D::Raise:
  case DevelopmentUltraAction3D::Lower:
  case DevelopmentUltraAction3D::EmergencyLower:
    return true;
  default:
    return false;
  }
}

std::uint64_t RatioBytes(std::uint64_t bytes, std::uint32_t numerator,
                         std::uint32_t denominator) noexcept {
  return bytes / denominator * numerator +
         bytes % denominator * numerator / denominator;
}

DxgiVideoMemorySegment3D QuerySegment(IDXGIAdapter3 &adapter,
                                      DXGI_MEMORY_SEGMENT_GROUP group) noexcept {
  DXGI_QUERY_VIDEO_MEMORY_INFO info{};
  DxgiVideoMemorySegment3D result{};
  result.querySucceeded = SUCCEEDED(adapter.QueryVideoMemoryInfo(0, group, &info));
  if (!result.querySucceeded) {
    return result;
  }
  result.budgetBytes = info.Budget;
  result.currentUsageBytes = info.CurrentUsage;
  result.availableForReservationBytes = info.AvailableForReservation;
  result.currentReservationBytes = info.CurrentReservation;
  return result;
}

} // namespace

DevelopmentUltraQuality3D MakeDevelopmentUltraQuality3D(
    DevelopmentUltraResolutionTier3D tier) noexcept {
  DevelopmentUltraQuality3D result{};
  result.tier = tier;
  if (tier == DevelopmentUltraResolutionTier3D::QuadHd) {
    result.resolution = {2560, 1440, .625f, 24, 6, 8, 1.25f};
  } else {
    result.tier = DevelopmentUltraResolutionTier3D::FullHd;
    result.resolution = {1920, 1080, .75f, 28, 7, 10, 1.50f};
  }
  return result;
}

DevelopmentUltraQuality3D NormalizeDevelopmentUltraQuality3D(
    DevelopmentUltraQuality3D profile) noexcept {
  const auto canonical = MakeDevelopmentUltraQuality3D(profile.tier);
  profile.resolution.width =
      std::clamp(profile.resolution.width, 1280u, 3840u);
  profile.resolution.height =
      std::clamp(profile.resolution.height, 720u, 2160u);
  profile.resolution.volumetricResolutionScale = std::clamp(
      std::isfinite(profile.resolution.volumetricResolutionScale)
          ? profile.resolution.volumetricResolutionScale
          : canonical.resolution.volumetricResolutionScale,
      .25f, 1.f);
  profile.resolution.cloudSampleCount =
      std::clamp(profile.resolution.cloudSampleCount, 4u, 32u);
  profile.resolution.cloudShadowSampleCount =
      std::clamp(profile.resolution.cloudShadowSampleCount, 1u, 16u);
  profile.resolution.lightShaftSampleCount =
      std::clamp(profile.resolution.lightShaftSampleCount, 2u, 16u);
  profile.resolution.vegetationDensityScale = std::clamp(
      std::isfinite(profile.resolution.vegetationDensityScale)
          ? profile.resolution.vegetationDensityScale
          : canonical.resolution.vegetationDensityScale,
      .25f, 2.f);
  profile.shadowCascadeCount = std::clamp(profile.shadowCascadeCount, 1u, 4u);
  profile.shadowResolution = std::clamp(profile.shadowResolution, 512u, 4096u);
  profile.textureBudgetBytes =
      std::clamp(profile.textureBudgetBytes, 256ull * 1024ull * 1024ull,
                 4ull * GiB);
  profile.meshBudgetBytes =
      std::clamp(profile.meshBudgetBytes, 128ull * 1024ull * 1024ull,
                 2ull * GiB);
  profile.maximumHardVramBytes =
      std::clamp(profile.maximumHardVramBytes, 1ull * GiB, 10ull * GiB);
  profile.dedicatedVramFraction = std::clamp(
      std::isfinite(profile.dedicatedVramFraction)
          ? profile.dedicatedVramFraction
          : .70f,
      .25f, .90f);
  profile.frameGate.targetFramesPerSecond =
      std::clamp(profile.frameGate.targetFramesPerSecond, 30u, 240u);
  profile.frameGate.minimumGpuBudgetRatio = std::clamp(
      std::isfinite(profile.frameGate.minimumGpuBudgetRatio)
          ? profile.frameGate.minimumGpuBudgetRatio
          : .80f,
      .50f, .89f);
  profile.frameGate.maximumGpuBudgetRatio = std::clamp(
      std::isfinite(profile.frameGate.maximumGpuBudgetRatio)
          ? profile.frameGate.maximumGpuBudgetRatio
          : .90f,
      profile.frameGate.minimumGpuBudgetRatio + .01f, .95f);
  profile.frameGate.maximumP99BudgetRatio = std::clamp(
      std::isfinite(profile.frameGate.maximumP99BudgetRatio)
          ? profile.frameGate.maximumP99BudgetRatio
          : .95f,
      profile.frameGate.maximumGpuBudgetRatio, .99f);
  profile.frameGate.maximumFrameBudgetRatio = std::clamp(
      std::isfinite(profile.frameGate.maximumFrameBudgetRatio)
          ? profile.frameGate.maximumFrameBudgetRatio
          : 1.f,
      profile.frameGate.maximumP99BudgetRatio, 1.f);
  profile.frameGate.sustainedWindowCount =
      std::clamp(profile.frameGate.sustainedWindowCount, 2u, 12u);
  profile.transition.stableFramesBeforeCommit =
      std::clamp(profile.transition.stableFramesBeforeCommit, 2u, 12u);
  profile.transition.maximumFlushesPerTransition =
      std::clamp(profile.transition.maximumFlushesPerTransition, 0u, 1u);
  return profile;
}

bool IsCanonicalDevelopmentUltraQuality3D(
    const DevelopmentUltraQuality3D &profile) noexcept {
  // 正式 Development Ultra 必須逐欄相符，避免 High 別名或欄位互換通過。
  return profile == MakeDevelopmentUltraQuality3D(profile.tier);
}

std::uint64_t FingerprintDevelopmentUltraQuality3D(
    const DevelopmentUltraQuality3D &source) noexcept {
  const auto profile = NormalizeDevelopmentUltraQuality3D(source);
  std::uint64_t hash = 1469598103934665603ull;
  HashValue(hash, profile.mode);
  HashValue(hash, profile.lockMode);
  HashValue(hash, profile.tier);
  HashValue(hash, profile.resolution.width);
  HashValue(hash, profile.resolution.height);
  HashValue(hash, profile.resolution.volumetricResolutionScale);
  HashValue(hash, profile.resolution.cloudSampleCount);
  HashValue(hash, profile.resolution.cloudShadowSampleCount);
  HashValue(hash, profile.resolution.lightShaftSampleCount);
  HashValue(hash, profile.resolution.vegetationDensityScale);
  HashValue(hash, profile.shadowCascadeCount);
  HashValue(hash, profile.shadowResolution);
  HashValue(hash, profile.textureBudgetBytes);
  HashValue(hash, profile.meshBudgetBytes);
  HashValue(hash, profile.maximumHardVramBytes);
  HashValue(hash, profile.dedicatedVramFraction);
  HashValue(hash, profile.frameGate.targetFramesPerSecond);
  HashValue(hash, profile.frameGate.minimumGpuBudgetRatio);
  HashValue(hash, profile.frameGate.maximumGpuBudgetRatio);
  HashValue(hash, profile.frameGate.maximumP99BudgetRatio);
  HashValue(hash, profile.frameGate.maximumFrameBudgetRatio);
  HashValue(hash, profile.frameGate.sustainedWindowCount);
  HashValue(hash, profile.transition.waitForFenceBeforeApply);
  HashValue(hash, profile.transition.requirePendingReleasesZero);
  HashValue(hash, profile.transition.trimTransientMemoryOnExit);
  HashValue(hash, profile.transition.stableFramesBeforeCommit);
  HashValue(hash, profile.transition.maximumFlushesPerTransition);
  HashValue(hash, profile.runtimeApplicationEnabled);
  return hash;
}

std::size_t DevelopmentUltraHardVramBudgetBytes(
    const DevelopmentUltraQuality3D &source,
    std::uint64_t dedicatedVideoMemoryBytes) noexcept {
  const auto profile = NormalizeDevelopmentUltraQuality3D(source);
  const auto fraction = static_cast<std::uint32_t>(
      std::llround(double(profile.dedicatedVramFraction) * 10000.0));
  const auto dedicatedLimit = RatioBytes(dedicatedVideoMemoryBytes, fraction, 10000);
  return static_cast<std::size_t>((std::min)(
      std::uint64_t(profile.maximumHardVramBytes), dedicatedLimit));
}

DxgiVideoMemoryTelemetry3D QueryDxgiVideoMemoryTelemetry3D(
    IDXGIAdapter3 &adapter, std::uint64_t queryEpoch) noexcept {
  DxgiVideoMemoryTelemetry3D result{};
  DXGI_ADAPTER_DESC2 description{};
  result.adapterFound = SUCCEEDED(adapter.GetDesc2(&description));
  if (!result.adapterFound) {
    return result;
  }
  result.hardwareAdapter =
      (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
  result.adapterLuidLow = description.AdapterLuid.LowPart;
  result.adapterLuidHigh = description.AdapterLuid.HighPart;
  result.dedicatedVideoMemoryBytes = description.DedicatedVideoMemory;
  result.local = QuerySegment(adapter, DXGI_MEMORY_SEGMENT_GROUP_LOCAL);
  result.nonLocal = QuerySegment(adapter, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL);
  result.sampledAtSteadyNanoseconds = SteadyNanoseconds();
  result.ageNanoseconds = 0;
  result.queryEpoch = queryEpoch;
  return result;
}

DxgiVideoMemoryTelemetry3D
QueryPrimaryDxgiVideoMemoryTelemetry3D(std::uint64_t queryEpoch) noexcept {
  ComPtr<IDXGIFactory6> factory;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
    return {};
  }
  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> adapter1;
    const HRESULT status = factory->EnumAdapterByGpuPreference(
        index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter1));
    if (status == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(status)) {
      return {};
    }
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter1->GetDesc1(&description)) ||
        (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
      continue;
    }
    ComPtr<IDXGIAdapter3> adapter3;
    if (SUCCEEDED(adapter1.As(&adapter3))) {
      return QueryDxgiVideoMemoryTelemetry3D(*adapter3.Get(), queryEpoch);
    }
  }
  return {};
}

DevelopmentUltraAdmission3D EvaluateDevelopmentUltraAdmission3D(
    const DevelopmentUltraQuality3D &profile,
    const DxgiVideoMemoryTelemetry3D &telemetry,
    const DevelopmentUltraAdmissionInput3D &input) noexcept {
  DevelopmentUltraAdmission3D result{};
  if (!IsCanonicalDevelopmentUltraQuality3D(profile)) {
    result.reason = DevelopmentUltraAdmissionReason3D::InvalidProfile;
    return result;
  }
  if (!telemetry.adapterFound || !telemetry.hardwareAdapter ||
      !telemetry.local.querySucceeded || !telemetry.nonLocal.querySucceeded ||
      telemetry.dedicatedVideoMemoryBytes == 0) {
    result.reason = DevelopmentUltraAdmissionReason3D::TelemetryUnavailable;
    return result;
  }
  const auto now = SteadyNanoseconds();
  const auto measuredAge =
      telemetry.sampledAtSteadyNanoseconds != 0 &&
              now >= telemetry.sampledAtSteadyNanoseconds
          ? now - telemetry.sampledAtSteadyNanoseconds
          : 0;
  if (telemetry.sampledAtSteadyNanoseconds == 0) {
    result.reason = DevelopmentUltraAdmissionReason3D::TelemetryUnavailable;
    return result;
  }
  if (telemetry.sampledAtSteadyNanoseconds > now ||
      (std::max)(telemetry.ageNanoseconds, measuredAge) >
      input.maximumTelemetryAgeNanoseconds) {
    result.reason = DevelopmentUltraAdmissionReason3D::TelemetryStale;
    return result;
  }
  if (input.pendingGpuReleaseBytes != 0) {
    result.reason = DevelopmentUltraAdmissionReason3D::PendingGpuReleases;
    return result;
  }
  result.hardVramBudgetBytes = DevelopmentUltraHardVramBudgetBytes(
      profile, telemetry.dedicatedVideoMemoryBytes);
  if (!AddWithoutOverflow(input.currentEngineResidentBytes,
                          input.additionalRequiredBytes,
                          result.projectedEngineResidentBytes)) {
    result.reason = DevelopmentUltraAdmissionReason3D::ArithmeticOverflow;
    return result;
  }
  if (result.projectedEngineResidentBytes > result.hardVramBudgetBytes) {
    result.reason = DevelopmentUltraAdmissionReason3D::EngineBudgetExceeded;
    return result;
  }
  result.adapterHeadroomBytes =
      telemetry.local.currentUsageBytes < telemetry.local.budgetBytes
          ? telemetry.local.budgetBytes - telemetry.local.currentUsageBytes
          : 0;
  // 保留 10% DXGI 預算，不把其他程序與桌面合成器逼到邊界。
  result.requiredAdapterHeadroomBytes =
      input.additionalRequiredBytes + telemetry.local.budgetBytes / 10;
  if (result.requiredAdapterHeadroomBytes < input.additionalRequiredBytes) {
    result.reason = DevelopmentUltraAdmissionReason3D::ArithmeticOverflow;
    return result;
  }
  if (result.adapterHeadroomBytes < result.requiredAdapterHeadroomBytes) {
    result.reason =
        DevelopmentUltraAdmissionReason3D::AdapterHeadroomExceeded;
    return result;
  }
  result.admitted = true;
  result.reason = DevelopmentUltraAdmissionReason3D::Admitted;
  return result;
}

DevelopmentUltraAction3D UpdateDevelopmentUltraHysteresis3D(
    const DevelopmentUltraQuality3D &source,
    const DevelopmentUltraFrameSample3D &sample,
    DevelopmentUltraHysteresis3D &state) noexcept {
  const auto profile = NormalizeDevelopmentUltraQuality3D(source);
  auto canonicalCandidate = source;
  canonicalCandidate.lockMode = DevelopmentUltraLockMode3D::QualityLocked;
  const bool knownLock =
      source.lockMode == DevelopmentUltraLockMode3D::Adaptive ||
      source.lockMode == DevelopmentUltraLockMode3D::QualityLocked;
  if (!knownLock || !KnownAction(state.lastAction) ||
      !IsCanonicalDevelopmentUltraQuality3D(canonicalCandidate) ||
      profile.lockMode == DevelopmentUltraLockMode3D::QualityLocked) {
    state.consecutiveUnderBudgetWindows = 0;
    state.consecutiveOverBudgetWindows = 0;
    return state.lastAction = DevelopmentUltraAction3D::Hold;
  }
  const double budget = 1'000'000'000.0 /
                        double(profile.frameGate.targetFramesPerSecond);
  if (double(sample.gpuMaximumNanoseconds) >
      budget * profile.frameGate.maximumFrameBudgetRatio) {
    state.consecutiveUnderBudgetWindows = 0;
    state.consecutiveOverBudgetWindows = 0;
    return state.lastAction = DevelopmentUltraAction3D::EmergencyLower;
  }
  const bool over =
      double(sample.gpuP95Nanoseconds) >
          budget * profile.frameGate.maximumGpuBudgetRatio ||
      double(sample.gpuP99Nanoseconds) >
          budget * profile.frameGate.maximumP99BudgetRatio;
  const bool under =
      double(sample.gpuP95Nanoseconds) <
          budget * profile.frameGate.minimumGpuBudgetRatio &&
      double(sample.gpuP99Nanoseconds) <=
          budget * profile.frameGate.maximumP99BudgetRatio;
  const auto incrementChecked = [](std::uint32_t value) noexcept {
    return value == (std::numeric_limits<std::uint32_t>::max)()
               ? value
               : value + 1;
  };
  state.consecutiveOverBudgetWindows =
      over ? incrementChecked(state.consecutiveOverBudgetWindows) : 0;
  state.consecutiveUnderBudgetWindows =
      under ? incrementChecked(state.consecutiveUnderBudgetWindows) : 0;
  if (state.consecutiveOverBudgetWindows >=
      profile.frameGate.sustainedWindowCount) {
    if (state.lastAction == DevelopmentUltraAction3D::Lower) {
      return DevelopmentUltraAction3D::Hold;
    }
    return state.lastAction = DevelopmentUltraAction3D::Lower;
  }
  if (state.consecutiveUnderBudgetWindows >=
      profile.frameGate.sustainedWindowCount) {
    if (state.lastAction == DevelopmentUltraAction3D::Raise) {
      return DevelopmentUltraAction3D::Hold;
    }
    return state.lastAction = DevelopmentUltraAction3D::Raise;
  }
  return state.lastAction = DevelopmentUltraAction3D::Hold;
}

DevelopmentUltraReceipt3D StageDevelopmentUltraReceipt3D(
    const DevelopmentUltraQuality3D &profile,
    const DevelopmentUltraAdmission3D &admission, std::uint64_t epoch,
    std::uint64_t downgradeCount) noexcept {
  DevelopmentUltraReceipt3D result{};
  result.requested = profile.mode;
  result.actual = DevelopmentUltraQualityMode3D::High;
  result.requestedFingerprint = FingerprintDevelopmentUltraQuality3D(profile);
  result.normalizedFingerprint =
      FingerprintDevelopmentUltraQuality3D(
          NormalizeDevelopmentUltraQuality3D(profile));
  result.actualFingerprint = 1469598103934665603ull;
  HashValue(result.actualFingerprint, result.actual);
  result.epoch = epoch;
  result.gpuTruth.epoch = epoch;
  result.downgradeCount = downgradeCount;
  result.phase = DevelopmentUltraTransitionPhase3D::Admission;
  result.requestActive = true;
  result.noOp = true;
  result.rendererApplied = false;
  result.transitionPolicyApplied = false;
  const bool admissionConsistent =
      KnownAdmissionReason(admission.reason) &&
      admission.admitted ==
          (admission.reason == DevelopmentUltraAdmissionReason3D::Admitted);
  result.reason = !admissionConsistent
                      ? DevelopmentUltraAdmissionReason3D::InvalidProfile
                      : admission.admitted
                            ? DevelopmentUltraAdmissionReason3D::
                                  RuntimeApplicationNotIntegrated
                             : admission.reason;
  if (!admissionConsistent || !admission.admitted) {
    result.firstFalseReason =
        admissionConsistent &&
                admission.reason ==
                    DevelopmentUltraAdmissionReason3D::PendingGpuReleases
            ? DevelopmentUltraFirstFalseReason3D::
                  PendingGpuReleasesBeforeApply
            : DevelopmentUltraFirstFalseReason3D::AdmissionDenied;
  }
  // 此階段只封閉 profile 與 DXGI admission；未接 renderer 前不得冒稱套用。
  return result;
}

void RecordDevelopmentUltraFirstFalse3D(
    DevelopmentUltraReceipt3D &receipt,
    DevelopmentUltraTransitionPhase3D phase,
    DevelopmentUltraFirstFalseReason3D reason) noexcept {
  if (reason == DevelopmentUltraFirstFalseReason3D::None ||
      receipt.firstFalseReason != DevelopmentUltraFirstFalseReason3D::None) {
    return;
  }
  receipt.phase = phase;
  receipt.firstFalseReason = reason;
}

std::uint64_t NextDevelopmentUltraEpoch3D(std::uint64_t current) noexcept {
  return current == (std::numeric_limits<std::uint64_t>::max)() ? current
                                                                : current + 1;
}

std::uint64_t AddDevelopmentUltraDowngradeCount3D(
    std::uint64_t current, std::uint64_t amount) noexcept {
  return amount > (std::numeric_limits<std::uint64_t>::max)() - current
             ? (std::numeric_limits<std::uint64_t>::max)()
             : current + amount;
}

bool DevelopmentUltraAdapterIdentityMatches3D(
    bool rendererAdapterLuidValid, std::uint32_t rendererAdapterLuidLow,
    std::int32_t rendererAdapterLuidHigh,
    const DxgiVideoMemoryTelemetry3D &telemetry) noexcept {
  // renderer 未提供 identity 或 telemetry 未找到 adapter 時一律 fail-closed。
  return rendererAdapterLuidValid && telemetry.adapterFound &&
         rendererAdapterLuidLow == telemetry.adapterLuidLow &&
         rendererAdapterLuidHigh == telemetry.adapterLuidHigh;
}

void RecordDevelopmentUltraAction3D(DevelopmentUltraReceipt3D &receipt,
                                    DevelopmentUltraAction3D action) noexcept {
  if (!KnownAction(action)) {
    receipt.reason = DevelopmentUltraAdmissionReason3D::InvalidProfile;
    receipt.rendererApplied = false;
    receipt.transitionPolicyApplied = false;
    receipt.shadowResourcesApplied = false;
    receipt.volumetricResolutionApplied = false;
    receipt.cloudSamplesApplied = false;
    receipt.cloudShadowSamplesApplied = false;
    receipt.lightShaftSamplesApplied = false;
    receipt.vegetationDensityApplied = false;
    receipt.textureBudgetApplied = false;
    receipt.meshBudgetApplied = false;
    receipt.gpuTruth = {};
    receipt.gpuTruth.epoch = receipt.epoch;
    receipt.noOp = true;
    return;
  }
  if (action == DevelopmentUltraAction3D::Lower ||
      action == DevelopmentUltraAction3D::EmergencyLower) {
    receipt.downgradeCount =
        AddDevelopmentUltraDowngradeCount3D(receipt.downgradeCount);
  }
}

bool AdvanceDevelopmentUltraGpuTruthSequence3D(
    DevelopmentUltraGpuTruthSequence3D &sequence,
    std::uint64_t submittedFrameSerial,
    std::uint64_t completedFrameSerial,
    std::uint64_t timingSampleSerial, bool framePassed,
    std::uint32_t requiredStableFrames, bool *freshOut) noexcept {
  const bool possible = completedFrameSerial <= submittedFrameSerial &&
                        timingSampleSerial <= submittedFrameSerial;
  const bool fresh = possible &&
                     submittedFrameSerial > sequence.lastSubmittedFrameSerial &&
                     completedFrameSerial > sequence.lastCompletedFrameSerial &&
                     timingSampleSerial > sequence.lastTimingSampleSerial;
  if (freshOut)
    *freshOut = fresh;
  // 每個 observation 都消耗已看見的序號；失敗 sample 不得在下一幀重用。
  if(possible){
    sequence.lastSubmittedFrameSerial =
        (std::max)(sequence.lastSubmittedFrameSerial, submittedFrameSerial);
    sequence.lastCompletedFrameSerial =
        (std::max)(sequence.lastCompletedFrameSerial, completedFrameSerial);
    sequence.lastTimingSampleSerial =
        (std::max)(sequence.lastTimingSampleSerial, timingSampleSerial);
  }
  if (!framePassed || !fresh || !requiredStableFrames) {
    sequence.stableFrames = 0;
    return false;
  }
  if (sequence.stableFrames < requiredStableFrames)
    ++sequence.stableFrames;
  return sequence.stableFrames >= requiredStableFrames;
}

bool DevelopmentUltraPostApplyGate3D(
    bool rendererApplySucceeded,
    std::size_t pendingGpuReleaseBytes) noexcept {
  return rendererApplySucceeded && pendingGpuReleaseBytes == 0;
}

} // namespace Aevrynd
