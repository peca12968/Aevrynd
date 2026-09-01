#pragma once

#include <Aevrynd/Math.h>
#include <Aevrynd/StableId3D.h>

#include <cstdint>
#include <cstddef>
#include <span>

namespace GameplayLab {

enum class CameraShoulder3D : std::uint8_t { Center, Right, Left };

struct CameraSubjectShape3D {
  float capsuleHeight{1.8f};
  float capsuleRadius{.35f};
  float focusHeight{1.45f};
};

struct CameraLensSafety3D {
  float nearPlane{.1f};
  float verticalFovRadians{1.04719755f};
  float subjectMargin{.08f};
};

struct CameraOcclusionSweep3D {
  float hitDistance{};
  Aevrynd::StableObjectId3D occluder{};
  std::uint64_t generation{};
  bool blocked{};
};

struct CameraOcclusionPolicyInput3D {
  CameraSubjectShape3D subject{};
  CameraLensSafety3D lens{};
  CameraOcclusionSweep3D center{};
  CameraOcclusionSweep3D right{};
  CameraOcclusionSweep3D left{};
  float desiredArm{4.f};
  float deltaTime{1.f / 60.f};
};

struct CameraOcclusionPolicySettings3D {
  float armRecoveryUnitsPerSecond{3.f};
  float shoulderSwitchAdvantage{.2f};
  float fadeOpacity{.18f};
  float fadeInSeconds{.12f};
  float fadeOutSeconds{.2f};
  float maximumDeltaTime{1.f / 30.f};
  float candidateSwitchAdvantage{.15f};
  float yawRateRadiansPerSecond{2.4f};
  float pitchRateRadiansPerSecond{1.8f};
  float elevationRateUnitsPerSecond{2.5f};
};

struct ProductionCameraCandidate3D {
  Aevrynd::StableObjectId3D candidateId{};
  Aevrynd::Vec3 cameraFocus{};
  CameraOcclusionSweep3D environmentSphereSweep{};
  CameraShoulder3D shoulder{CameraShoulder3D::Center};
  float yawOffsetRadians{};
  float pitchRadians{};
  float verticalElevation{};
  float springArmDistance{4.f};
  float collisionWorldMaxArm{4.f};
  float heightFieldMaxArm{4.f};
  float sweepRadius{.35f};
  float subjectScreenHeight{.5f};
  float subjectScreenWidth{.2f};
  float subjectScreenArea{.1f};
  float headroom{.1f};
  float buildingUnionCoverage{};
  float maximumWallCoverage{};
  float roofCoverage{};
  float subjectOcclusionCoverage{};
  float actorMinimumMargin{.1f};
  float actorGapPixels{64.f};
  bool allActorsOnScreen{true};
  bool compositionAdmitted{true};
  bool feetOnScreen{true};
  bool roofOrTopBlocked{};
};

struct ProductionCameraOcclusionInput3D {
  CameraSubjectShape3D subject{};
  CameraLensSafety3D lens{};
  std::span<const ProductionCameraCandidate3D> candidates{};
  float desiredArm{4.f};
  float deltaTime{1.f / 60.f};
};

struct ProductionCameraFramingArmInterval3D {
  float nearArm{}, midArm{}, farArm{}, minimumSubjectArm{};
  bool valid{};
};

[[nodiscard]] ProductionCameraFramingArmInterval3D
DeriveProductionCameraFramingArmInterval3D(
    CameraSubjectShape3D subject, CameraLensSafety3D lens, float aspectRatio,
    float maximumArm, float minimumScreenHeight = .35f,
    float maximumScreenHeight = .70f) noexcept;

struct CameraOcclusionPolicyOutput3D {
  Aevrynd::StableObjectId3D fadeObject{};
  std::uint64_t fadeGeneration{};
  CameraShoulder3D shoulder{CameraShoulder3D::Center};
  float arm{};
  float minimumSubjectArm{};
  float fadeAlpha{1.f};
  float targetFadeOpacity{1.f};
  bool fadeActive{};
  bool fadeSettled{};
  bool valid{};
  Aevrynd::StableObjectId3D candidateId{};
  std::size_t candidateIndex{};
  float yawOffsetRadians{};
  float pitchRadians{};
  float verticalElevation{};
  bool clearLineOfSight{};
  bool subjectFramed{};
  bool usedFallback{};
};

class GameplayCameraOcclusionPolicy3D {
public:
  explicit GameplayCameraOcclusionPolicy3D(
      CameraOcclusionPolicySettings3D settings = {}) noexcept;

  // Policy 僅產生相機與透明度建議；呼叫端保留碰撞、陰影與物件身分。
  [[nodiscard]] CameraOcclusionPolicyOutput3D
  Step(const CameraOcclusionPolicyInput3D &input) noexcept;
  [[nodiscard]] CameraOcclusionPolicyOutput3D
  StepProduction(const ProductionCameraOcclusionInput3D &input) noexcept;
  void Reset() noexcept;

private:
  CameraOcclusionPolicySettings3D settings_{};
  Aevrynd::StableObjectId3D fadeObject_{};
  std::uint64_t fadeGeneration_{};
  float arm_{};
  float fadeAlpha_{1.f};
  CameraShoulder3D shoulder_{CameraShoulder3D::Center};
  Aevrynd::StableObjectId3D candidateId_{};
  float yawOffsetRadians_{};
  float pitchRadians_{};
  float verticalElevation_{};
  bool initialized_{};
  bool fading_{};
};

} // namespace GameplayLab
