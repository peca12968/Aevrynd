#include "GameplayCameraProductionBuilder3D.h"
#include "GameplayAnimation3D.h"
#ifndef AEVRYND_CHARACTER_BUILD_FINGERPRINT
#define AEVRYND_CHARACTER_BUILD_FINGERPRINT "camera-production-integration-test"
#endif
#include "GameplayLabGame.h"

#include <Aevrynd/Camera.h>
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/EnvironmentCollisionStreaming3D.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/SkinnedModelAsset.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <limits>
#include <vector>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
constexpr std::uint64_t Partition = 0xca6e0001ull;

GameplayCameraProductionBuildOutput3D BuildActual(
    std::span<const Vertex3D> vertices,
    std::span<const GameplayCameraCompositionActor3D> actors = {}) {
  GameplayCameraProductionQueryAdapters3D queries{};
  queries.environmentSphereSweep=[](Vec3,float,Vec3){return CameraOcclusionSweep3D{};};
  queries.collisionWorldMaxArm=[](const Camera3D&,Vec3){return 4.5f;};
  queries.heightFieldMaxArm=[](const Camera3D&,Vec3){return 4.5f;};
  queries.roofOrTopBlocked=[](const Camera3D&){return false;};
  GameplayCameraProductionBuildInput3D input{};input.bindVertices=vertices;input.candidateIdHigh=0xca6e2001ull;input.compositionActors=actors;
  return BuildGameplayCameraProductionCandidates3D(input,queries);
}

std::array<Vertex3D,8> BoundsHull(MeshBounds3D bounds){std::array<Vertex3D,8> result{};for(unsigned corner=0;corner<8;++corner){result[corner].x=bounds.center.x+(corner&1?bounds.extents.x:-bounds.extents.x);result[corner].y=bounds.center.y+(corner&2?bounds.extents.y:-bounds.extents.y);result[corner].z=bounds.center.z+(corner&4?bounds.extents.z:-bounds.extents.z);}return result;}
MeshBounds3D MergeBounds(MeshBounds3D left,MeshBounds3D right){const Vec3 minimum{std::min(left.center.x-left.extents.x,right.center.x-right.extents.x),std::min(left.center.y-left.extents.y,right.center.y-right.extents.y),std::min(left.center.z-left.extents.z,right.center.z-right.extents.z)},maximum{std::max(left.center.x+left.extents.x,right.center.x+right.extents.x),std::max(left.center.y+left.extents.y,right.center.y+right.extents.y),std::max(left.center.z+left.extents.z,right.center.z+right.extents.z)},center=(minimum+maximum)*.5f,extents=(maximum-minimum)*.5f;return{center,Length(extents),extents};}
std::uint64_t PoseHash(const AnimationPose3D& pose){std::uint64_t hash=14695981039346656037ull;for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t index=0;index<sizeof(value);++index){hash^=bytes[index];hash*=1099511628211ull;}}return hash;}

CameraOcclusionPolicyOutput3D Run(float hz, std::uint64_t &candidateCount,
                                  std::uint64_t &sphereSweeps,
                                  std::uint64_t &worldConstraints,
                                  std::uint64_t &terrainConstraints,
                                  float aspectRatio = 16.f / 9.f,
                                  float verticalFov = 1.04719755f,
                                  float characterHeight = 1.8f) {
  PhysicsWorld3D physics;
  EnvironmentCollisionStreaming3D environment(physics, {2, 16, 16});
  const std::array descriptors{
      EnvironmentColliderDescriptor3D{{Partition, 1},
                                      EnvironmentColliderSemantic3D::Building,
                                      {0, 1.5f, -2.2f}, {1.1f, 2.f, .35f}}};
  if (!environment.RegisterPartition({Partition, {}, 32.f}, descriptors) ||
      !environment.UpdateObserver({}))
    return {};

  CollisionWorld3D world;
  if (!world.CreateBox({{{-2.4f, 1.5f, -2.f}, {.35f, 2.f, 1.f}}}))
    return {};
  HeightField3D terrain({{-16, 0, -16}, 33, 33, 1.f});
  terrain.Fill(0.f);

  Mesh3D visual = Mesh3D::CreateUnitBox();
  for (auto &vertex : visual.vertices) {
    vertex.x *= .7f;
    vertex.y = (vertex.y + .5f) * characterHeight;
    vertex.z *= .45f;
  }
  const Vec2 viewport{720.f * aspectRatio, 720.f};
  GameplayCameraProductionQueryAdapters3D queries{};
  queries.environmentSphereSweep=[&](Vec3 center,float radius,Vec3 displacement){CameraOcclusionSweep3D result{};const auto hit=environment.SweepSphere(center,radius,displacement);if(hit)result={std::max(0.f,hit.distance-.12f),hit.object.id,hit.object.generation,true};return result;};
  queries.collisionWorldMaxArm=[&](const Camera3D& camera,Vec3 focus){return Length(ConstrainThirdPersonCamera(camera,focus,world,.25f).position-focus);};
  queries.heightFieldMaxArm=[&](const Camera3D& camera,Vec3 focus){return Length(ConstrainThirdPersonCamera(camera,focus,terrain,.25f).position-focus);};
  queries.roofOrTopBlocked=[](const Camera3D&){return false;};
  GameplayCameraProductionBuildInput3D buildInput{};buildInput.bindVertices=visual.vertices;buildInput.projection.verticalFieldOfView=verticalFov;buildInput.viewport=viewport;buildInput.desiredArm=8.f;buildInput.candidateIdHigh=0xca6e1001ull;
  auto built=BuildGameplayCameraProductionCandidates3D(buildInput,queries);
  if(!built.receipt.valid)return{};
  candidateCount += built.receipt.candidates;sphereSweeps+=built.receipt.environmentSweeps;worldConstraints+=built.receipt.collisionWorldConstraints;terrainConstraints+=built.receipt.heightFieldConstraints;
  GameplayCameraOcclusionPolicy3D policy;
  ProductionCameraOcclusionInput3D input{};
  input.candidates = built.candidates;
  input.subject = built.subject;
  input.lens = built.lens;
  input.desiredArm = 8.f;
  input.deltaTime = 1.f / hz;
  CameraOcclusionPolicyOutput3D output{};
  for (int frame = 0; frame < static_cast<int>(hz); ++frame)
    output = policy.StepProduction(input);
  return output;
}
} // namespace

int main() {
  CameraProjection canonicalProjection{};
  canonicalProjection.verticalFieldOfView = 1.04719755f;
  const auto horizonRejected = EvaluateGameplayThirdPersonGroundHorizon3D(
      canonicalProjection, .60f);
  const auto horizonAccepted = EvaluateGameplayThirdPersonGroundHorizon3D(
      canonicalProjection, .44f);
  if (horizonRejected.accepted ||
      !std::isfinite(horizonRejected.topCenterRayElevation) ||
      horizonRejected.topCenterRayElevation >= .03f ||
      !horizonAccepted.accepted ||
      std::abs(horizonAccepted.topCenterRayElevation -
               (canonicalProjection.verticalFieldOfView * .5f - .44f)) >
          .000001f ||
      horizonAccepted.minimumTopCenterRayElevation != .03f)
    return 53;
  CameraProjection orthographicProjection = canonicalProjection;
  orthographicProjection.mode = ProjectionMode::Orthographic;
  if (EvaluateGameplayThirdPersonGroundHorizon3D(orthographicProjection, .44f)
          .accepted ||
      EvaluateGameplayThirdPersonGroundHorizon3D(
          canonicalProjection, (std::numeric_limits<float>::quiet_NaN)())
          .accepted ||
      EvaluateGameplayThirdPersonGroundHorizon3D(canonicalProjection, .44f,
                                                  -.01f)
          .accepted)
    return 54;
  const std::filesystem::path modelRoot=std::filesystem::path(AEVRYND_SOURCE_DIR)/"games/character_lab/assets/model-packs";
  const auto female=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-female/realistic-female.aeskin");
  const auto male=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-male/realistic-male.aeskin");
  if(!female||!male)return 5;
  const auto femaleArms=BuildFirstPersonArmsSkinnedMesh3D(female.asset),maleArms=BuildFirstPersonArmsSkinnedMesh3D(male.asset);
  const auto femaleArmsEvidence=BuildGameplayFirstPersonArmsEvidence3D(femaleArms),maleArmsEvidence=BuildGameplayFirstPersonArmsEvidence3D(maleArms);
  const auto productionArmsContract=[](const SkinnedModelAssetData& source,const FirstPersonArmsMeshResult3D& arms,const GameplayFirstPersonArmsEvidence3D& evidence){
    if(!arms||!evidence.Complete()||arms.stats.sourceTriangles!=source.model.bindMesh.indices.size()/3||
       arms.stats.retainedTriangles+arms.stats.rejectedTriangles!=arms.stats.sourceTriangles||
       arms.stats.retainedTriangles*3!=arms.model.bindMesh.indices.size()||
       arms.stats.leftArmTriangles+arms.stats.rightArmTriangles!=arms.stats.retainedTriangles||
       arms.sourceVertexIndices.size()!=arms.model.bindMesh.vertices.size()||arms.submeshes.empty()||
       arms.leftArmJoints.empty()||arms.rightArmJoints.empty())return false;
    std::uint32_t cursor{};for(const auto& range:arms.submeshes){if(range.firstIndex!=cursor||!range.indexCount||range.indexCount%3||std::size_t(range.firstIndex)+range.indexCount>arms.model.bindMesh.indices.size()||range.materialSlot>=source.materialSlotNames.size())return false;cursor+=range.indexCount;}return cursor==arms.model.bindMesh.indices.size();
  };
  if(!productionArmsContract(female.asset,femaleArms,femaleArmsEvidence)||!productionArmsContract(male.asset,maleArms,maleArmsEvidence)||
     femaleArms.model.skeletonCompatibilityId!=female.asset.model.skeletonCompatibilityId||
     maleArms.model.skeletonCompatibilityId!=male.asset.model.skeletonCompatibilityId||
     femaleArms.model.bindMesh.indices.size()>=female.asset.model.bindMesh.indices.size()||
     maleArms.model.bindMesh.indices.size()>=male.asset.model.bindMesh.indices.size()){
    std::cerr<<"arms contract female="<<femaleArmsEvidence.submeshCount<<':'<<femaleArmsEvidence.retainedTriangles<<':'<<femaleArmsEvidence.leftArmTriangles<<':'<<femaleArmsEvidence.rightArmTriangles
             <<" male="<<maleArmsEvidence.submeshCount<<':'<<maleArmsEvidence.retainedTriangles<<':'<<maleArmsEvidence.leftArmTriangles<<':'<<maleArmsEvidence.rightArmTriangles<<'\n';return 21;}
  // 破壞正式 typed skeleton 的必要手部關節，確認 extraction 必須 fail-closed。
  auto missingHand=female.asset;const auto hand=std::ranges::find(missingHand.jointNames,std::string{"LeftHand"});if(hand==missingHand.jointNames.end())return 58;*hand="MissingRequiredJoint";const auto rejectedArms=BuildFirstPersonArmsSkinnedMesh3D(missingHand);if(rejectedArms.status!=FirstPersonArmsMeshStatus3D::MissingJoint||rejectedArms.Complete())return 59;
  const auto femaleBuild=BuildActual(female.asset.model.bindMesh.vertices),maleBuild=BuildActual(male.asset.model.bindMesh.vertices);
  if(femaleBuild.receipt.rejectedHorizon!=240u||
     maleBuild.receipt.rejectedHorizon!=240u||
     std::abs(femaleBuild.receipt.minimumTopCenterRayElevation-.03f)>.000001f||
     femaleBuild.receipt.minimumAcceptedTopCenterRayElevation<.03f||
     !std::isfinite(femaleBuild.receipt.minimumAcceptedTopCenterRayElevation))
    return 55;
  if(!femaleBuild.receipt.valid||!maleBuild.receipt.valid||!femaleBuild.receipt.admittedCandidates||!maleBuild.receipt.admittedCandidates){std::cerr<<"actual female admitted="<<femaleBuild.receipt.admittedCandidates<<" safe="<<femaleBuild.receipt.safeCandidates<<" framed="<<femaleBuild.receipt.framedCandidates<<" heightReject="<<femaleBuild.receipt.rejectedHeight<<" areaReject="<<femaleBuild.receipt.rejectedArea<<" headroomReject="<<femaleBuild.receipt.rejectedHeadroom<<" feetReject="<<femaleBuild.receipt.rejectedFeet<<" male admitted="<<maleBuild.receipt.admittedCandidates<<" safe="<<maleBuild.receipt.safeCandidates<<" framed="<<maleBuild.receipt.framedCandidates<<" heightReject="<<maleBuild.receipt.rejectedHeight<<" areaReject="<<maleBuild.receipt.rejectedArea<<" headroomReject="<<maleBuild.receipt.rejectedHeadroom<<" feetReject="<<maleBuild.receipt.rejectedFeet<<'\n';return 6;}
  GameplayCameraOcclusionPolicy3D femalePolicy,malePolicy;ProductionCameraOcclusionInput3D femaleInput{femaleBuild.subject,femaleBuild.lens,femaleBuild.candidates,4.5f,1.f/60.f},maleInput{maleBuild.subject,maleBuild.lens,maleBuild.candidates,4.5f,1.f/60.f};
  if(!femalePolicy.StepProduction(femaleInput).valid||!malePolicy.StepProduction(maleInput).valid)return 7;
  GameplayAnimation3D femaleIdleAnimation,maleIdleAnimation;std::string animationError;
  AnimationPose3D femaleIdlePose{},maleIdlePose{};SkinningWorkspace3D poseWorkspace{};Mesh3D femaleIdleMesh{},maleIdleMesh{};
  if(!femaleIdleAnimation.Initialize(female.asset,&animationError)||!maleIdleAnimation.Initialize(male.asset,&animationError)||
     !femaleIdleAnimation.EvaluateInto(femaleIdlePose)||!maleIdleAnimation.EvaluateInto(maleIdlePose)||
     !SkinModelInto(female.asset.model,femaleIdlePose,poseWorkspace,femaleIdleMesh)||!SkinModelInto(male.asset.model,maleIdlePose,poseWorkspace,maleIdleMesh))return 24;
  GameplayShowcaseEnvironment3D godEnvironment;if(!godEnvironment.Initialize(0x4a17u))return 35;const auto godGrassBounds=CreateGrassClumpMesh3D(11,3).Bounds();const Vec3 spawnGodFocus{-16,1.1f,8};const Matrix4 spawnGodWorld=CreateTranslationMatrix({-16,0,8});const auto spawnGod=SelectGameplayGodCameraLayout3D(spawnGodFocus,godEnvironment.Descriptors(),godGrassBounds,femaleIdleMesh.vertices,femaleIdleMesh.indices,spawnGodWorld);if(!spawnGod.valid||spawnGod.playerCoverage<GameplayGodMinimumPlayerCoverage3D||spawnGod.visibleWorldDescriptors<20||!spawnGod.lodTransitions){std::cerr<<"spawn god max="<<spawnGod.maximumActualTriangleCoverage<<" coverage="<<spawnGod.playerCoverage<<" coarse="<<spawnGod.coarseCandidates<<" raster="<<spawnGod.rasterCandidates<<" rejectActual="<<spawnGod.rejectedActualCoverage<<" rejectLod="<<spawnGod.rejectedLod<<" rejectWorld="<<spawnGod.rejectedWorld<<" rejectBuilding="<<spawnGod.rejectedBuilding<<'\n';return 45;}const Vec3 godFocus{-16,1.4f,8.73334f};const Matrix4 godPlayerWorld=CreateTranslationMatrix(godFocus-Vec3{0,1.4f,0});const auto femaleGod=SelectGameplayGodCameraLayout3D(godFocus,godEnvironment.Descriptors(),godGrassBounds,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld),maleGod=SelectGameplayGodCameraLayout3D(godFocus,godEnvironment.Descriptors(),godGrassBounds,maleIdleMesh.vertices,maleIdleMesh.indices,godPlayerWorld);if(!femaleGod.valid||!maleGod.valid||femaleGod.playerCoverage<GameplayGodMinimumPlayerCoverage3D||maleGod.playerCoverage<GameplayGodMinimumPlayerCoverage3D||!femaleGod.lodTransitions||!maleGod.lodTransitions||femaleGod.buildingUnion>.22f||maleGod.buildingUnion>.22f){std::cerr<<"god posed female="<<femaleGod.valid<<":"<<femaleGod.playerCoverage<<":"<<femaleGod.lodTransitions<<":"<<femaleGod.buildingUnion<<" male="<<maleGod.valid<<":"<<maleGod.playerCoverage<<":"<<maleGod.lodTransitions<<":"<<maleGod.buildingUnion<<'\n';return 36;}const auto femaleGodRecheck=ValidateGameplayGodCameraLayout3D(godFocus,femaleGod.camera,godEnvironment.Descriptors(),godGrassBounds,femaleGod.distance,femaleGod.azimuth,femaleGod.elevation,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld);if(!femaleGodRecheck.valid||std::abs(femaleGodRecheck.playerCoverage-femaleGod.playerCoverage)>.00001f)return 37;
  const auto godGeometry=LegacyGameplayGodProductionGeometry3D(godGrassBounds);
  const auto identityConstraint=[](Camera3D camera){return camera;};
  const auto resolvedGod=ResolveGameplayGodCameraLayout3D(godFocus,femaleGod,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,identityConstraint,identityConstraint);
  const auto repeatedGod=ResolveGameplayGodCameraLayout3D(godFocus,resolvedGod,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,identityConstraint,identityConstraint);
  if(!resolvedGod.valid||!repeatedGod.valid||resolvedGod.camera.position.x!=repeatedGod.camera.position.x||resolvedGod.camera.position.y!=repeatedGod.camera.position.y||resolvedGod.camera.position.z!=repeatedGod.camera.position.z||resolvedGod.camera.projection.orthographicHeight!=repeatedGod.camera.projection.orthographicHeight)return 56;
  // 非冪等約束模擬舊版候選每次重驗都漂移；固定點契約必須精確拒絕，不能靠放寬容差通過。
  const auto driftingConstraint=[](Camera3D camera){camera.position.x+=.01f;camera.target.x+=.01f;return camera;};
  if(ResolveGameplayGodCameraLayout3D(godFocus,femaleGod,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,driftingConstraint,identityConstraint).valid)return 57;
  CollisionWorld3D godCollision;const Vec3 godOffset=femaleGod.camera.position-godFocus;if(!godCollision.CreateBox({{godFocus+godOffset*.75f,{.4f,.4f,.4f}}}))return 60;
  HeightField3D godTerrain(HeightFieldSettings3D{{-128,-100,-128},257,257,1.f});godTerrain.Fill(-100.f);const Vec3 terrainObstacle=godFocus+godOffset*.25f;const unsigned terrainX=unsigned(std::lround(terrainObstacle.x+128.f)),terrainZ=unsigned(std::lround(terrainObstacle.z+128.f));for(unsigned z=terrainZ-1;z<=terrainZ+1;++z)for(unsigned x=terrainX-1;x<=terrainX+1;++x)godTerrain.SetHeight(x,z,terrainObstacle.y+1.f);
  const auto sameVec=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};const auto directCollision=ConstrainThirdPersonCamera(femaleGod.camera,godFocus,godCollision,.25f),directTerrain=ConstrainThirdPersonCamera(directCollision,godFocus,godTerrain,.25f);if(sameVec(directCollision.position,femaleGod.camera.position)||sameVec(directTerrain.position,directCollision.position)||godTerrain.MinimumHeight()==godTerrain.MaximumHeight())return 60;
  std::uint32_t collisionChanges{},terrainChanges{};const auto productionConstrainedGod=SelectGameplayGodCameraLayoutConstrained3D(godFocus,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,[&](Camera3D camera){const auto constrained=ConstrainThirdPersonCamera(camera,godFocus,godCollision,.25f);collisionChanges+=!sameVec(constrained.position,camera.position);return constrained;},[&](Camera3D camera){const auto constrained=ConstrainThirdPersonCamera(camera,godFocus,godTerrain,.25f);terrainChanges+=!sameVec(constrained.position,camera.position);return constrained;});
  if(!productionConstrainedGod.valid||!collisionChanges||!terrainChanges)return 60;
  // 倒序留下無效 up，因而在完整 Validate 被拒；此案例不把倒序誤稱為非冪等。
  const auto markCollision=[](Camera3D camera){camera.up={0,0,0};return camera;};const auto consumeTerrain=[](Camera3D camera){if(camera.up.x==0.f&&camera.up.y==0.f&&camera.up.z==0.f)camera.up={0,1,0};return camera;};
  if(!SelectGameplayGodCameraLayoutConstrained3D(godFocus,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,markCollision,consumeTerrain).valid||SelectGameplayGodCameraLayoutConstrained3D(godFocus,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,consumeTerrain,markCollision).valid)return 61;
  // 第一個視覺最佳候選故意不冪等；正式搜尋必須 deterministic 地退到下一個 collision-safe 候選。
  struct GodCandidateIdentity{Camera3D camera;float distance,azimuth,elevation;};const GodCandidateIdentity firstIdentity{femaleGod.camera,femaleGod.distance,femaleGod.azimuth,femaleGod.elevation};
  std::uint32_t rejectedFirstCalls{};const auto rejectFirst=[&](Camera3D camera){const float positionDrift=camera.position.x-firstIdentity.camera.position.x,targetDrift=camera.target.x-firstIdentity.camera.target.x;const bool sameUniqueCandidate=camera.position.y==firstIdentity.camera.position.y&&camera.position.z==firstIdentity.camera.position.z&&camera.target.y==firstIdentity.camera.target.y&&camera.target.z==firstIdentity.camera.target.z&&positionDrift==targetDrift&&positionDrift>=0.f&&positionDrift<.1f&&camera.projection.mode==firstIdentity.camera.projection.mode&&camera.projection.verticalFieldOfView==firstIdentity.camera.projection.verticalFieldOfView&&camera.projection.orthographicHeight==firstIdentity.camera.projection.orthographicHeight&&camera.projection.nearPlane==firstIdentity.camera.projection.nearPlane&&camera.projection.farPlane==firstIdentity.camera.projection.farPlane;if(sameUniqueCandidate){camera.position.x+=.01f;camera.target.x+=.01f;++rejectedFirstCalls;}return camera;};
  const auto fallbackGod=SelectGameplayGodCameraLayoutConstrained3D(godFocus,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,rejectFirst,identityConstraint),fallbackRepeat=SelectGameplayGodCameraLayoutConstrained3D(godFocus,godEnvironment.Descriptors(),godGeometry,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld,rejectFirst,identityConstraint);
  const auto exactCamera=[&](const Camera3D& a,const Camera3D& b){return sameVec(a.position,b.position)&&sameVec(a.target,b.target)&&sameVec(a.up,b.up)&&a.projection.mode==b.projection.mode&&a.projection.verticalFieldOfView==b.projection.verticalFieldOfView&&a.projection.orthographicHeight==b.projection.orthographicHeight&&a.projection.nearPlane==b.projection.nearPlane&&a.projection.farPlane==b.projection.farPlane;};
  const auto sameIdentity=[&](const GameplayGodCameraLayout3D& layout,const GodCandidateIdentity& identity){return exactCamera(layout.camera,identity.camera)&&layout.distance==identity.distance&&layout.azimuth==identity.azimuth&&layout.elevation==identity.elevation;};
  const auto exactLayout=[&](const GameplayGodCameraLayout3D& a,const GameplayGodCameraLayout3D& b){return exactCamera(a.camera,b.camera)&&a.distance==b.distance&&a.azimuth==b.azimuth&&a.elevation==b.elevation&&a.playerCoverage==b.playerCoverage&&a.buildingUnion==b.buildingUnion&&a.maximumBuildingPart==b.maximumBuildingPart&&a.roofCoverage==b.roofCoverage&&a.lodTransitions==b.lodTransitions&&a.visibleWorldDescriptors==b.visibleWorldDescriptors&&a.roofTouchesTop==b.roofTouchesTop&&a.valid==b.valid&&a.maximumActualTriangleCoverage==b.maximumActualTriangleCoverage&&a.evaluatedCandidates==b.evaluatedCandidates&&a.coarseCandidates==b.coarseCandidates&&a.rasterCandidates==b.rasterCandidates&&a.rejectedActualCoverage==b.rejectedActualCoverage&&a.rejectedLod==b.rejectedLod&&a.rejectedWorld==b.rejectedWorld&&a.rejectedBuilding==b.rejectedBuilding&&a.riverCoverage==b.riverCoverage&&a.visibleBuildingComponents==b.visibleBuildingComponents&&a.buildingEdgeMask==b.buildingEdgeMask&&a.productionLodExact==b.productionLodExact;};
  if(!fallbackGod.valid||!fallbackRepeat.valid||rejectedFirstCalls!=4||sameIdentity(fallbackGod,firstIdentity)||!exactLayout(fallbackGod,fallbackRepeat)||!fallbackGod.rejectedActualCoverage)return 62;
  for(const auto* layout:{&femaleGod,&maleGod}){const GameplayCaptureProjectionEvidence3D projection{layout->camera.projection.mode,layout->camera.projection.verticalFieldOfView,layout->camera.projection.orthographicHeight,layout->playerCoverage,layout->buildingUnion,layout->maximumBuildingPart,layout->roofCoverage,layout->visibleWorldDescriptors,layout->lodTransitions};if(!projection.Complete(GameplayCaptureCamera3D::God))return 38;}
  const Camera3D firstCamera=MakeFirstPersonCamera({},0,0);auto firstProjection=firstCamera.projection;firstProjection.orthographicHeight=0.f;const GameplayCaptureProjectionEvidence3D firstProjectionEvidence{firstProjection.mode,firstProjection.verticalFieldOfView,firstProjection.orthographicHeight};if(!firstProjectionEvidence.Complete(GameplayCaptureCamera3D::First))return 39;
  for(unsigned sex=0;sex<2;++sex){const auto& source=sex?male:female;const auto& arms=sex?maleArms:femaleArms;const auto& animation=sex?maleIdleAnimation:femaleIdleAnimation;const auto& pose=sex?maleIdlePose:femaleIdlePose;Mesh3D posedArms{};if(!SkinModelInto(arms.model,pose,poseWorkspace,posedArms))return 46;Matrix4 modelHand{},armsWorld{},weaponWorld{};if(!CharacterSocketTransform3D(source.asset.model.skeleton,animation.Rig(),HumanoidJoint3D::RightHand,pose,Matrix4::Identity(),modelHand))return 47;const auto layout=GameplayFirstPersonViewmodelLayout(sex?GameplayViewmodelSex3D::Male:GameplayViewmodelSex3D::Female);if(!ComposeGameplayFirstPersonWeaponWorld3D(firstCamera,modelHand,layout,true,armsWorld)||!ComposeGameplayFirstPersonWeaponWorld3D(firstCamera,modelHand,layout,false,weaponWorld))return 48;float minimumX=1280,minimumY=720,maximumX=0,maximumY=0;for(const auto& vertex:posedArms.vertices){const auto point=ProjectToViewport(TransformPoint(armsWorld,{vertex.x,vertex.y,vertex.z}),firstCamera,{1280,720});minimumX=std::min(minimumX,point.position.x);minimumY=std::min(minimumY,point.position.y);maximumX=std::max(maximumX,point.position.x);maximumY=std::max(maximumY,point.position.y);}Matrix4 handWorld{};if(!CharacterSocketTransform3D(source.asset.model.skeleton,animation.Rig(),HumanoidJoint3D::RightHand,pose,weaponWorld,handWorld))return 49;const auto evidence=EvaluateGameplayFirstPersonViewmodelProjection3D(firstCamera,handWorld,layout);if(!evidence.Complete()||minimumX<16||minimumY<32||maximumX+16>=1280||maximumY+16>=720){std::cerr<<"viewmodel posed sex="<<sex<<" arms="<<minimumX<<","<<minimumY<<"-"<<maximumX<<","<<maximumY<<" weaponCoverage="<<evidence.weaponCoverage<<" grip="<<evidence.gripX<<","<<evidence.gripY<<'\n';return 50;}}
  for(unsigned sex=0;sex<2;++sex){const auto& source=sex?male:female;const auto& arms=sex?maleArms:femaleArms;const auto layout=GameplayFirstPersonViewmodelLayout(sex?GameplayViewmodelSex3D::Male:GameplayViewmodelSex3D::Female);for(unsigned state=0;state<=unsigned(AnimationState3D::Death);++state)for(float sample:{0.f,.12f,.28f,.52f,.76f}){GameplayAnimation3D animation;std::string error;if(!animation.Initialize(source.asset,&error)){return 66;}animation.SetState(AnimationState3D(state),true);animation.Tick(sample);AnimationPose3D pose{};Mesh3D posedArms{};if(!animation.EvaluateInto(pose)||!SkinModelInto(arms.model,pose,poseWorkspace,posedArms))return 67;Matrix4 modelHand{},armsWorld{};if(!CharacterSocketTransform3D(source.asset.model.skeleton,animation.Rig(),HumanoidJoint3D::RightHand,pose,Matrix4::Identity(),modelHand)||!ComposeGameplayFirstPersonWeaponWorld3D(firstCamera,modelHand,layout,true,armsWorld))return 68;float minimumX=1280,minimumY=720,maximumX=0,maximumY=0;for(const auto& vertex:posedArms.vertices){const auto point=ProjectToViewport(TransformPoint(armsWorld,{vertex.x,vertex.y,vertex.z}),firstCamera,{1280,720});minimumX=std::min(minimumX,point.position.x);minimumY=std::min(minimumY,point.position.y);maximumX=std::max(maximumX,point.position.x);maximumY=std::max(maximumY,point.position.y);}if(minimumX<16||minimumY<32||maximumX+16>=1280||maximumY+16>=720){std::cerr<<"viewmodel envelope sex="<<sex<<" state="<<state<<" sample="<<sample<<" arms="<<minimumX<<","<<minimumY<<"-"<<maximumX<<","<<maximumY<<'\n';return 69;}}}
  for(unsigned sex=0;sex<2;++sex){auto& animation=sex?maleIdleAnimation:femaleIdleAnimation;auto& pose=sex?maleIdlePose:femaleIdlePose;auto& mesh=sex?maleIdleMesh:femaleIdleMesh;const auto& model=sex?male.asset.model:female.asset.model;animation.SetState(AnimationState3D::Attack3,true);animation.Tick(.173f);if(!animation.EvaluateInto(pose)||!SkinModelInto(model,pose,poseWorkspace,mesh))return 40;const auto layout=SelectGameplayGodCameraLayout3D(godFocus,godEnvironment.Descriptors(),godGrassBounds,mesh.vertices,mesh.indices,godPlayerWorld);if(!layout.valid){std::cerr<<"god attack3 sex="<<sex<<" maxActual="<<layout.maximumActualTriangleCoverage<<" coarse="<<layout.coarseCandidates<<" rejectActual="<<layout.rejectedActualCoverage<<" rejectLod="<<layout.rejectedLod<<" rejectWorld="<<layout.rejectedWorld<<" rejectBuilding="<<layout.rejectedBuilding<<'\n';return 41;}}
  femaleIdleAnimation.SetState(AnimationState3D::Run,true);femaleIdleAnimation.Tick(.173f);if(!femaleIdleAnimation.EvaluateInto(femaleIdlePose)||!SkinModelInto(female.asset.model,femaleIdlePose,poseWorkspace,femaleIdleMesh))return 43;const auto runGodLayout=SelectGameplayGodCameraLayout3D(godFocus,godEnvironment.Descriptors(),godGrassBounds,femaleIdleMesh.vertices,femaleIdleMesh.indices,godPlayerWorld);if(!runGodLayout.valid){std::cerr<<"god run maxActual="<<runGodLayout.maximumActualTriangleCoverage<<" coverage="<<runGodLayout.playerCoverage<<" ortho="<<runGodLayout.camera.projection.orthographicHeight<<" world="<<runGodLayout.visibleWorldDescriptors<<" lod="<<runGodLayout.lodTransitions<<" river="<<runGodLayout.riverCoverage<<" union="<<runGodLayout.buildingUnion<<" maxPart="<<runGodLayout.maximumBuildingPart<<" roof="<<runGodLayout.roofCoverage<<" edges="<<runGodLayout.buildingEdgeMask<<" components="<<runGodLayout.visibleBuildingComponents<<" coarse="<<runGodLayout.coarseCandidates<<" raster="<<runGodLayout.rasterCandidates<<" rejectActual="<<runGodLayout.rejectedActualCoverage<<" rejectLod="<<runGodLayout.rejectedLod<<" rejectWorld="<<runGodLayout.rejectedWorld<<" rejectBuilding="<<runGodLayout.rejectedBuilding<<'\n';return 44;}
  if(!femaleIdleAnimation.Initialize(female.asset,&animationError)||!maleIdleAnimation.Initialize(male.asset,&animationError)||!femaleIdleAnimation.EvaluateInto(femaleIdlePose)||!maleIdleAnimation.EvaluateInto(maleIdlePose)||!SkinModelInto(female.asset.model,femaleIdlePose,poseWorkspace,femaleIdleMesh)||!SkinModelInto(male.asset.model,maleIdlePose,poseWorkspace,maleIdleMesh))return 42;
  const auto maleWorldBounds=TransformBounds(maleIdleMesh.Bounds(),{GameplayShowcaseMonsterSpawn3D({},0),{0,-.8f,0},{1.08f,1.08f,1.08f}});
  const Transform3D maleWorldTransform{GameplayShowcaseMonsterSpawn3D({},0),{0,-.8f,0},{1.08f,1.08f,1.08f}};
  const auto maleIdleHull=BoundsHull(maleIdleMesh.Bounds());const std::array separatedActors{GameplayCameraCompositionActor3D{maleWorldBounds.center,maleWorldBounds.extents,maleIdleHull,CreateTransformMatrix(maleWorldTransform.position,maleWorldTransform.rotation,maleWorldTransform.scale)}};
  const auto allActorBuild=BuildActual(femaleIdleMesh.vertices,separatedActors);
  if(!allActorBuild.receipt.valid||!allActorBuild.receipt.admittedCandidates||
     std::ranges::any_of(allActorBuild.candidates,[](const auto& candidate){return candidate.compositionAdmitted&&(candidate.actorMinimumMargin<.05f||candidate.actorGapPixels<64.f||!candidate.allActorsOnScreen);})){
    std::cerr<<"all-actor admitted="<<allActorBuild.receipt.admittedCandidates<<" boundsReject="<<allActorBuild.receipt.rejectedActorBounds<<" gapReject="<<allActorBuild.receipt.rejectedActorGap<<'\n';return 22;
  }
  const auto admittedActor=std::ranges::find_if(allActorBuild.candidates,[](const auto& candidate){return candidate.compositionAdmitted;});
  if(admittedActor==allActorBuild.candidates.end())return 25;
  const Vec3 centeredActorFocus=(allActorBuild.visualFocus+maleWorldBounds.center)*.5f,actorFocusDelta=centeredActorFocus-allActorBuild.visualFocus;bool actorFocusMatched=false;for(float factor:{.7f,1.f,1.3f})actorFocusMatched|=Length(admittedActor->cameraFocus-(allActorBuild.visualFocus+actorFocusDelta*factor+Vec3{0,admittedActor->verticalElevation,0}))<=.0001f;
  if(!actorFocusMatched)return 26;
  GameplayCameraOcclusionPolicy3D actorPolicy;ProductionCameraOcclusionInput3D actorInput{allActorBuild.subject,allActorBuild.lens,allActorBuild.candidates,4.5f,1.f/60.f};
  const auto actorOutput=actorPolicy.StepProduction(actorInput);if(!actorOutput.valid||actorOutput.candidateIndex>=allActorBuild.candidates.size())return 27;
  const auto& selectedActor=allActorBuild.candidates[actorOutput.candidateIndex];
  const auto reconstructedActorCamera=MakeThirdPersonCamera(selectedActor.cameraFocus,actorOutput.yawOffsetRadians,actorOutput.pitchRadians,actorOutput.arm,{});
  if(Length(reconstructedActorCamera.target-selectedActor.cameraFocus)>.0001f)return 28;
  const auto femaleBounds=female.asset.model.bindMesh.Bounds();
  const std::array overlappingActors{GameplayCameraCompositionActor3D{femaleBounds.center,femaleBounds.extents}};
  const auto overlappingActorBuild=BuildActual(female.asset.model.bindMesh.vertices,overlappingActors);
  if(!overlappingActorBuild.receipt.valid||overlappingActorBuild.receipt.admittedCandidates||!overlappingActorBuild.receipt.rejectedActorGap)return 23;
  const Transform3D closeCombatTransform{{1.39f,0,.16f},{0,-.8f,0},{1.08f,1.08f,1.08f}};
  const auto closeCombatBounds=TransformBounds(maleIdleMesh.Bounds(),closeCombatTransform);
  const std::array closeCombatActor{GameplayCameraCompositionActor3D{closeCombatBounds.center,closeCombatBounds.extents,maleIdleHull,CreateTransformMatrix(closeCombatTransform.position,closeCombatTransform.rotation,closeCombatTransform.scale)}};
  const auto closeCombatBuild=BuildActual(femaleIdleMesh.vertices,closeCombatActor);
  if(!closeCombatBuild.receipt.admittedCandidates){std::cerr<<"close combat composition boundsReject="<<closeCombatBuild.receipt.rejectedActorBounds<<" gapReject="<<closeCombatBuild.receipt.rejectedActorGap<<'\n';return 34;}
  // 正式 capture 只會在 Attack3 命中窗且怪物死亡後進入 melee-combat。
  // 以同一 normalized sample、正式側向距離與完整 posed bounds 固定重播，避免
  // bind-pose 或寬鬆的假 fixture 掩蓋雙角色構圖退化。
  GameplayAnimation3D meleePlayerAnimation,meleeMonsterAnimation;AnimationPose3D meleePlayerPose{},meleeMonsterPose{};Mesh3D meleePlayerMesh{},meleeMonsterMesh{};
  if(!meleePlayerAnimation.Initialize(male.asset,&animationError)||!meleeMonsterAnimation.Initialize(male.asset,&animationError))return 46;
  constexpr float meleeAttackSample=.12f/.55f;meleePlayerAnimation.SetState(AnimationState3D::Attack3,true);meleePlayerAnimation.SynchronizeNormalizedPhase(meleeAttackSample);meleeMonsterAnimation.SetState(AnimationState3D::Death,true);meleeMonsterAnimation.Tick(1.f/60.f);
  if(!meleePlayerAnimation.EvaluateInto(meleePlayerPose)||!meleeMonsterAnimation.EvaluateInto(meleeMonsterPose)||!SkinModelInto(male.asset.model,meleePlayerPose,poseWorkspace,meleePlayerMesh)||!SkinModelInto(male.asset.model,meleeMonsterPose,poseWorkspace,meleeMonsterMesh))return 47;
  const Vec3 meleeFeet{-16.f,0.f,8.73334f};const float meleeYaw=0.f;const Vec3 meleeMonsterPosition=GameplayShowcaseMonsterSpawn3D(meleeFeet,meleeYaw);
  const Transform3D meleeMonsterTransform{meleeMonsterPosition,{0,-.8f,0},{1.08f,1.08f,1.08f}};const auto meleeMonsterBounds=TransformBounds(meleeMonsterMesh.Bounds(),meleeMonsterTransform);const auto meleeMonsterHull=BoundsHull(meleeMonsterMesh.Bounds());
  const std::array meleeActor{GameplayCameraCompositionActor3D{meleeMonsterBounds.center,meleeMonsterBounds.extents,meleeMonsterHull,CreateTransformMatrix(meleeMonsterTransform.position,meleeMonsterTransform.rotation,meleeMonsterTransform.scale)}};
  GameplayCameraProductionQueryAdapters3D meleeQueries{};meleeQueries.environmentSphereSweep=[](Vec3,float,Vec3){return CameraOcclusionSweep3D{};};meleeQueries.collisionWorldMaxArm=[](const Camera3D&,Vec3){return 4.5f;};meleeQueries.heightFieldMaxArm=[](const Camera3D&,Vec3){return 4.5f;};meleeQueries.roofOrTopBlocked=[](const Camera3D&){return false;};
  GameplayCameraProductionBuildInput3D meleeInput{};meleeInput.bindVertices=meleePlayerMesh.vertices;meleeInput.feet=meleeFeet;meleeInput.characterYaw=meleeYaw;meleeInput.candidateIdHigh=0xca6e3001ull;meleeInput.compositionActors=meleeActor;
  const auto meleeBuild=BuildGameplayCameraProductionCandidates3D(meleeInput,meleeQueries);const auto meleeBest=std::ranges::max_element(meleeBuild.candidates,{},[](const auto& candidate){return candidate.actorMinimumMargin*1000.f+candidate.actorGapPixels+candidate.subjectScreenHeight*100.f;});
  if(!meleeBuild.receipt.valid||!meleeBuild.receipt.admittedCandidates||meleeBest==meleeBuild.candidates.end()){
    std::cerr<<"exact melee playerState="<<unsigned(AnimationState3D::Attack3)<<" monsterState="<<unsigned(AnimationState3D::Death)<<" sample="<<meleeAttackSample<<" playerPoseHash="<<PoseHash(meleePlayerPose)<<" monsterPoseHash="<<PoseHash(meleeMonsterPose)<<" playerPosition="<<meleeFeet.x<<','<<meleeFeet.y<<','<<meleeFeet.z<<" monsterPosition="<<meleeMonsterPosition.x<<','<<meleeMonsterPosition.y<<','<<meleeMonsterPosition.z<<" playerBounds="<<meleePlayerMesh.Bounds().center.x<<','<<meleePlayerMesh.Bounds().center.y<<','<<meleePlayerMesh.Bounds().center.z<<':'<<meleePlayerMesh.Bounds().extents.x<<','<<meleePlayerMesh.Bounds().extents.y<<','<<meleePlayerMesh.Bounds().extents.z<<" monsterBounds="<<meleeMonsterBounds.center.x<<','<<meleeMonsterBounds.center.y<<','<<meleeMonsterBounds.center.z<<':'<<meleeMonsterBounds.extents.x<<','<<meleeMonsterBounds.extents.y<<','<<meleeMonsterBounds.extents.z<<" admitted="<<meleeBuild.receipt.admittedCandidates<<" actorBounds="<<meleeBuild.receipt.rejectedActorBounds<<" actorGap="<<meleeBuild.receipt.rejectedActorGap<<'\n';return 48;
  }
  // 2026-08-31 failure-frame exact live bounds：舊4.5m grid的最佳候選已有
  // 9.19% margin/185px gap，但主角高度73.49%、面積21.45%。union-derived
  // dolly必須在原5%/64px/35..70%/2..20% gate不變時找到可行解。
  const MeshBounds3D livePlayerLocal{{-.190041f,1.04855f,.239156f},1.378f,{.743856f,1.06097f,.477875f}};
  const MeshBounds3D livePlayerWorld=TransformBounds(livePlayerLocal,{meleeFeet,{0,meleeYaw,0},{1,1,1}});
  const MeshBounds3D liveMonsterWorld{{-14.1321f,.937714f,8.22883f},1.598f,{.730323f,1.22256f,.743609f}};
  const auto livePlayerHull=BoundsHull(livePlayerLocal);const std::array liveActor{GameplayCameraCompositionActor3D{liveMonsterWorld.center,liveMonsterWorld.extents}};
  GameplayCameraProductionBuildInput3D liveInput{};liveInput.bindVertices=livePlayerHull;liveInput.feet=meleeFeet;liveInput.characterYaw=meleeYaw;liveInput.projection.verticalFieldOfView=.78f;liveInput.minimumSubjectScreenHeight=.52f;liveInput.minimumSubjectScreenArea=.05f;liveInput.candidateIdHigh=0xca6e3002ull;liveInput.compositionActors=liveActor;
  const auto liveBaseBuild=BuildGameplayCameraProductionCandidates3D(liveInput,meleeQueries);
  const float liveDerivedArm=DeriveGameplayMultiActorCameraArm3D(liveInput.desiredArm,livePlayerWorld,liveActor);
  if(!liveBaseBuild.receipt.valid||!liveBaseBuild.receipt.admittedCandidates||!std::isfinite(liveDerivedArm)||liveDerivedArm<=liveInput.desiredArm||liveDerivedArm>liveInput.desiredArm+1.5001f)return 49;
  liveInput.desiredArm=liveDerivedArm;liveInput.candidateIdHigh=0xca6e3003ull;const auto liveDerivedBuild=BuildGameplayCameraProductionCandidates3D(liveInput,meleeQueries);
  if(!liveDerivedBuild.receipt.valid||!liveDerivedBuild.receipt.admittedCandidates){std::cerr<<"live melee union arm="<<liveDerivedArm<<" admitted="<<liveDerivedBuild.receipt.admittedCandidates<<" heightReject="<<liveDerivedBuild.receipt.rejectedHeight<<" areaReject="<<liveDerivedBuild.receipt.rejectedArea<<" actorBounds="<<liveDerivedBuild.receipt.rejectedActorBounds<<" actorGap="<<liveDerivedBuild.receipt.rejectedActorGap<<'\n';return 50;}
  liveInput.projection=CameraProjection{};liveInput.desiredArm=4.5f;liveInput.candidateIdHigh=0xca6e3004ull;const auto canonicalLiveBuild=BuildGameplayCameraProductionCandidates3D(liveInput,meleeQueries);if(!canonicalLiveBuild.receipt.valid||!canonicalLiveBuild.receipt.admittedCandidates)return 52;
  if(DeriveGameplayMultiActorCameraArm3D(4.5f,{},liveActor)!=0.f||DeriveGameplayMultiActorCameraArm3D(4.5f,livePlayerWorld,std::array<GameplayCameraCompositionActor3D,1>{{GameplayCameraCompositionActor3D{{},{}}}})!=0.f)return 51;
  constexpr std::array primaryStates{AnimationState3D::Idle,AnimationState3D::Walk,AnimationState3D::Run,AnimationState3D::Crouch,AnimationState3D::Jump,AnimationState3D::Fall,AnimationState3D::Attack1,AnimationState3D::Attack2,AnimationState3D::Attack3,AnimationState3D::Hit,AnimationState3D::Death};
  constexpr std::array companionStates{AnimationState3D::Walk,AnimationState3D::Death};
  for(const auto primaryState:primaryStates)for(const auto companionState:companionStates){
    femaleIdleAnimation.SetState(primaryState,true);maleIdleAnimation.SetState(companionState,true);
    femaleIdleAnimation.Tick(.173f);maleIdleAnimation.Tick(.173f);
    if(!femaleIdleAnimation.EvaluateInto(femaleIdlePose)||!maleIdleAnimation.EvaluateInto(maleIdlePose)||
       !SkinModelInto(female.asset.model,femaleIdlePose,poseWorkspace,femaleIdleMesh)||!SkinModelInto(male.asset.model,maleIdlePose,poseWorkspace,maleIdleMesh))return 29;
    const auto posedCompanionBounds=TransformBounds(maleIdleMesh.Bounds(),maleWorldTransform);
    const auto posedCompanionHull=BoundsHull(maleIdleMesh.Bounds());const std::array posedActor{GameplayCameraCompositionActor3D{posedCompanionBounds.center,posedCompanionBounds.extents,posedCompanionHull,CreateTransformMatrix(maleWorldTransform.position,maleWorldTransform.rotation,maleWorldTransform.scale)}};
    const auto posedBuild=BuildActual(femaleIdleMesh.vertices,posedActor);
    if(!posedBuild.receipt.admittedCandidates){std::cerr<<"pose composition primary="<<unsigned(primaryState)<<" companion="<<unsigned(companionState)<<" boundsReject="<<posedBuild.receipt.rejectedActorBounds<<" gapReject="<<posedBuild.receipt.rejectedActorGap<<'\n';return 30;}
  }
  GameplayAnimation3D malePlayerAnimation,maleCompanionAnimation;AnimationPose3D malePlayerPose{},maleCompanionPose{};Mesh3D malePlayerMesh{},maleCompanionMesh{};
  if(!malePlayerAnimation.Initialize(male.asset,&animationError)||!maleCompanionAnimation.Initialize(male.asset,&animationError))return 31;
  for(const auto primaryState:primaryStates)for(const auto companionState:companionStates){
    malePlayerAnimation.SetState(primaryState,true);maleCompanionAnimation.SetState(companionState,true);malePlayerAnimation.Tick(.173f);maleCompanionAnimation.Tick(.173f);
    if(!malePlayerAnimation.EvaluateInto(malePlayerPose)||!maleCompanionAnimation.EvaluateInto(maleCompanionPose)||!SkinModelInto(male.asset.model,malePlayerPose,poseWorkspace,malePlayerMesh)||!SkinModelInto(male.asset.model,maleCompanionPose,poseWorkspace,maleCompanionMesh))return 32;
    const auto posedCompanionBounds=TransformBounds(maleCompanionMesh.Bounds(),maleWorldTransform);
    const auto posedCompanionHull=BoundsHull(maleCompanionMesh.Bounds());const std::array posedActor{GameplayCameraCompositionActor3D{posedCompanionBounds.center,posedCompanionBounds.extents,posedCompanionHull,CreateTransformMatrix(maleWorldTransform.position,maleWorldTransform.rotation,maleWorldTransform.scale)}};
    const auto posedBuild=BuildActual(malePlayerMesh.vertices,posedActor);
    if(!posedBuild.receipt.admittedCandidates){std::cerr<<"male pose composition primary="<<unsigned(primaryState)<<" companion="<<unsigned(companionState)<<" boundsReject="<<posedBuild.receipt.rejectedActorBounds<<" gapReject="<<posedBuild.receipt.rejectedActorGap<<'\n';return 33;}
  }
  for(const auto [primaryState,companionState]:std::array<std::pair<AnimationState3D,AnimationState3D>,2>{{{AnimationState3D::Fall,AnimationState3D::Walk},{AnimationState3D::Attack3,AnimationState3D::Death}}}){
    MeshBounds3D primaryEnvelope{},companionEnvelope{};bool firstSample=true;
    for(float sample:{.05f,.173f,.34f,.5f}){femaleIdleAnimation.SetState(primaryState,true);maleIdleAnimation.SetState(companionState,true);femaleIdleAnimation.Tick(sample);maleIdleAnimation.Tick(sample);if(!femaleIdleAnimation.EvaluateInto(femaleIdlePose)||!maleIdleAnimation.EvaluateInto(maleIdlePose)||!SkinModelInto(female.asset.model,femaleIdlePose,poseWorkspace,femaleIdleMesh)||!SkinModelInto(male.asset.model,maleIdlePose,poseWorkspace,maleIdleMesh))return 35;primaryEnvelope=firstSample?femaleIdleMesh.Bounds():MergeBounds(primaryEnvelope,femaleIdleMesh.Bounds());companionEnvelope=firstSample?maleIdleMesh.Bounds():MergeBounds(companionEnvelope,maleIdleMesh.Bounds());firstSample=false;}
    const auto primaryHull=BoundsHull(primaryEnvelope);
    const auto companionHull=BoundsHull(companionEnvelope);
    const auto worldEnvelope=TransformBounds(companionEnvelope,maleWorldTransform);
    const std::array actor{GameplayCameraCompositionActor3D{worldEnvelope.center,worldEnvelope.extents,companionHull,CreateTransformMatrix(maleWorldTransform.position,maleWorldTransform.rotation,maleWorldTransform.scale)}};const auto envelopeBuild=BuildActual(primaryHull,actor);if(!envelopeBuild.receipt.admittedCandidates){const auto& receipt=envelopeBuild.receipt;std::cerr<<"clip envelope primary="<<unsigned(primaryState)<<" companion="<<unsigned(companionState)<<" admitted=0 safe="<<receipt.safeCandidates<<" framed="<<receipt.framedCandidates<<" actorBounds="<<receipt.rejectedActorBounds<<" actorGap="<<receipt.rejectedActorGap<<" height="<<receipt.rejectedHeight<<" width="<<receipt.rejectedWidth<<" area="<<receipt.rejectedArea<<" headroom="<<receipt.rejectedHeadroom<<" feet="<<receipt.rejectedFeet<<" primaryCenter="<<primaryEnvelope.center.x<<','<<primaryEnvelope.center.y<<','<<primaryEnvelope.center.z<<" primaryExtents="<<primaryEnvelope.extents.x<<','<<primaryEnvelope.extents.y<<','<<primaryEnvelope.extents.z<<" companionCenter="<<worldEnvelope.center.x<<','<<worldEnvelope.center.y<<','<<worldEnvelope.center.z<<" companionExtents="<<worldEnvelope.extents.x<<','<<worldEnvelope.extents.y<<','<<worldEnvelope.extents.z<<'\n';for(const auto& candidate:envelopeBuild.candidates)if(candidate.actorGapPixels>=40.f)std::cerr<<"  yaw="<<candidate.yawOffsetRadians<<" pitch="<<candidate.pitchRadians<<" elevation="<<candidate.verticalElevation<<" arm="<<candidate.springArmDistance<<" gap="<<candidate.actorGapPixels<<" margin="<<candidate.actorMinimumMargin<<" height="<<candidate.subjectScreenHeight<<" width="<<candidate.subjectScreenWidth<<" area="<<candidate.subjectScreenArea<<" headroom="<<candidate.headroom<<" feet="<<candidate.feetOnScreen<<'\n';return 36;}
  }
  if(std::ranges::any_of(femaleBuild.candidates,[&](const auto&candidate){return candidate.sweepRadius<femaleBuild.subject.capsuleRadius;})||std::ranges::any_of(maleBuild.candidates,[&](const auto&candidate){return candidate.sweepRadius<maleBuild.subject.capsuleRadius;}))return 8;
  std::vector<Vertex3D> crouchedFemale=female.asset.model.bindMesh.vertices;
  const float femaleBase=std::ranges::min(crouchedFemale,{},[](const Vertex3D& vertex){return vertex.y;}).y;
  for(auto& vertex:crouchedFemale)vertex.y=femaleBase+(vertex.y-femaleBase)*.72f;
  const auto crouchedBuild=BuildActual(crouchedFemale);
  if(!crouchedBuild.receipt.valid||!crouchedBuild.receipt.admittedCandidates||crouchedBuild.subject.capsuleHeight>=femaleBuild.subject.capsuleHeight*.8f){std::cerr<<"pose-aware crouch bounds were not consumed\n";return 9;}
  GameplayCameraOcclusionPolicy3D transitionPolicy;
  ProductionCameraOcclusionInput3D standingTransition{femaleBuild.subject,femaleBuild.lens,femaleBuild.candidates,4.5f,1.f/60.f};
  ProductionCameraOcclusionInput3D crouchedTransition{crouchedBuild.subject,crouchedBuild.lens,crouchedBuild.candidates,4.5f,1.f/60.f};
  if(!transitionPolicy.StepProduction(standingTransition).valid)return 10;
  transitionPolicy.Reset();
  const auto crouchedTransitionOutput=transitionPolicy.StepProduction(crouchedTransition);
  if(!crouchedTransitionOutput.valid||!crouchedTransitionOutput.subjectFramed||crouchedTransitionOutput.arm<crouchedTransitionOutput.minimumSubjectArm)return 11;
  std::array<CameraOcclusionPolicyOutput3D, 3> outputs{};
  constexpr std::array<float, 3> rates{30.f, 60.f, 144.f};
  std::uint64_t candidates{}, sphereSweeps{}, worldConstraints{},
      terrainConstraints{};
  for (std::size_t index = 0; index < rates.size(); ++index)
    outputs[index] = Run(rates[index], candidates, sphereSweeps,
                         worldConstraints, terrainConstraints);
  std::array<CameraOcclusionPolicyOutput3D, 4> resized{
      Run(60.f, candidates, sphereSweeps, worldConstraints, terrainConstraints,
          9.f / 16.f),
      Run(60.f, candidates, sphereSweeps, worldConstraints, terrainConstraints,
          32.f / 9.f),
      Run(60.f, candidates, sphereSweeps, worldConstraints, terrainConstraints,
          16.f / 9.f, .78f),
      Run(60.f, candidates, sphereSweeps, worldConstraints, terrainConstraints,
          16.f / 9.f, 1.2f, 2.2f)};
  constexpr std::uint64_t expectedProductionQueries=960u*7u;if (candidates != expectedProductionQueries || sphereSweeps != expectedProductionQueries || worldConstraints != expectedProductionQueries ||
      terrainConstraints != expectedProductionQueries)
    return 1;
  for (const auto &output : outputs)
    if (!output.valid || output.usedFallback || !output.clearLineOfSight ||
        !output.subjectFramed || output.arm < output.minimumSubjectArm ||
        output.fadeActive || std::abs(output.fadeAlpha - 1.f) > .0001f) {
      std::cerr << "invalid output valid=" << output.valid
                << " fallback=" << output.usedFallback
                << " clear=" << output.clearLineOfSight
                << " framed=" << output.subjectFramed << " arm=" << output.arm
                << " minimum=" << output.minimumSubjectArm
                << " fade=" << output.fadeActive
                << " alpha=" << output.fadeAlpha << '\n';
      return 2;
    }
  if (outputs[0].candidateId != outputs[1].candidateId ||
      outputs[1].candidateId != outputs[2].candidateId ||
      std::abs(outputs[0].arm - outputs[1].arm) > .001f ||
      std::abs(outputs[1].arm - outputs[2].arm) > .001f)
    return 3;
  for (std::size_t resizedIndex=0;resizedIndex<resized.size();++resizedIndex){const auto& output=resized[resizedIndex];
    if (!output.valid || !output.clearLineOfSight || !output.subjectFramed ||
        output.fadeActive){std::cerr<<"resized="<<resizedIndex<<" valid="<<output.valid<<" clear="<<output.clearLineOfSight<<" framed="<<output.subjectFramed<<" fade="<<output.fadeActive<<'\n';return 4;}}
  std::cout << "status=pass candidates=" << candidates
            << " sphereSweeps=" << sphereSweeps
            << " worldConstraints=" << worldConstraints
            << " terrainConstraints=" << terrainConstraints
            << " selected=" << outputs[0].candidateId.low
            << " arm=" << outputs[0].arm << " opacity=" << outputs[0].fadeAlpha
            << " godRunCoverage=" << runGodLayout.playerCoverage
            << " godRunMaximumCoverage=" << runGodLayout.maximumActualTriangleCoverage
            << " godRunCoarse=" << runGodLayout.coarseCandidates
            << " godRunRaster=" << runGodLayout.rasterCandidates
            << " spawnGodCoverage=" << spawnGod.playerCoverage
            << " spawnGodMaximumCoverage=" << spawnGod.maximumActualTriangleCoverage
            << " spawnGodFov=" << spawnGod.camera.projection.verticalFieldOfView
            << " spawnGodElevation=" << spawnGod.elevation
            << '\n';
}
