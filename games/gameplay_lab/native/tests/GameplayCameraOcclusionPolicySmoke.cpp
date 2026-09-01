#include "GameplayCameraOcclusionPolicy3D.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
constexpr StableObjectId3D Wall{7, 11};
constexpr StableObjectId3D Tree{7, 12};

CameraOcclusionPolicyInput3D Clear(float dt = 1.f / 60.f) {
  CameraOcclusionPolicyInput3D input{};
  input.deltaTime = dt;
  return input;
}

CameraOcclusionSweep3D Hit(float distance, StableObjectId3D id = Wall) {
  return {distance, id, 1, true};
}

bool Near(float left, float right, float epsilon = .001f) {
  return std::abs(left - right) <= epsilon;
}

float SimulateRecovery(float hz) {
  GameplayCameraOcclusionPolicy3D policy;
  auto blocked = Clear(1.f / hz);
  blocked.center = Hit(1.f);
  blocked.right = Hit(1.f);
  blocked.left = Hit(1.f);
  if (!policy.Step(blocked).valid)
    return -1.f;
  auto clear = Clear(1.f / hz);
  const int steps = static_cast<int>(hz);
  CameraOcclusionPolicyOutput3D output{};
  for (int index = 0; index < steps; ++index)
    output = policy.Step(clear);
  return output.arm;
}

ProductionCameraCandidate3D Candidate(std::uint64_t id, float yaw = 0.f,
                                      float pitch = 0.f,
                                      float elevation = 0.f) {
  ProductionCameraCandidate3D candidate{};
  candidate.candidateId = {9, id};
  candidate.yawOffsetRadians = yaw;
  candidate.pitchRadians = pitch;
  candidate.verticalElevation = elevation;
  return candidate;
}

float SimulateProductionTransition(float hz) {
  GameplayCameraOcclusionPolicy3D policy;
  std::array candidates{Candidate(1)};
  ProductionCameraOcclusionInput3D input{};
  input.candidates = candidates;
  input.deltaTime = 1.f / hz;
  if (!policy.StepProduction(input).valid)
    return -1.f;
  candidates[0].yawOffsetRadians = 1.f;
  CameraOcclusionPolicyOutput3D output{};
  for (int frame = 0; frame < static_cast<int>(hz); ++frame)
    output = policy.StepProduction(input);
  return output.yawOffsetRadians;
}
} // namespace

int main() {
  std::uint64_t gates{};

  GameplayCameraOcclusionPolicy3D clearPolicy;
  const auto clear = clearPolicy.Step(Clear());
  if (!clear.valid || !Near(clear.arm, 4.f) || clear.fadeActive ||
      clear.shoulder != CameraShoulder3D::Center)
    return 1;
  ++gates;

  GameplayCameraOcclusionPolicy3D wallPolicy;
  auto wall = Clear();
  wall.center = Hit(3.5f);
  const auto shortened = wallPolicy.Step(wall);
  if (!shortened.valid || !Near(shortened.arm, 3.5f) ||
      shortened.arm < shortened.minimumSubjectArm)
    return 2;
  ++gates;

  GameplayCameraOcclusionPolicy3D shoulderPolicy;
  auto tooClose = Clear();
  tooClose.center = Hit(.1f);
  tooClose.right = {};
  tooClose.left = Hit(.2f, Tree);
  const auto shoulder = shoulderPolicy.Step(tooClose);
  if (!shoulder.valid || shoulder.shoulder != CameraShoulder3D::Right ||
      !Near(shoulder.arm, 4.f) || shoulder.fadeActive)
    return 3;
  ++gates;

  GameplayCameraOcclusionPolicy3D tiePolicy;
  auto tie = Clear();
  tie.center = Hit(.1f);
  tie.right = Hit(.2f, Wall);
  tie.left = Hit(.2f, Tree);
  const auto tied = tiePolicy.Step(tie);
  if (!tied.valid || tied.shoulder != CameraShoulder3D::Right ||
      tied.fadeObject != Wall || tied.fadeGeneration != 1)
    return 4;
  ++gates;

  GameplayCameraOcclusionPolicy3D shoulderHysteresisPolicy;
  auto nearRight = Clear();
  nearRight.center = Hit(.1f);
  nearRight.right = Hit(3.3f, Wall);
  nearRight.left = Hit(3.2f, Tree);
  const auto retainedRight = shoulderHysteresisPolicy.Step(nearRight);
  auto nearLeft = nearRight;
  nearLeft.right = Hit(3.2f, Wall);
  nearLeft.left = Hit(3.3f, Tree);
  const auto stillRight = shoulderHysteresisPolicy.Step(nearLeft);
  nearLeft.left = Hit(3.5f, Tree);
  const auto switchedLeft = shoulderHysteresisPolicy.Step(nearLeft);
  const auto resetCenter = shoulderHysteresisPolicy.Step(Clear());
  if (retainedRight.shoulder != CameraShoulder3D::Right ||
      stillRight.shoulder != CameraShoulder3D::Right ||
      switchedLeft.shoulder != CameraShoulder3D::Left ||
      resetCenter.shoulder != CameraShoulder3D::Center)
    return 5;
  ++gates;

  GameplayCameraOcclusionPolicy3D recoveryPolicy;
  auto blocked = Clear();
  blocked.center = Hit(1.f);
  blocked.right = Hit(1.f);
  blocked.left = Hit(1.f);
  const auto initial = recoveryPolicy.Step(blocked);
  const auto recovered = recoveryPolicy.Step(Clear(1.f / 60.f));
  if (!initial.valid || !Near(initial.arm, initial.minimumSubjectArm) ||
      !(recovered.arm > initial.arm && recovered.arm < 4.f))
    return 6;
  ++gates;

  const float arm30 = SimulateRecovery(30.f);
  const float arm60 = SimulateRecovery(60.f);
  const float arm144 = SimulateRecovery(144.f);
  if (!Near(arm30, 4.f) || !Near(arm30, arm60) || !Near(arm60, arm144))
    return 7;
  ++gates;

  GameplayCameraOcclusionPolicy3D fadePolicy;
  auto fadeInput = Clear();
  fadeInput.center = Hit(.1f, Tree);
  fadeInput.right = Hit(.1f, Tree);
  fadeInput.left = Hit(.1f, Wall);
  const auto entering = fadePolicy.Step(fadeInput);
  if (!entering.valid || !entering.fadeActive || entering.fadeObject != Tree ||
      entering.fadeGeneration != 1 ||
      !(entering.fadeAlpha < 1.f && entering.fadeAlpha > .18f))
    return 8;
  CameraOcclusionPolicyOutput3D settled = entering;
  int settleFrames = 1;
  while (!settled.fadeSettled && settleFrames <= 12) {
    settled = fadePolicy.Step(fadeInput);
    ++settleFrames;
  }
  if (!settled.fadeSettled || settleFrames > 12 ||
      !Near(settled.fadeAlpha, settled.targetFadeOpacity))
    return 18;
  const float stableAlpha = settled.fadeAlpha;
  for (int frame = 0; frame < 60; ++frame)
    settled = fadePolicy.Step(fadeInput);
  if (!Near(stableAlpha, settled.fadeAlpha, .01f))
    return 19;
  auto exiting = fadePolicy.Step(Clear());
  if (!exiting.valid || !exiting.fadeActive || exiting.fadeObject != Tree ||
      exiting.fadeGeneration != 1 || !(exiting.fadeAlpha > stableAlpha))
    return 9;
  int recoveryFrames = 1;
  for (; recoveryFrames < 12 && exiting.fadeActive; ++recoveryFrames)
    exiting = fadePolicy.Step(Clear());
  if (exiting.fadeActive || exiting.fadeObject != StableObjectId3D{} ||
      exiting.fadeGeneration != 0 || !Near(exiting.fadeAlpha, 1.f) ||
      recoveryFrames > 12)
    return 10;
  ++gates;

  GameplayCameraOcclusionPolicy3D invalidPolicy;
  auto invalid = Clear();
  invalid.desiredArm = std::numeric_limits<float>::quiet_NaN();
  if (invalidPolicy.Step(invalid).valid)
    return 11;
  invalid = Clear();
  invalid.center = Hit(std::numeric_limits<float>::infinity());
  if (invalidPolicy.Step(invalid).valid)
    return 12;
  invalid = Clear();
  invalid.center = Hit(1.f);
  invalid.center.generation = 0;
  if (invalidPolicy.Step(invalid).valid)
    return 14;
  invalid = Clear();
  invalid.center.occluder = Tree;
  invalid.center.generation = 1;
  if (invalidPolicy.Step(invalid).valid)
    return 17;
  GameplayCameraOcclusionPolicy3D generationPolicy;
  auto firstGeneration = Clear();
  firstGeneration.center = Hit(.1f, Tree);
  firstGeneration.right = Hit(.1f, Tree);
  firstGeneration.left = Hit(.1f, Tree);
  if (generationPolicy.Step(firstGeneration).fadeGeneration != 1)
    return 15;
  firstGeneration.center.generation = 2;
  firstGeneration.right.generation = 2;
  firstGeneration.left.generation = 2;
  const auto secondGeneration = generationPolicy.Step(firstGeneration);
  if (secondGeneration.fadeObject != Tree || secondGeneration.fadeGeneration != 2 ||
      !(secondGeneration.fadeAlpha < 1.f))
    return 16;
  if (!invalidPolicy.Step(Clear()).valid)
    return 13;
  ++gates;

  // 建築牆與屋頂：屋頂候選 fail-fast，選擇有完整 LOS 的抬升候選。
  GameplayCameraOcclusionPolicy3D buildingPolicy;
  std::array building{Candidate(101), Candidate(102, 0.f, .25f, 1.2f),
                      Candidate(103, 0.f, .5f, 2.f)};
  building[0].environmentSphereSweep = Hit(1.f, Wall);
  building[2].roofOrTopBlocked = true;
  ProductionCameraOcclusionInput3D production{};
  production.candidates = building;
  const auto buildingResult = buildingPolicy.StepProduction(production);
  if (!buildingResult.valid || !buildingResult.clearLineOfSight ||
      buildingResult.candidateId != StableObjectId3D{9, 102} ||
      buildingResult.fadeActive || !Near(buildingResult.fadeAlpha, 1.f))
    return 20;
  ++gates;

  // 角落與左右對稱：clear LOS 優先，完全對稱時固定選較小 index。
  GameplayCameraOcclusionPolicy3D cornerPolicy;
  std::array corner{Candidate(201, -.35f), Candidate(202, .35f)};
  corner[0].environmentSphereSweep = Hit(2.f, Wall);
  corner[0].shoulder = CameraShoulder3D::Left;
  corner[1].shoulder = CameraShoulder3D::Right;
  production.candidates = corner;
  const auto cornerResult = cornerPolicy.StepProduction(production);
  if (cornerResult.candidateId != StableObjectId3D{9, 202} ||
      cornerResult.shoulder != CameraShoulder3D::Right)
    return 21;
  GameplayCameraOcclusionPolicy3D symmetryPolicy;
  corner[0].environmentSphereSweep = {};
  const auto symmetry = symmetryPolicy.StepProduction(production);
  if (symmetry.candidateIndex != 0 ||
      symmetry.candidateId != StableObjectId3D{9, 201})
    return 22;
  ++gates;

  // framing 勝過較小偏移，提供可用的 elevation/pitch 解。
  GameplayCameraOcclusionPolicy3D elevationPolicy;
  std::array elevation{Candidate(301), Candidate(302, .1f, .2f, .8f)};
  elevation[0].subjectScreenHeight = .8f;
  elevation[0].headroom = .02f;
  production.candidates = elevation;
  const auto elevationResult = elevationPolicy.StepProduction(production);
  if (elevationResult.candidateId != StableObjectId3D{9, 302} ||
      !elevationResult.subjectFramed ||
      !Near(elevationResult.verticalElevation, .8f) ||
      !Near(elevationResult.pitchRadians, .2f))
    return 23;
  ++gates;

  GameplayCameraOcclusionPolicy3D noSolutionPolicy;
  elevation[0].roofOrTopBlocked = true;
  elevation[1].roofOrTopBlocked = true;
  const auto noSolution = noSolutionPolicy.StepProduction(production);
  if (noSolution.valid || noSolution.usedFallback || noSolution.fadeActive)
    return 24;
  elevation[0].roofOrTopBlocked = false;
  elevation[0].subjectScreenHeight = .5f;
  elevation[0].headroom = .1f;
  if (!noSolutionPolicy.StepProduction(production).valid)
    return 27;
  elevation[0].roofOrTopBlocked = true;
  const auto lastValidFallback = noSolutionPolicy.StepProduction(production);
  if (lastValidFallback.valid || lastValidFallback.usedFallback ||
      lastValidFallback.fadeActive)
    return 28;
  elevation[0].roofOrTopBlocked = false;
  elevation[0].subjectScreenHeight = .2f;
  elevation[1].subjectScreenHeight = .8f;
  if (noSolutionPolicy.StepProduction(production).valid)
    return 35;
  elevation[0].subjectScreenHeight = .5f;
  elevation[0].subjectScreenArea = .43189f;
  elevation[1].subjectScreenHeight = .5f;
  elevation[1].subjectScreenArea = .43189f;
  if (noSolutionPolicy.StepProduction(production).valid)
    return 36;
  ++gates;

  const float production30 = SimulateProductionTransition(30.f);
  const float production60 = SimulateProductionTransition(60.f);
  const float production144 = SimulateProductionTransition(144.f);
  if (!Near(production30, 1.f) || !Near(production30, production60) ||
      !Near(production60, production144))
    return 25;
  ++gates;

  GameplayCameraOcclusionPolicy3D transitionPolicy;
  std::array transition{Candidate(401, .4f), Candidate(402, .41f)};
  production.candidates = transition;
  production.deltaTime = 1.f / 60.f;
  const auto firstChoice = transitionPolicy.StepProduction(production);
  transition[0].yawOffsetRadians = .41f;
  transition[1].yawOffsetRadians = .4f;
  const auto retainedChoice = transitionPolicy.StepProduction(production);
  transition[0].yawOffsetRadians = .4f;
  transition[1].yawOffsetRadians = 0.f;
  const auto switchedChoice = transitionPolicy.StepProduction(production);
  const float maximumYawStep = 2.4f / 60.f + .0001f;
  if (firstChoice.candidateId != StableObjectId3D{9, 401} ||
      retainedChoice.candidateId != StableObjectId3D{9, 401} ||
      switchedChoice.candidateId != StableObjectId3D{9, 402} ||
      std::abs(switchedChoice.yawOffsetRadians -
               retainedChoice.yawOffsetRadians) > maximumYawStep)
    return 26;
  ++gates;

  // Environment clear、但 world constraint 限縮時不得保留上一幀的 fade identity。
  GameplayCameraOcclusionPolicy3D staleFadePolicy;
  std::array staleFade{Candidate(501)};
  staleFade[0].environmentSphereSweep = Hit(3.2f, Wall);
  production.candidates = staleFade;
  const auto faded = staleFadePolicy.StepProduction(production);
  staleFade[0].environmentSphereSweep = {};
  staleFade[0].collisionWorldMaxArm = 3.2f;
  const auto worldLimited = staleFadePolicy.StepProduction(production);
  if (!faded.fadeActive || worldLimited.fadeActive ||
      worldLimited.fadeObject != StableObjectId3D{} ||
      !Near(worldLimited.fadeAlpha, 1.f))
    return 29;
  ++gates;

  CameraOcclusionPolicySettings3D badSettings{};
  badSettings.maximumDeltaTime = .3f;
  GameplayCameraOcclusionPolicy3D invalidProductionSettings(badSettings);
  production.candidates = staleFade;
  if (invalidProductionSettings.StepProduction(production).valid)
    return 30;
  badSettings = {};
  badSettings.fadeInSeconds = std::numeric_limits<float>::infinity();
  if (GameplayCameraOcclusionPolicy3D(badSettings)
          .StepProduction(production)
          .valid)
    return 31;
  production.deltaTime = .3f;
  if (GameplayCameraOcclusionPolicy3D().StepProduction(production).valid)
    return 32;
  production.deltaTime = 1.f / 60.f;
  ++gates;

  const auto standardInterval = DeriveProductionCameraFramingArmInterval3D(
      {}, {}, 16.f / 9.f, 8.f);
  const auto portraitInterval = DeriveProductionCameraFramingArmInterval3D(
      {}, {}, 9.f / 16.f, 8.f);
  const auto ultraWideInterval = DeriveProductionCameraFramingArmInterval3D(
      {}, {}, 32.f / 9.f, 8.f);
  CameraLensSafety3D narrowLens{};
  narrowLens.verticalFovRadians = .75f;
  const auto narrowInterval = DeriveProductionCameraFramingArmInterval3D(
      {}, narrowLens, 21.f / 9.f, 12.f);
  CameraSubjectShape3D tallSubject{};
  tallSubject.capsuleHeight = 2.2f;
  tallSubject.focusHeight = 1.7f;
  const auto tallInterval = DeriveProductionCameraFramingArmInterval3D(
      tallSubject, {}, 4.f / 3.f, 12.f);
  if (!standardInterval.valid || !portraitInterval.valid ||
      !ultraWideInterval.valid || !narrowInterval.valid ||
      !tallInterval.valid ||
      portraitInterval.nearArm < standardInterval.nearArm ||
      !Near(standardInterval.farArm, ultraWideInterval.farArm) ||
      !(narrowInterval.farArm > standardInterval.farArm) ||
      !(tallInterval.nearArm > standardInterval.nearArm) ||
      standardInterval.nearArm < standardInterval.minimumSubjectArm ||
      standardInterval.midArm < standardInterval.nearArm ||
      standardInterval.midArm > standardInterval.farArm)
    return 33;
  if (DeriveProductionCameraFramingArmInterval3D(
          {}, {}, std::numeric_limits<float>::quiet_NaN(), 8.f)
          .valid ||
      DeriveProductionCameraFramingArmInterval3D({}, {}, 1.f, 2.f).valid)
    return 34;
  ++gates;

  std::cout << "gates=" << gates << " recovery30=" << arm30
            << " recovery60=" << arm60 << " recovery144=" << arm144
            << " production30=" << production30
            << " production60=" << production60
            << " production144=" << production144
            << " minimumArm=" << clear.minimumSubjectArm << '\n';
}
