#include <Aevrynd/DevelopmentUltraQuality3D.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>

using namespace Aevrynd;

namespace {

DxgiVideoMemoryTelemetry3D MockTelemetry() {
  DxgiVideoMemoryTelemetry3D result{};
  result.adapterFound = true;
  result.hardwareAdapter = true;
  result.adapterLuidLow = 7;
  result.adapterLuidHigh = 11;
  result.dedicatedVideoMemoryBytes = 16ull * 1024ull * 1024ull * 1024ull;
  result.local = {true, 15ull * 1024ull * 1024ull * 1024ull,
                  2ull * 1024ull * 1024ull * 1024ull,
                  12ull * 1024ull * 1024ull * 1024ull, 0};
  result.nonLocal = {true, 8ull * 1024ull * 1024ull * 1024ull, 0,
                     7ull * 1024ull * 1024ull * 1024ull, 0};
  result.queryEpoch = 3;
  result.sampledAtSteadyNanoseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
  return result;
}

} // namespace

int main() {
  const auto fullHd =
      MakeDevelopmentUltraQuality3D(DevelopmentUltraResolutionTier3D::FullHd);
  const auto quadHd =
      MakeDevelopmentUltraQuality3D(DevelopmentUltraResolutionTier3D::QuadHd);
  if (!IsCanonicalDevelopmentUltraQuality3D(fullHd) ||
      !IsCanonicalDevelopmentUltraQuality3D(quadHd) ||
      fullHd.lockMode != DevelopmentUltraLockMode3D::QualityLocked ||
      fullHd.resolution.width != 1920 || fullHd.resolution.height != 1080 ||
      fullHd.resolution.volumetricResolutionScale != .75f ||
      fullHd.resolution.cloudSampleCount != 28 ||
      fullHd.resolution.cloudShadowSampleCount != 7 ||
      fullHd.resolution.lightShaftSampleCount != 10 ||
      fullHd.resolution.vegetationDensityScale != 1.5f ||
      quadHd.resolution.width != 2560 || quadHd.resolution.height != 1440 ||
      quadHd.resolution.volumetricResolutionScale != .625f ||
      quadHd.resolution.cloudSampleCount != 24 ||
      quadHd.resolution.cloudShadowSampleCount != 6 ||
      quadHd.resolution.lightShaftSampleCount != 8 ||
      quadHd.resolution.vegetationDensityScale != 1.25f ||
      fullHd.shadowCascadeCount != 4 || fullHd.shadowResolution != 4096 ||
      fullHd.textureBudgetBytes != 2ull * 1024ull * 1024ull * 1024ull ||
      fullHd.meshBudgetBytes != 1ull * 1024ull * 1024ull * 1024ull ||
      !fullHd.transition.waitForFenceBeforeApply ||
      !fullHd.transition.requirePendingReleasesZero ||
      !fullHd.transition.trimTransientMemoryOnExit ||
      fullHd.transition.stableFramesBeforeCommit != 3 ||
      fullHd.transition.maximumFlushesPerTransition != 1 ||
      fullHd.runtimeApplicationEnabled) {
    return 1;
  }
  auto highAlias = fullHd;
  highAlias.mode = DevelopmentUltraQualityMode3D::High;
  auto swapped = fullHd;
  std::swap(swapped.resolution.cloudSampleCount,
            swapped.resolution.lightShaftSampleCount);
  if (IsCanonicalDevelopmentUltraQuality3D(highAlias) ||
      IsCanonicalDevelopmentUltraQuality3D(swapped) ||
      FingerprintDevelopmentUltraQuality3D(highAlias) ==
          FingerprintDevelopmentUltraQuality3D(fullHd) ||
      FingerprintDevelopmentUltraQuality3D(swapped) ==
          FingerprintDevelopmentUltraQuality3D(fullHd)) {
    return 2;
  }
  auto unknownMode = fullHd;
  unknownMode.mode = static_cast<DevelopmentUltraQualityMode3D>(255);
  auto unknownTier = fullHd;
  unknownTier.tier = static_cast<DevelopmentUltraResolutionTier3D>(255);
  auto telemetry = MockTelemetry();
  if(!DevelopmentUltraAdapterIdentityMatches3D(true,telemetry.adapterLuidLow,
                                               telemetry.adapterLuidHigh,telemetry)||
     DevelopmentUltraAdapterIdentityMatches3D(false,telemetry.adapterLuidLow,
                                              telemetry.adapterLuidHigh,telemetry)||
     DevelopmentUltraAdapterIdentityMatches3D(true,telemetry.adapterLuidLow+1,
                                              telemetry.adapterLuidHigh,telemetry)){
    return 30;
  }
  auto identityUnavailable=telemetry;identityUnavailable.adapterFound=false;
  if(DevelopmentUltraAdapterIdentityMatches3D(true,telemetry.adapterLuidLow,
                                              telemetry.adapterLuidHigh,
                                              identityUnavailable)){
    return 31;
  }
  const DevelopmentUltraAdmissionInput3D valid{
      512ull * 1024ull * 1024ull, 1024ull * 1024ull * 1024ull, 0};
  const auto admitted =
      EvaluateDevelopmentUltraAdmission3D(fullHd, telemetry, valid);
  if (!admitted.admitted ||
      admitted.hardVramBudgetBytes != 10ull * 1024ull * 1024ull * 1024ull) {
    return 3;
  }
  if (EvaluateDevelopmentUltraAdmission3D(unknownMode, telemetry, valid).reason !=
          DevelopmentUltraAdmissionReason3D::InvalidProfile ||
      EvaluateDevelopmentUltraAdmission3D(unknownTier, telemetry, valid).reason !=
          DevelopmentUltraAdmissionReason3D::InvalidProfile) {
    return 17;
  }
  auto unavailable = telemetry;
  unavailable.local.querySucceeded = false;
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, unavailable, valid).reason !=
      DevelopmentUltraAdmissionReason3D::TelemetryUnavailable) {
    return 4;
  }
  auto stale = telemetry;
  stale.ageNanoseconds = valid.maximumTelemetryAgeNanoseconds + 1;
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, stale, valid).reason !=
      DevelopmentUltraAdmissionReason3D::TelemetryStale) {
    return 5;
  }
  auto missingTimestamp = telemetry;
  missingTimestamp.sampledAtSteadyNanoseconds = 0;
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, missingTimestamp, valid).reason !=
      DevelopmentUltraAdmissionReason3D::TelemetryUnavailable) {
    return 12;
  }
  auto futureTimestamp = telemetry;
  futureTimestamp.sampledAtSteadyNanoseconds =
      (std::numeric_limits<std::uint64_t>::max)();
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, futureTimestamp, valid).reason !=
      DevelopmentUltraAdmissionReason3D::TelemetryStale) {
    return 13;
  }
  auto pending = valid;
  pending.pendingGpuReleaseBytes = 1;
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, telemetry, pending).reason !=
      DevelopmentUltraAdmissionReason3D::PendingGpuReleases) {
    return 6;
  }
  auto overflow = valid;
  overflow.currentEngineResidentBytes =
      (std::numeric_limits<std::size_t>::max)();
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, telemetry, overflow).reason !=
      DevelopmentUltraAdmissionReason3D::ArithmeticOverflow) {
    return 7;
  }
  auto noHeadroom = telemetry;
  noHeadroom.local.currentUsageBytes = noHeadroom.local.budgetBytes;
  if (EvaluateDevelopmentUltraAdmission3D(fullHd, noHeadroom, valid).reason !=
      DevelopmentUltraAdmissionReason3D::AdapterHeadroomExceeded) {
    return 8;
  }
  DevelopmentUltraHysteresis3D lockedHysteresis{7, 9,
                                                 DevelopmentUltraAction3D::Lower};
  const DevelopmentUltraFrameSample3D over{15'100'000, 15'900'000,
                                            16'000'000};
  if (UpdateDevelopmentUltraHysteresis3D(fullHd, over, lockedHysteresis) !=
          DevelopmentUltraAction3D::Hold ||
      lockedHysteresis.consecutiveUnderBudgetWindows != 0 ||
      lockedHysteresis.consecutiveOverBudgetWindows != 0) {
    return 9;
  }
  auto adaptive = fullHd;
  adaptive.lockMode = DevelopmentUltraLockMode3D::Adaptive;
  DevelopmentUltraHysteresis3D hysteresis{};
  if (UpdateDevelopmentUltraHysteresis3D(adaptive, over, hysteresis) !=
          DevelopmentUltraAction3D::Hold ||
      UpdateDevelopmentUltraHysteresis3D(adaptive, over, hysteresis) !=
          DevelopmentUltraAction3D::Hold ||
      UpdateDevelopmentUltraHysteresis3D(adaptive, over, hysteresis) !=
          DevelopmentUltraAction3D::Lower) {
    return 14;
  }
  const DevelopmentUltraFrameSample3D emergency{15'000'000, 15'500'000,
                                                 17'000'000};
  if (UpdateDevelopmentUltraHysteresis3D(adaptive, emergency, hysteresis) !=
      DevelopmentUltraAction3D::EmergencyLower) {
    return 10;
  }
  hysteresis.consecutiveOverBudgetWindows =
      (std::numeric_limits<std::uint32_t>::max)();
  if (UpdateDevelopmentUltraHysteresis3D(adaptive, over, hysteresis) !=
          DevelopmentUltraAction3D::Lower ||
      hysteresis.consecutiveOverBudgetWindows !=
          (std::numeric_limits<std::uint32_t>::max)()) {
    return 15;
  }
  auto unknown = fullHd;
  unknown.lockMode = static_cast<DevelopmentUltraLockMode3D>(255);
  hysteresis = {1, 1, DevelopmentUltraAction3D::Raise};
  if (UpdateDevelopmentUltraHysteresis3D(unknown, over, hysteresis) !=
          DevelopmentUltraAction3D::Hold ||
      hysteresis.consecutiveUnderBudgetWindows != 0 ||
      hysteresis.consecutiveOverBudgetWindows != 0) {
    return 16;
  }
  hysteresis = {1, 1, static_cast<DevelopmentUltraAction3D>(255)};
  if (UpdateDevelopmentUltraHysteresis3D(adaptive, over, hysteresis) !=
          DevelopmentUltraAction3D::Hold ||
      hysteresis.lastAction != DevelopmentUltraAction3D::Hold ||
      hysteresis.consecutiveUnderBudgetWindows != 0 ||
      hysteresis.consecutiveOverBudgetWindows != 0) {
    return 18;
  }
  const auto receipt = StageDevelopmentUltraReceipt3D(fullHd, admitted, 42, 3);
  if (receipt.actual != DevelopmentUltraQualityMode3D::High ||
      receipt.reason !=
          DevelopmentUltraAdmissionReason3D::RuntimeApplicationNotIntegrated ||
      !receipt.requestActive || receipt.rendererApplied || receipt.transitionPolicyApplied || !receipt.noOp || receipt.epoch != 42 ||
      receipt.downgradeCount != 3 ||
      receipt.actualFingerprint == 0 ||
      receipt.requestedFingerprint != receipt.normalizedFingerprint ||
      receipt.gpuTruth.epoch != 42 || receipt.gpuTruth.observed ||
      receipt.gpuTruth.resourcesTrue || receipt.gpuTruth.telemetryTrue ||
      receipt.gpuTruth.rendererAdapterIdentityValid ||
      receipt.gpuTruth.adapterIdentityTrue ||
      receipt.gpuTruth.validationTrue || receipt.gpuTruth.frameBudgetTrue ||
      receipt.gpuTruth.fresh || receipt.gpuTruth.stableFrames) {
    return 11;
  }
  DevelopmentUltraAdmission3D unknownAdmission{};
  unknownAdmission.reason =
      static_cast<DevelopmentUltraAdmissionReason3D>(255);
  if (StageDevelopmentUltraReceipt3D(fullHd, unknownAdmission, 43).reason !=
      DevelopmentUltraAdmissionReason3D::InvalidProfile) {
    return 19;
  }
  DevelopmentUltraAdmission3D inconsistentAdmission{};
  inconsistentAdmission.reason = DevelopmentUltraAdmissionReason3D::Admitted;
  if (StageDevelopmentUltraReceipt3D(fullHd, inconsistentAdmission, 44).reason !=
      DevelopmentUltraAdmissionReason3D::InvalidProfile) {
    return 20;
  }
  if (NextDevelopmentUltraEpoch3D(
          (std::numeric_limits<std::uint64_t>::max)()) !=
          (std::numeric_limits<std::uint64_t>::max)() ||
      AddDevelopmentUltraDowngradeCount3D(
          (std::numeric_limits<std::uint64_t>::max)(), 1) !=
          (std::numeric_limits<std::uint64_t>::max)()) {
    return 21;
  }
  auto saturatedReceipt = receipt;
  saturatedReceipt.downgradeCount =
      (std::numeric_limits<std::uint64_t>::max)();
  RecordDevelopmentUltraAction3D(saturatedReceipt,
                                 DevelopmentUltraAction3D::Lower);
  if (saturatedReceipt.downgradeCount !=
      (std::numeric_limits<std::uint64_t>::max)()) {
    return 22;
  }
  saturatedReceipt.gpuTruth.observed = true;
  saturatedReceipt.gpuTruth.resourcesTrue = true;
  saturatedReceipt.gpuTruth.committed = true;
  saturatedReceipt.gpuTruth.stableFrames = 3;
  RecordDevelopmentUltraAction3D(
      saturatedReceipt, static_cast<DevelopmentUltraAction3D>(255));
  if (saturatedReceipt.reason !=
          DevelopmentUltraAdmissionReason3D::InvalidProfile ||
      saturatedReceipt.rendererApplied ||
      saturatedReceipt.transitionPolicyApplied || !saturatedReceipt.noOp ||
      saturatedReceipt.gpuTruth.epoch != saturatedReceipt.epoch ||
      saturatedReceipt.gpuTruth.observed ||
      saturatedReceipt.gpuTruth.resourcesTrue ||
      saturatedReceipt.gpuTruth.committed ||
      saturatedReceipt.gpuTruth.stableFrames) {
    return 23;
  }
  DevelopmentUltraGpuTruthSequence3D sequence{};
  bool fresh=true;
  // 首幀只有提交，尚無完成 timestamp，必須留下 fail-closed 的 first false。
  if (AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 1, 0, 0, false, 3,
                                                 &fresh) ||
      fresh || sequence.stableFrames != 0 ||
      AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 2, 1, 1, true, 3,
                                                 &fresh) ||
      !fresh || sequence.stableFrames != 1 ||
      AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 3, 2, 2, true, 3,
                                                 &fresh) ||
      !fresh || sequence.stableFrames != 2 ||
      !AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 4, 3, 3, true, 3,
                                                  &fresh) ||
      !fresh || sequence.stableFrames != 3) {
    return 24;
  }
  // 間歇性失敗與重複序號都必須清掉連續三幀證據。
  if (AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 5, 4, 4, false, 3,
                                                &fresh) ||
      !fresh || sequence.stableFrames != 0 ||
      AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 5, 4, 4, true, 3,
                                                &fresh) ||
      fresh || sequence.stableFrames != 0 ||
      AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 6, 5, 5, true, 3) ||
      AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 7, 6, 6, true, 3) ||
      !AdvanceDevelopmentUltraGpuTruthSequence3D(sequence, 8, 7, 7, true, 3)) {
    return 26;
  }
  // 新 epoch 以舊 epoch 的 submitted 上界為 baseline，不接受延遲 publish 的舊 sample。
  DevelopmentUltraGpuTruthSequence3D epochSequence{100,100,80,0};
  if(AdvanceDevelopmentUltraGpuTruthSequence3D(epochSequence,101,100,81,true,3,
                                               &fresh)||fresh||
     AdvanceDevelopmentUltraGpuTruthSequence3D(epochSequence,102,101,82,true,3,
                                               &fresh)||!fresh||
     epochSequence.stableFrames!=1){
    return 28;
  }
  DevelopmentUltraGpuTruthSequence3D impossibleSequence{};
  if(AdvanceDevelopmentUltraGpuTruthSequence3D(impossibleSequence,9,10,1,true,3,
                                               &fresh)||fresh||
     impossibleSequence.stableFrames||impossibleSequence.lastSubmittedFrameSerial||
     impossibleSequence.lastCompletedFrameSerial||
     impossibleSequence.lastTimingSampleSerial||
     AdvanceDevelopmentUltraGpuTruthSequence3D(impossibleSequence,9,1,10,true,3,
                                               &fresh)||fresh||
     impossibleSequence.stableFrames||
     AdvanceDevelopmentUltraGpuTruthSequence3D(impossibleSequence,1,1,1,true,3,
                                               &fresh)||!fresh||
     impossibleSequence.stableFrames!=1){
    return 29;
  }
  const auto nextEpochReceipt=StageDevelopmentUltraReceipt3D(fullHd,admitted,43);
  if(nextEpochReceipt.gpuTruth.epoch!=43||nextEpochReceipt.gpuTruth.observed||
     nextEpochReceipt.gpuTruth.committed||nextEpochReceipt.gpuTruth.stableFrames){
    return 27;
  }
  if (!DevelopmentUltraPostApplyGate3D(true, 0) ||
      DevelopmentUltraPostApplyGate3D(true, 1) ||
      DevelopmentUltraPostApplyGate3D(false, 0)) {
    return 25;
  }
  std::cout << "developmentUltraFingerprint="
            << FingerprintDevelopmentUltraQuality3D(fullHd)
            << " hardVramBytes=" << admitted.hardVramBudgetBytes
            << " rendererApplied=" << receipt.rendererApplied << '\n';
  return 0;
}
