#pragma once

#include "GameplayCameraOcclusionPolicy3D.h"
#include "GameplayOccluderMask3D.h"

#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>

#include <functional>
#include <span>
#include <vector>

namespace GameplayLab {

struct GameplayCameraProductionQueryAdapters3D {
  std::function<CameraOcclusionSweep3D(Aevrynd::Vec3, float, Aevrynd::Vec3)>
      environmentSphereSweep;
  std::function<float(const Aevrynd::Camera3D &, Aevrynd::Vec3)>
      collisionWorldMaxArm;
  std::function<float(const Aevrynd::Camera3D &, Aevrynd::Vec3)>
      heightFieldMaxArm;
  std::function<bool(const Aevrynd::Camera3D &)> roofOrTopBlocked;
};

struct GameplayCameraCompositionActor3D {
  Aevrynd::Vec3 center{};
  Aevrynd::Vec3 extents{};
  std::span<const Aevrynd::Vertex3D> posedVertices{};
  Aevrynd::Matrix4 world{Aevrynd::Matrix4::Identity()};
};

struct GameplayCameraProductionBuildInput3D {
  std::span<const Aevrynd::Vertex3D> bindVertices{};
  Aevrynd::Vec3 feet{};
  float characterYaw{};
  Aevrynd::CameraProjection projection{};
  Aevrynd::Vec2 viewport{1280, 720};
  float desiredArm{4.5f};
  float sweepRadius{.35f};
  float minimumSubjectScreenHeight{.35f};
  float minimumSubjectScreenArea{.02f};
  float minimumTopCenterRayElevation{.03f};
  std::uint64_t candidateIdHigh{};
  std::span<const GameplayOccluderBox3D> compositionOccluders{};
  std::span<const GameplayCameraCompositionActor3D> compositionActors{};
};

struct GameplayCameraProductionBuildReceipt3D {
  std::size_t candidates{}, environmentSweeps{}, collisionWorldConstraints{},
      heightFieldConstraints{}, roofQueries{};
  std::size_t completeIdentities{}, blockedWithGeneration{};
  std::size_t safeCandidates{}, framedCandidates{}, admittedCandidates{};
  std::size_t rejectedEnvironment{}, rejectedCollisionWorld{},
      rejectedHeightField{}, rejectedRoof{}, rejectedHeight{}, rejectedWidth{},
      rejectedArea{}, rejectedHeadroom{}, rejectedFeet{};
  std::size_t compositionEvaluations{}, compositionCandidates{},
      rejectedBuildingUnion{}, rejectedBuildingPart{}, rejectedRoofCoverage{},
      rejectedSubjectOcclusion{};
  std::size_t rejectedActorBounds{}, rejectedActorGap{}, rejectedHorizon{};
  bool visualBoundsValid{}, projectionValid{}, valid{};
  float minimumBuildingUnion{1.f}, minimumWallCoverage{1.f},
      minimumRoofCoverage{1.f}, minimumSubjectOcclusion{1.f},
      minimumSubjectScreenArea{.02f},
      minimumTopCenterRayElevation{.03f},
      minimumAcceptedTopCenterRayElevation{};
};

struct GameplayCameraGroundHorizonGate3D {
  float topCenterRayElevation{};
  float minimumTopCenterRayElevation{};
  bool accepted{};
};

// MakeThirdPersonCamera的正pitch代表鏡頭向下俯視；viewport頂部中央射線
// 相對水平面的仰角為half vertical FOV減pitch。此解析gate不依賴場景資源，
// 只保證畫面頂部仍有地平線以上的潛在天空，不聲稱射線未被幾何遮擋。
[[nodiscard]] GameplayCameraGroundHorizonGate3D
EvaluateGameplayThirdPersonGroundHorizon3D(
    Aevrynd::CameraProjection projection, float pitchRadians,
    float minimumTopCenterRayElevation = .03f) noexcept;

struct GameplayCameraProductionBuildOutput3D {
  std::vector<ProductionCameraCandidate3D> candidates;
  CameraSubjectShape3D subject{};
  CameraLensSafety3D lens{};
  Aevrynd::Vec3 visualFocus{};
  GameplayCameraProductionBuildReceipt3D receipt{};
};

[[nodiscard]] bool AcceptGameplayCameraSubjectScreenArea3D(
    float subjectScreenArea, float minimumSubjectScreenArea) noexcept;

// 多角色構圖的最大dolly距離由正式world bounds union推導；不改變角色位置、
// FOV或任何screen-space構圖門檻。
[[nodiscard]] float DeriveGameplayMultiActorCameraArm3D(
    float baseArm, Aevrynd::MeshBounds3D playerWorldBounds,
    std::span<const GameplayCameraCompositionActor3D> actors) noexcept;

[[nodiscard]] GameplayCameraProductionBuildOutput3D
BuildGameplayCameraProductionCandidates3D(
    const GameplayCameraProductionBuildInput3D &input,
    const GameplayCameraProductionQueryAdapters3D &queries);

} // namespace GameplayLab
