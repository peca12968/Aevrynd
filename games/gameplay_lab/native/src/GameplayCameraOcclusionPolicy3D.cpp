#include "GameplayCameraOcclusionPolicy3D.h"

#include <algorithm>
#include <cmath>

namespace GameplayLab {
namespace {

[[nodiscard]] bool Finite(float value) noexcept { return std::isfinite(value); }
[[nodiscard]] bool HasId(Aevrynd::StableObjectId3D id) noexcept {
  return id.high != 0 || id.low != 0;
}

[[nodiscard]] bool ValidSweep(const CameraOcclusionSweep3D &sweep,
                              float desiredArm) noexcept {
  if (!sweep.blocked)
    return sweep.hitDistance == 0.f && !HasId(sweep.occluder) &&
           sweep.generation == 0;
  return Finite(sweep.hitDistance) && sweep.hitDistance >= 0.f &&
         sweep.hitDistance <= desiredArm && HasId(sweep.occluder) &&
         sweep.generation != 0;
}

[[nodiscard]] float AvailableArm(const CameraOcclusionSweep3D &sweep,
                                 float desiredArm) noexcept {
  return sweep.blocked ? std::min(sweep.hitDistance, desiredArm) : desiredArm;
}

[[nodiscard]] float DerivedMinimumArm(const CameraSubjectShape3D &subject,
                                      const CameraLensSafety3D &lens) noexcept {
  const float tangent = std::tan(lens.verticalFovRadians * .5f);
  const float verticalExtent =
      std::max(subject.focusHeight, subject.capsuleHeight - subject.focusHeight);
  return verticalExtent / tangent + lens.nearPlane + subject.capsuleRadius +
         lens.subjectMargin;
}

[[nodiscard]] float MoveTowards(float current, float target,
                                float maximumDelta) noexcept {
  return target > current ? std::min(target, current + maximumDelta)
                          : std::max(target, current - maximumDelta);
}

} // namespace

ProductionCameraFramingArmInterval3D
DeriveProductionCameraFramingArmInterval3D(
    CameraSubjectShape3D subject, CameraLensSafety3D lens, float aspectRatio,
    float maximumArm, float minimumScreenHeight,
    float maximumScreenHeight) noexcept {
  ProductionCameraFramingArmInterval3D result{};
  if (!Finite(subject.capsuleHeight) || subject.capsuleHeight <= 0.f ||
      !Finite(subject.capsuleRadius) || subject.capsuleRadius <= 0.f ||
      subject.capsuleHeight < subject.capsuleRadius * 2.f ||
      !Finite(subject.focusHeight) || subject.focusHeight < 0.f ||
      subject.focusHeight > subject.capsuleHeight || !Finite(lens.nearPlane) ||
      lens.nearPlane <= 0.f || !Finite(lens.verticalFovRadians) ||
      lens.verticalFovRadians <= 0.f ||
      lens.verticalFovRadians >= 3.14159265f ||
      !Finite(lens.subjectMargin) || lens.subjectMargin < 0.f ||
      !Finite(aspectRatio) || aspectRatio <= 0.f || !Finite(maximumArm) ||
      maximumArm <= 0.f || !Finite(minimumScreenHeight) ||
      !Finite(maximumScreenHeight) || minimumScreenHeight <= 0.f ||
      maximumScreenHeight > 1.f || minimumScreenHeight > maximumScreenHeight)
    return result;
  const float tangent = std::tan(lens.verticalFovRadians * .5f);
  const float safeArm = DerivedMinimumArm(subject, lens);
  const float horizontalTangent = tangent * aspectRatio;
  const float framingNear =
      subject.capsuleHeight / (2.f * maximumScreenHeight * tangent);
  const float framingFar =
      subject.capsuleHeight / (2.f * minimumScreenHeight * tangent);
  const float horizontalNear = subject.capsuleRadius / (.9f * horizontalTangent);
  result.nearArm = std::max({safeArm, framingNear, horizontalNear});
  result.farArm = std::min(maximumArm, framingFar);
  result.minimumSubjectArm = safeArm;
  if (!Finite(result.nearArm) || !Finite(result.farArm) ||
      result.nearArm > result.farArm)
    return result;
  result.midArm = (result.nearArm + result.farArm) * .5f;
  result.valid = true;
  return result;
}

GameplayCameraOcclusionPolicy3D::GameplayCameraOcclusionPolicy3D(
    CameraOcclusionPolicySettings3D settings) noexcept
    : settings_(settings) {}

void GameplayCameraOcclusionPolicy3D::Reset() noexcept {
  fadeObject_ = {};
  fadeGeneration_ = 0;
  arm_ = 0.f;
  fadeAlpha_ = 1.f;
  shoulder_ = CameraShoulder3D::Center;
  candidateId_ = {};
  yawOffsetRadians_ = 0.f;
  pitchRadians_ = 0.f;
  verticalElevation_ = 0.f;
  initialized_ = false;
  fading_ = false;
}

CameraOcclusionPolicyOutput3D GameplayCameraOcclusionPolicy3D::Step(
    const CameraOcclusionPolicyInput3D &input) noexcept {
  CameraOcclusionPolicyOutput3D output{};
  const auto &subject = input.subject;
  const auto &lens = input.lens;
  const bool settingsValid =
      Finite(settings_.armRecoveryUnitsPerSecond) &&
      settings_.armRecoveryUnitsPerSecond > 0.f && Finite(settings_.fadeOpacity) &&
      Finite(settings_.shoulderSwitchAdvantage) &&
      settings_.shoulderSwitchAdvantage >= 0.f &&
      settings_.fadeOpacity >= 0.f && settings_.fadeOpacity <= 1.f &&
      Finite(settings_.fadeInSeconds) && settings_.fadeInSeconds > 0.f &&
      Finite(settings_.fadeOutSeconds) && settings_.fadeOutSeconds > 0.f &&
      Finite(settings_.maximumDeltaTime) && settings_.maximumDeltaTime > 0.f;
  const bool inputValid =
      Finite(subject.capsuleHeight) && subject.capsuleHeight > 0.f &&
      Finite(subject.capsuleRadius) && subject.capsuleRadius > 0.f &&
      subject.capsuleHeight >= subject.capsuleRadius * 2.f &&
      Finite(subject.focusHeight) && subject.focusHeight >= 0.f &&
      subject.focusHeight <= subject.capsuleHeight && Finite(lens.nearPlane) &&
      lens.nearPlane > 0.f && Finite(lens.verticalFovRadians) &&
      lens.verticalFovRadians > 0.f &&
      lens.verticalFovRadians < 3.14159265f && Finite(lens.subjectMargin) &&
      lens.subjectMargin >= 0.f && Finite(input.desiredArm) &&
      input.desiredArm > 0.f && Finite(input.deltaTime) &&
      input.deltaTime >= 0.f && ValidSweep(input.center, input.desiredArm) &&
      ValidSweep(input.right, input.desiredArm) &&
      ValidSweep(input.left, input.desiredArm);
  if (!settingsValid || !inputValid)
    return output;

  // 以焦點上下最大的膠囊範圍推導完整角色進入垂直 FOV 所需的安全距離。
  const float minimumArm = DerivedMinimumArm(subject, lens);
  if (!Finite(minimumArm) || minimumArm <= 0.f ||
      minimumArm > input.desiredArm)
    return output;

  const float centerArm = AvailableArm(input.center, input.desiredArm);
  const float rightArm = AvailableArm(input.right, input.desiredArm);
  const float leftArm = AvailableArm(input.left, input.desiredArm);
  CameraShoulder3D shoulder = CameraShoulder3D::Center;
  const CameraOcclusionSweep3D *chosen = &input.center;
  float available = centerArm;

  // 中心安全時立即重設；遮擋時只有另一肩具明確優勢才跨側，避免逐幀跳動。
  if (centerArm < minimumArm) {
    if (shoulder_ == CameraShoulder3D::Right) {
      shoulder = leftArm > rightArm + settings_.shoulderSwitchAdvantage
                     ? CameraShoulder3D::Left
                     : CameraShoulder3D::Right;
    } else if (shoulder_ == CameraShoulder3D::Left) {
      shoulder = rightArm > leftArm + settings_.shoulderSwitchAdvantage
                     ? CameraShoulder3D::Right
                     : CameraShoulder3D::Left;
    } else {
      shoulder = rightArm >= leftArm ? CameraShoulder3D::Right
                                     : CameraShoulder3D::Left;
    }
    chosen = shoulder == CameraShoulder3D::Right ? &input.right : &input.left;
    available = shoulder == CameraShoulder3D::Right ? rightArm : leftArm;
  }
  shoulder_ = shoulder;

  const float targetArm = std::max(minimumArm, available);
  if (!initialized_) {
    arm_ = input.center.blocked || chosen->blocked ? targetArm : input.desiredArm;
    initialized_ = true;
  } else if (targetArm < arm_) {
    arm_ = targetArm;
  } else {
    const float boundedDt = std::min(input.deltaTime, settings_.maximumDeltaTime);
    arm_ = std::min(targetArm,
                    arm_ + settings_.armRecoveryUnitsPerSecond * boundedDt);
  }

  const bool needsFade = chosen->blocked && available < minimumArm;
  if (needsFade) {
    if (!fading_ || fadeObject_ != chosen->occluder ||
        fadeGeneration_ != chosen->generation) {
      fadeObject_ = chosen->occluder;
      fadeGeneration_ = chosen->generation;
      fadeAlpha_ = 1.f;
    }
    fading_ = true;
    const float boundedDt = std::min(input.deltaTime, settings_.maximumDeltaTime);
    const float amount =
        (1.f - settings_.fadeOpacity) * boundedDt / settings_.fadeInSeconds;
    fadeAlpha_ = std::max(settings_.fadeOpacity, fadeAlpha_ - amount);
  } else if (fading_) {
    const float boundedDt = std::min(input.deltaTime, settings_.maximumDeltaTime);
    const float amount =
        (1.f - settings_.fadeOpacity) * boundedDt / settings_.fadeOutSeconds;
    fadeAlpha_ = std::min(1.f, fadeAlpha_ + amount);
    if (fadeAlpha_ >= 1.f - .000001f) {
      fadeAlpha_ = 1.f;
      fadeObject_ = {};
      fadeGeneration_ = 0;
      fading_ = false;
    }
  }

  output.fadeObject = fadeObject_;
  output.fadeGeneration = fadeGeneration_;
  output.shoulder = shoulder;
  output.arm = arm_;
  output.minimumSubjectArm = minimumArm;
  output.fadeAlpha = fadeAlpha_;
  output.targetFadeOpacity = fading_ ? settings_.fadeOpacity : 1.f;
  output.fadeActive = fading_;
  output.fadeSettled = fading_ &&
                       std::abs(fadeAlpha_ - settings_.fadeOpacity) <= .000001f;
  output.valid = true;
  return output;
}

CameraOcclusionPolicyOutput3D GameplayCameraOcclusionPolicy3D::StepProduction(
    const ProductionCameraOcclusionInput3D &input) noexcept {
  CameraOcclusionPolicyOutput3D output{};
  const auto &subject = input.subject;
  const auto &lens = input.lens;
  const bool globalValid =
      Finite(subject.capsuleHeight) && subject.capsuleHeight > 0.f &&
      Finite(subject.capsuleRadius) && subject.capsuleRadius > 0.f &&
      subject.capsuleHeight >= subject.capsuleRadius * 2.f &&
      Finite(subject.focusHeight) && subject.focusHeight >= 0.f &&
      subject.focusHeight <= subject.capsuleHeight && Finite(lens.nearPlane) &&
      lens.nearPlane > 0.f && Finite(lens.verticalFovRadians) &&
      lens.verticalFovRadians > 0.f && lens.verticalFovRadians < 3.14159265f &&
      Finite(lens.subjectMargin) && lens.subjectMargin >= 0.f &&
      Finite(input.desiredArm) && input.desiredArm > 0.f &&
      Finite(input.deltaTime) && input.deltaTime >= 0.f &&
      input.deltaTime <= .25f &&
      Finite(settings_.armRecoveryUnitsPerSecond) &&
      settings_.armRecoveryUnitsPerSecond > 0.f &&
      Finite(settings_.shoulderSwitchAdvantage) &&
      settings_.shoulderSwitchAdvantage >= 0.f &&
      Finite(settings_.fadeOpacity) && settings_.fadeOpacity >= 0.f &&
      settings_.fadeOpacity <= 1.f && Finite(settings_.fadeInSeconds) &&
      settings_.fadeInSeconds > 0.f && Finite(settings_.fadeOutSeconds) &&
      settings_.fadeOutSeconds > 0.f && Finite(settings_.maximumDeltaTime) &&
      settings_.maximumDeltaTime > 0.f && settings_.maximumDeltaTime <= .25f &&
      Finite(settings_.candidateSwitchAdvantage) &&
      settings_.candidateSwitchAdvantage >= 0.f &&
      Finite(settings_.yawRateRadiansPerSecond) &&
      settings_.yawRateRadiansPerSecond > 0.f &&
      Finite(settings_.pitchRateRadiansPerSecond) &&
      settings_.pitchRateRadiansPerSecond > 0.f &&
      Finite(settings_.elevationRateUnitsPerSecond) &&
      settings_.elevationRateUnitsPerSecond > 0.f;
  const float minimumArm = DerivedMinimumArm(subject, lens);
  if (!globalValid || !Finite(minimumArm) || minimumArm <= 0.f ||
      minimumArm > input.desiredArm)
    return output;

  struct Evaluation {
    const ProductionCameraCandidate3D *candidate{};
    std::size_t index{};
    float available{}, offsetCost{};
    bool clear{}, safe{}, framed{};
  };
  auto validCandidate = [&](const ProductionCameraCandidate3D &candidate) {
    return HasId(candidate.candidateId) && Finite(candidate.cameraFocus.x) &&
           Finite(candidate.cameraFocus.y) && Finite(candidate.cameraFocus.z) &&
           ValidSweep(candidate.environmentSphereSweep, input.desiredArm) &&
           Finite(candidate.yawOffsetRadians) && Finite(candidate.pitchRadians) &&
           Finite(candidate.verticalElevation) &&
           Finite(candidate.springArmDistance) && candidate.springArmDistance > 0.f &&
           candidate.springArmDistance <= input.desiredArm &&
           Finite(candidate.collisionWorldMaxArm) &&
           candidate.collisionWorldMaxArm >= 0.f &&
           Finite(candidate.heightFieldMaxArm) &&
           candidate.heightFieldMaxArm >= 0.f && Finite(candidate.sweepRadius) &&
           candidate.sweepRadius >= subject.capsuleRadius &&
           Finite(candidate.subjectScreenHeight) &&
           candidate.subjectScreenHeight >= 0.f &&
           candidate.subjectScreenHeight <= 1.f && Finite(candidate.headroom) &&
           Finite(candidate.subjectScreenWidth) &&
           candidate.subjectScreenWidth > 0.f &&
           candidate.subjectScreenWidth <= 1.f &&
           Finite(candidate.subjectScreenArea) &&
           candidate.subjectScreenArea >= 0.f &&
           candidate.subjectScreenArea <= 1.f &&
           Finite(candidate.buildingUnionCoverage) && candidate.buildingUnionCoverage>=0.f && candidate.buildingUnionCoverage<=1.f &&
           Finite(candidate.maximumWallCoverage) && candidate.maximumWallCoverage>=0.f && candidate.maximumWallCoverage<=1.f &&
           Finite(candidate.roofCoverage) && candidate.roofCoverage>=0.f && candidate.roofCoverage<=1.f &&
           Finite(candidate.subjectOcclusionCoverage) && candidate.subjectOcclusionCoverage>=0.f && candidate.subjectOcclusionCoverage<=1.f &&
           Finite(candidate.actorMinimumMargin) && candidate.actorMinimumMargin>=0.f && candidate.actorMinimumMargin<=1.f &&
           Finite(candidate.actorGapPixels) && candidate.actorGapPixels>=0.f &&
           candidate.headroom >= 0.f && candidate.headroom <= 1.f;
  };
  auto evaluate = [&](const ProductionCameraCandidate3D &candidate,
                      std::size_t index) {
    Evaluation result{&candidate, index};
    result.available = std::min(
        {candidate.springArmDistance, candidate.collisionWorldMaxArm,
         candidate.heightFieldMaxArm,
         AvailableArm(candidate.environmentSphereSweep, input.desiredArm)});
    result.clear = !candidate.environmentSphereSweep.blocked &&
                   result.available >= candidate.springArmDistance;
    result.safe = result.available >= minimumArm;
    result.framed = candidate.subjectScreenHeight >= .35f &&
                    candidate.subjectScreenHeight <= .70f &&
                    candidate.subjectScreenWidth <= .9f &&
                    candidate.subjectScreenArea >= .02f &&
                    candidate.subjectScreenArea <= .20f &&
                    candidate.feetOnScreen &&
                    candidate.headroom >= .05f && candidate.allActorsOnScreen &&
                    candidate.compositionAdmitted;
    result.offsetCost = candidate.subjectOcclusionCoverage*100.f + candidate.buildingUnionCoverage*4.f + candidate.maximumWallCoverage*8.f + candidate.roofCoverage*8.f + std::abs(candidate.yawOffsetRadians) +
                        std::abs(candidate.pitchRadians) +
                        std::abs(candidate.verticalElevation) * .25f +
                        std::abs(candidate.subjectScreenHeight - .55f) * 4.f;
    return result;
  };
  auto better = [](const Evaluation &left, const Evaluation &right) {
    if (left.clear != right.clear)
      return left.clear;
    if (left.safe != right.safe)
      return left.safe;
    if (left.framed != right.framed)
      return left.framed;
    if (left.offsetCost != right.offsetCost)
      return left.offsetCost < right.offsetCost;
    return left.index < right.index;
  };

  Evaluation best{};
  bool found{};
  Evaluation retained{};
  bool retainedFound{};
  for (std::size_t index = 0; index < input.candidates.size(); ++index) {
    const auto &candidate = input.candidates[index];
    // 屋頂或頂面命中直接淘汰，避免相機解落入室內天花板或地形下方。
    if (candidate.roofOrTopBlocked || !validCandidate(candidate))
      continue;
    const auto current = evaluate(candidate, index);
    const bool compositionAdmitted=candidate.buildingUnionCoverage<=.22f&&candidate.maximumWallCoverage<=.08f&&candidate.roofCoverage<=.05f&&candidate.subjectOcclusionCoverage<=.005f;
    const bool admitted = current.safe && current.framed && compositionAdmitted;
    if (admitted && (!found || better(current, best))) {
      best = current;
      found = true;
    }
    if (admitted && candidate.candidateId == candidateId_) {
      retained = current;
      retainedFound = true;
    }
  }

  if (!found) {
    Reset();
    return output;
  }
  if (retainedFound && retained.clear == best.clear &&
      retained.safe == best.safe && retained.framed == best.framed &&
      retained.offsetCost <=
          best.offsetCost + settings_.candidateSwitchAdvantage)
    best = retained;

  const auto &chosen = *best.candidate;
  const float boundedDt = std::min(input.deltaTime, settings_.maximumDeltaTime);
  const float targetArm = std::max(minimumArm, best.available);
  if (!initialized_) {
    arm_ = targetArm;
    yawOffsetRadians_ = chosen.yawOffsetRadians;
    pitchRadians_ = chosen.pitchRadians;
    verticalElevation_ = chosen.verticalElevation;
    initialized_ = true;
  } else {
    arm_ = targetArm < arm_
               ? targetArm
               : std::min(targetArm, arm_ +
                                         settings_.armRecoveryUnitsPerSecond * boundedDt);
    yawOffsetRadians_ = MoveTowards(yawOffsetRadians_, chosen.yawOffsetRadians,
                                    settings_.yawRateRadiansPerSecond * boundedDt);
    pitchRadians_ = MoveTowards(pitchRadians_, chosen.pitchRadians,
                                settings_.pitchRateRadiansPerSecond * boundedDt);
    verticalElevation_ = MoveTowards(
        verticalElevation_, chosen.verticalElevation,
        settings_.elevationRateUnitsPerSecond * boundedDt);
  }
  candidateId_ = chosen.candidateId;
  shoulder_ = chosen.shoulder;

  if (best.clear) {
    fadeObject_ = {};
    fadeGeneration_ = 0;
    fadeAlpha_ = 1.f;
    fading_ = false;
  } else if (chosen.environmentSphereSweep.blocked) {
    if (!fading_ || fadeObject_ != chosen.environmentSphereSweep.occluder ||
        fadeGeneration_ != chosen.environmentSphereSweep.generation) {
      fadeObject_ = chosen.environmentSphereSweep.occluder;
      fadeGeneration_ = chosen.environmentSphereSweep.generation;
      fadeAlpha_ = 1.f;
    }
    fading_ = true;
    const float amount =
        (1.f - settings_.fadeOpacity) * boundedDt / settings_.fadeInSeconds;
    fadeAlpha_ = std::max(settings_.fadeOpacity, fadeAlpha_ - amount);
  } else {
    // World 或 HeightField 限縮沒有可淡出的環境 stable ID，必須清除舊 fade。
    fadeObject_ = {};
    fadeGeneration_ = 0;
    fadeAlpha_ = 1.f;
    fading_ = false;
  }

  output.fadeObject = fadeObject_;
  output.fadeGeneration = fadeGeneration_;
  output.shoulder = chosen.shoulder;
  output.arm = arm_;
  output.minimumSubjectArm = minimumArm;
  output.fadeAlpha = fadeAlpha_;
  output.targetFadeOpacity = fading_ ? settings_.fadeOpacity : 1.f;
  output.fadeActive = fading_;
  output.fadeSettled = fading_ &&
                       std::abs(fadeAlpha_ - settings_.fadeOpacity) <= .000001f;
  output.candidateId = chosen.candidateId;
  output.candidateIndex = best.index;
  output.yawOffsetRadians = yawOffsetRadians_;
  output.pitchRadians = pitchRadians_;
  output.verticalElevation = verticalElevation_;
  output.clearLineOfSight = best.clear;
  output.subjectFramed = best.framed;
  output.valid = true;
  return output;
}

} // namespace GameplayLab
