#include "GameplayCameraProductionBuilder3D.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace GameplayLab {

bool AcceptGameplayCameraSubjectScreenArea3D(
    float subjectScreenArea, float minimumSubjectScreenArea) noexcept {
  return std::isfinite(subjectScreenArea) &&
         std::isfinite(minimumSubjectScreenArea) &&
         subjectScreenArea >= minimumSubjectScreenArea &&
         subjectScreenArea <= .20f;
}
using namespace Aevrynd;

GameplayCameraGroundHorizonGate3D
EvaluateGameplayThirdPersonGroundHorizon3D(
    CameraProjection projection, float pitchRadians,
    float minimumTopCenterRayElevation) noexcept {
  GameplayCameraGroundHorizonGate3D result{};
  result.minimumTopCenterRayElevation = minimumTopCenterRayElevation;
  result.topCenterRayElevation =
      projection.verticalFieldOfView * .5f - pitchRadians;
  result.accepted = projection.mode == ProjectionMode::Perspective &&
                    std::isfinite(projection.verticalFieldOfView) &&
                    projection.verticalFieldOfView > 0.f &&
                    std::isfinite(pitchRadians) &&
                    std::isfinite(minimumTopCenterRayElevation) &&
                    minimumTopCenterRayElevation >= 0.f &&
                    std::isfinite(result.topCenterRayElevation) &&
                    result.topCenterRayElevation >= minimumTopCenterRayElevation;
  return result;
}

float DeriveGameplayMultiActorCameraArm3D(
    float baseArm, MeshBounds3D playerWorldBounds,
    std::span<const GameplayCameraCompositionActor3D> actors) noexcept {
  if (!std::isfinite(baseArm) || baseArm <= 0.f ||
      !std::isfinite(playerWorldBounds.center.x) ||
      !std::isfinite(playerWorldBounds.center.y) ||
      !std::isfinite(playerWorldBounds.center.z) ||
      !std::isfinite(playerWorldBounds.extents.x) ||
      !std::isfinite(playerWorldBounds.extents.y) ||
      !std::isfinite(playerWorldBounds.extents.z) ||
      playerWorldBounds.extents.x <= 0.f ||
      playerWorldBounds.extents.y <= 0.f ||
      playerWorldBounds.extents.z <= 0.f)
    return 0.f;
  Vec3 minimum=playerWorldBounds.center-playerWorldBounds.extents;
  Vec3 maximum=playerWorldBounds.center+playerWorldBounds.extents;
  for(const auto& actor:actors){
    if(!std::isfinite(actor.center.x)||!std::isfinite(actor.center.y)||
       !std::isfinite(actor.center.z)||!std::isfinite(actor.extents.x)||
       !std::isfinite(actor.extents.y)||!std::isfinite(actor.extents.z)||
       actor.extents.x<=0.f||actor.extents.y<=0.f||actor.extents.z<=0.f)
      return 0.f;
    minimum.x=(std::min)(minimum.x,actor.center.x-actor.extents.x);
    minimum.y=(std::min)(minimum.y,actor.center.y-actor.extents.y);
    minimum.z=(std::min)(minimum.z,actor.center.z-actor.extents.z);
    maximum.x=(std::max)(maximum.x,actor.center.x+actor.extents.x);
    maximum.y=(std::max)(maximum.y,actor.center.y+actor.extents.y);
    maximum.z=(std::max)(maximum.z,actor.center.z+actor.extents.z);
  }
  const Vec3 unionExtents=(maximum-minimum)*.5f;
  const float playerHorizontal=std::sqrt(playerWorldBounds.extents.x*playerWorldBounds.extents.x+playerWorldBounds.extents.z*playerWorldBounds.extents.z);
  const float unionHorizontal=std::sqrt(unionExtents.x*unionExtents.x+unionExtents.z*unionExtents.z);
  if(!std::isfinite(playerHorizontal)||!std::isfinite(unionHorizontal))return 0.f;
  return baseArm+(std::clamp)(unionHorizontal-playerHorizontal,0.f,1.5f);
}

GameplayCameraProductionBuildOutput3D
BuildGameplayCameraProductionCandidates3D(
    const GameplayCameraProductionBuildInput3D &input,
    const GameplayCameraProductionQueryAdapters3D &queries) {
  GameplayCameraProductionBuildOutput3D output{};
  if (input.bindVertices.empty() || !queries.environmentSphereSweep ||
      !queries.collisionWorldMaxArm || !queries.heightFieldMaxArm ||
      !queries.roofOrTopBlocked || !std::isfinite(input.characterYaw) ||
      !std::isfinite(input.desiredArm) || input.desiredArm <= 0.f ||
      !std::isfinite(input.sweepRadius) || input.sweepRadius <= 0.f ||
      !std::isfinite(input.minimumSubjectScreenHeight) || input.minimumSubjectScreenHeight<.35f || input.minimumSubjectScreenHeight>.70f ||
      !std::isfinite(input.minimumSubjectScreenArea) || input.minimumSubjectScreenArea<.02f || input.minimumSubjectScreenArea>.20f ||
      !std::isfinite(input.maximumBuildingUnionCoverage) || input.maximumBuildingUnionCoverage<0.f || input.maximumBuildingUnionCoverage>1.f ||
      !std::isfinite(input.maximumBuildingPartCoverage) || input.maximumBuildingPartCoverage<0.f || input.maximumBuildingPartCoverage>1.f ||
      !std::isfinite(input.maximumRoofCoverage) || input.maximumRoofCoverage<0.f || input.maximumRoofCoverage>1.f ||
      !std::isfinite(input.maximumSubjectOcclusion) || input.maximumSubjectOcclusion<0.f || input.maximumSubjectOcclusion>1.f ||
      !std::isfinite(input.minimumTopCenterRayElevation) || input.minimumTopCenterRayElevation<.03f || input.minimumTopCenterRayElevation>.25f ||
      !std::isfinite(input.viewport.x) || !std::isfinite(input.viewport.y) ||
      input.viewport.x <= 0.f || input.viewport.y <= 0.f ||
      input.projection.mode != ProjectionMode::Perspective ||
      input.candidateIdHigh == 0)
    return output;
  output.receipt.minimumTopCenterRayElevation =
      input.minimumTopCenterRayElevation;
  output.receipt.minimumSubjectScreenArea = input.minimumSubjectScreenArea;
  for(const auto& occluder:input.compositionOccluders)
    if(!occluder.stableLow||!std::isfinite(occluder.center.x)||!std::isfinite(occluder.center.y)||!std::isfinite(occluder.center.z)||!std::isfinite(occluder.size.x)||!std::isfinite(occluder.size.y)||!std::isfinite(occluder.size.z)||!std::isfinite(occluder.yaw)||occluder.size.x<=0.f||occluder.size.y<=0.f||occluder.size.z<=0.f)
      return output;
  for(const auto& actor:input.compositionActors)
    if(!std::isfinite(actor.center.x)||!std::isfinite(actor.center.y)||!std::isfinite(actor.center.z)||
       !std::isfinite(actor.extents.x)||!std::isfinite(actor.extents.y)||!std::isfinite(actor.extents.z)||
       actor.extents.x<=0.f||actor.extents.y<=0.f||actor.extents.z<=0.f||
       std::ranges::any_of(actor.posedVertices,[](const Vertex3D& vertex){return !std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z);}))
      return output;

  Vec3 minimum{std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
  Vec3 maximum{-std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max()};
  for (const auto &vertex : input.bindVertices) {
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
        !std::isfinite(vertex.z))
      return output;
    minimum.x = std::min(minimum.x, vertex.x);
    minimum.y = std::min(minimum.y, vertex.y);
    minimum.z = std::min(minimum.z, vertex.z);
    maximum.x = std::max(maximum.x, vertex.x);
    maximum.y = std::max(maximum.y, vertex.y);
    maximum.z = std::max(maximum.z, vertex.z);
  }
  const Vec3 size = maximum - minimum;
  const Vec3 center = (minimum + maximum) * .5f;
  const float radius = std::max({input.sweepRadius, size.x * .5f,
                                 size.z * .5f});
  if (!std::isfinite(size.y) || size.y <= 0.f || !std::isfinite(radius) ||
      radius <= 0.f)
    return output;
  output.receipt.visualBoundsValid = true;
  const Matrix4 world =
      CreateTransformMatrix(input.feet, {0, input.characterYaw, 0}, {1, 1, 1});
  output.visualFocus = TransformPoint(world, center);
  Vec3 compositionFocus=output.visualFocus;
  if(!input.compositionActors.empty()){
    Vec3 sum=output.visualFocus;
    for(const auto& actor:input.compositionActors)sum=sum+actor.center;
    compositionFocus=sum*(1.f/float(input.compositionActors.size()+1));
  }
  output.subject = {size.y, radius, center.y - minimum.y};
  output.lens = {input.projection.nearPlane,
                 input.projection.verticalFieldOfView, .10f};
  const auto interval = DeriveProductionCameraFramingArmInterval3D(
      output.subject, output.lens, input.viewport.x / input.viewport.y,
      input.desiredArm);
  if (!interval.valid)
    return output;

  constexpr std::array<float, 15> runtimeYawOffsets{0.f,-.25f,.25f,-.35f,.35f,-.45f,.45f,-.55f,.55f,-.8f,.8f,-1.05f,1.05f,-1.57079633f,1.57079633f};
  // 零建築展示只補一個經正式 gate 驗出的正面角度；一般遊玩不支付額外候選成本，
  // 避免鏡頭求解時間改變固定擷取的 frame pacing。
  constexpr std::array<float, 16> cleanShowcaseYawOffsets{0.f,-.25f,.25f,-.35f,.35f,-.45f,.45f,-.55f,.55f,-.8f,.8f,-1.05f,1.05f,-1.57079633f,1.57079633f,2.6f};
  const std::span<const float> yawOffsets=input.useCleanShowcaseYawOffsets||input.maximumBuildingUnionCoverage==0.f
      ?std::span<const float>{cleanShowcaseYawOffsets}
      :std::span<const float>{runtimeYawOffsets};
  constexpr std::array<float, 4> pitches{.12f,.28f,.44f,.60f};
  constexpr std::array<float, 4> elevations{0.f,.45f,.9f,1.35f};
  // 中距候選保留1%的主體可讀性餘量；數量與所有碰撞/構圖gate不變。
  const std::array<float, 4> arms{interval.nearArm,
                                  (std::max)(interval.nearArm,interval.midArm*.90f),
                                  (std::max)(interval.nearArm,interval.midArm*.99f),
                                  interval.farArm};
  const float minimumArm=interval.minimumSubjectArm;
  const std::array<float,3> multiActorFocusFactors{.7f,1.f,1.3f};
  const std::span<const float> focusFactors=input.compositionActors.empty()
      ?std::span<const float>{multiActorFocusFactors.data()+1,1}
      :std::span<const float>{multiActorFocusFactors};
  const std::size_t expectedCandidates=yawOffsets.size()*pitches.size()*elevations.size()*arms.size()*focusFactors.size();
  output.candidates.reserve(expectedCandidates);
  std::uint64_t candidateLow{};
  for (const float focusFactor:focusFactors)
    for (const float yawOffset : yawOffsets)
      for (const float pitch : pitches)
        for (const float elevation : elevations)
          for (const float arm : arms) {
          // 多角色構圖以所有可見演員的中心為正式視線焦點；碰撞 sweep、world/terrain
          // 約束與最終 camera 共用此焦點，避免只移畫面卻留下錯誤 spring-arm 路徑。
          const Vec3 focus = output.visualFocus+(compositionFocus-output.visualFocus)*focusFactor+Vec3{0, elevation, 0};
          const Camera3D camera = MakeThirdPersonCamera(
              focus, input.characterYaw + yawOffset, pitch, arm,
              input.projection);
          ProductionCameraCandidate3D candidate{};
          candidate.candidateId = {input.candidateIdHigh, ++candidateLow};
          candidate.cameraFocus=focus;
          candidate.shoulder = yawOffset < -.001f
                                   ? CameraShoulder3D::Right
                                   : yawOffset > .001f
                                         ? CameraShoulder3D::Left
                                         : CameraShoulder3D::Center;
          candidate.yawOffsetRadians = yawOffset;
          candidate.pitchRadians = pitch;
          candidate.verticalElevation = elevation;
          candidate.springArmDistance = arm;
          candidate.sweepRadius = radius;
          const auto horizon = EvaluateGameplayThirdPersonGroundHorizon3D(
              input.projection, pitch, input.minimumTopCenterRayElevation);
          candidate.environmentSphereSweep = queries.environmentSphereSweep(
              focus, candidate.sweepRadius, camera.position - focus);
          ++output.receipt.environmentSweeps;
          candidate.collisionWorldMaxArm = queries.collisionWorldMaxArm(camera, focus);
          ++output.receipt.collisionWorldConstraints;
          candidate.heightFieldMaxArm = queries.heightFieldMaxArm(camera, focus);
          ++output.receipt.heightFieldConstraints;
          candidate.roofOrTopBlocked = queries.roofOrTopBlocked(camera);
          ++output.receipt.roofQueries;

          float minX = input.viewport.x, minY = input.viewport.y, maxX = 0.f,
                maxY = 0.f;
          bool projected = true;
          for (unsigned corner = 0; corner < 8; ++corner) {
            const Vec3 local{corner & 1 ? maximum.x : minimum.x,
                             corner & 2 ? maximum.y : minimum.y,
                             corner & 4 ? maximum.z : minimum.z};
            const auto point =
                ProjectToViewport(TransformPoint(world, local), camera,
                                  input.viewport);
            projected &= point.visible;
            minX = std::min(minX, point.position.x);
            minY = std::min(minY, point.position.y);
            maxX = std::max(maxX, point.position.x);
            maxY = std::max(maxY, point.position.y);
          }
          candidate.subjectScreenHeight =
              projected ? Clamp((maxY - minY) / input.viewport.y, 0.f, 1.f) : 1.f;
          candidate.subjectScreenWidth =
              projected ? Clamp((maxX - minX) / input.viewport.x, 0.f, 1.f) : 1.f;
          candidate.subjectScreenArea =
              candidate.subjectScreenHeight * candidate.subjectScreenWidth;
          candidate.headroom = projected ? Clamp(minY / input.viewport.y, 0.f, 1.f) : 0.f;
          candidate.feetOnScreen = projected && maxY >= 0.f &&
                                   maxY <= input.viewport.y && minX < input.viewport.x &&
                                   maxX > 0.f;
          candidate.actorMinimumMargin=projected?(std::min)({minX/input.viewport.x,minY/input.viewport.y,1.f-maxX/input.viewport.x,1.f-maxY/input.viewport.y}):0.f;
          candidate.actorGapPixels=(std::numeric_limits<float>::max)();
          bool actorsProjected=projected;
          for(const auto& actor:input.compositionActors){
            float actorMinX=input.viewport.x,actorMinY=input.viewport.y,actorMaxX=0.f,actorMaxY=0.f;bool actorProjected=true;
            const auto consumePoint=[&](Vec3 point){const auto projectedPoint=ProjectToViewport(point,camera,input.viewport);actorProjected&=projectedPoint.visible;actorMinX=(std::min)(actorMinX,projectedPoint.position.x);actorMinY=(std::min)(actorMinY,projectedPoint.position.y);actorMaxX=(std::max)(actorMaxX,projectedPoint.position.x);actorMaxY=(std::max)(actorMaxY,projectedPoint.position.y);};
            if(!actor.posedVertices.empty())for(const auto& vertex:actor.posedVertices)consumePoint(TransformPoint(actor.world,{vertex.x,vertex.y,vertex.z}));
            else for(unsigned corner=0;corner<8;++corner)consumePoint({actor.center.x+(corner&1?actor.extents.x:-actor.extents.x),actor.center.y+(corner&2?actor.extents.y:-actor.extents.y),actor.center.z+(corner&4?actor.extents.z:-actor.extents.z)});
            actorsProjected&=actorProjected;
            const float margin=actorProjected?(std::min)({actorMinX/input.viewport.x,actorMinY/input.viewport.y,1.f-actorMaxX/input.viewport.x,1.f-actorMaxY/input.viewport.y}):0.f;
            candidate.actorMinimumMargin=(std::min)(candidate.actorMinimumMargin,margin);
            const float gap=maxX<actorMinX?actorMinX-maxX:actorMaxX<minX?minX-actorMaxX:0.f;
            candidate.actorGapPixels=(std::min)(candidate.actorGapPixels,gap);
          }
          if(input.compositionActors.empty())candidate.actorGapPixels=64.f;
          candidate.allActorsOnScreen=actorsProjected&&candidate.actorMinimumMargin>=.05f;
          constexpr std::uint32_t compositionWidth=20,compositionHeight=12;
          struct CompositionMask{std::uint64_t stableLow{};float depth{};std::vector<std::uint8_t> pixels;};
          std::vector<CompositionMask> masks;
          std::vector<std::uint64_t> visibleCounts(157);
          const Matrix4 compositionView=CreateViewMatrix(camera);
          bool compositionValid=true;
          for(const auto& occluder:input.compositionOccluders){
            GameplayOccluderMask3D mask{};
            ++output.receipt.compositionEvaluations;
            if(!SphereIntersectsFrustum(occluder.center,Length(occluder.size)*.5f,camera,input.viewport.x/input.viewport.y))continue;
            // 完全離開viewport的合法物件回傳false，等同零覆蓋，不是候選失效。
            (void)AnalyzeGameplayOccluderMask3D(camera,std::span<const GameplayOccluderBox3D>(&occluder,1),compositionWidth,compositionHeight,mask);
            if(occluder.stableLow!=151&& !mask.pixels.empty())masks.push_back({occluder.stableLow,TransformPoint(compositionView,occluder.center).z,std::move(mask.pixels)});
          }
          const std::size_t compositionPixels=std::size_t(compositionWidth)*compositionHeight;
          std::vector<std::uint8_t> compositionUnion(compositionPixels);
          std::vector<float> nearestDepth(compositionPixels,(std::numeric_limits<float>::infinity)());
          std::vector<std::uint64_t> nearestId(compositionPixels);
          for(const auto& mask:masks)for(std::size_t pixel=0;pixel<compositionPixels;++pixel)if(mask.pixels[pixel]&&mask.depth<nearestDepth[pixel]){nearestDepth[pixel]=mask.depth;nearestId[pixel]=mask.stableLow;}
          std::uint64_t unionPixels{};for(std::size_t pixel=0;pixel<compositionPixels;++pixel)if(nearestId[pixel]){compositionUnion[pixel]=255;++unionPixels;if(nearestId[pixel]<visibleCounts.size())++visibleCounts[nearestId[pixel]];}
          candidate.buildingUnionCoverage=float(unionPixels)/float(compositionPixels);
          for(std::uint64_t id=152;id<=155;++id)candidate.maximumWallCoverage=(std::max)(candidate.maximumWallCoverage,float(visibleCounts[id])/float(compositionPixels));candidate.roofCoverage=float(visibleCounts[156])/float(compositionPixels);
          if(projected&&compositionValid){
            const auto pixelClamp=[](float value,int maximum){return (std::clamp)(int(value),0,maximum);};
            const int left=pixelClamp(std::floor(minX/input.viewport.x*float(compositionWidth)),int(compositionWidth)-1),right=pixelClamp(std::ceil(maxX/input.viewport.x*float(compositionWidth)),int(compositionWidth)-1),top=pixelClamp(std::floor(minY/input.viewport.y*float(compositionHeight)),int(compositionHeight)-1),bottom=pixelClamp(std::ceil(maxY/input.viewport.y*float(compositionHeight)),int(compositionHeight)-1);
            std::uint64_t overlap{},subjectPixels{};
            const float subjectDepth=TransformPoint(compositionView,output.visualFocus).z;
            for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){++subjectPixels;const auto pixel=std::size_t(y)*compositionWidth+std::size_t(x);overlap+=compositionUnion[pixel]&&nearestDepth[pixel]<subjectDepth?1u:0u;}
            candidate.subjectOcclusionCoverage=subjectPixels?float(overlap)/float(subjectPixels):1.f;
          }else candidate.subjectOcclusionCoverage=1.f;
          output.receipt.minimumBuildingUnion=(std::min)(output.receipt.minimumBuildingUnion,candidate.buildingUnionCoverage);output.receipt.minimumWallCoverage=(std::min)(output.receipt.minimumWallCoverage,candidate.maximumWallCoverage);output.receipt.minimumRoofCoverage=(std::min)(output.receipt.minimumRoofCoverage,candidate.roofCoverage);output.receipt.minimumSubjectOcclusion=(std::min)(output.receipt.minimumSubjectOcclusion,candidate.subjectOcclusionCoverage);
          const float environmentArm=candidate.environmentSphereSweep.blocked?candidate.environmentSphereSweep.hitDistance:candidate.springArmDistance;
          const bool environmentSafe=environmentArm>=minimumArm,worldSafe=candidate.collisionWorldMaxArm>=minimumArm,terrainSafe=candidate.heightFieldMaxArm>=minimumArm;
          const bool heightValid=candidate.subjectScreenHeight>=input.minimumSubjectScreenHeight&&candidate.subjectScreenHeight<=.70f,widthValid=candidate.subjectScreenWidth<=.9f,areaValid=AcceptGameplayCameraSubjectScreenArea3D(candidate.subjectScreenArea,input.minimumSubjectScreenArea),headroomValid=candidate.headroom>=.05f,unionValid=compositionValid&&candidate.buildingUnionCoverage<=input.maximumBuildingUnionCoverage,wallValid=compositionValid&&candidate.maximumWallCoverage<=input.maximumBuildingPartCoverage,roofCoverageValid=compositionValid&&candidate.roofCoverage<=input.maximumRoofCoverage,subjectClear=compositionValid&&candidate.subjectOcclusionCoverage<=input.maximumSubjectOcclusion,actorBoundsValid=candidate.allActorsOnScreen,actorGapValid=candidate.actorGapPixels>=64.f,horizonValid=horizon.accepted;
          output.receipt.rejectedEnvironment+=environmentSafe?0u:1u;output.receipt.rejectedCollisionWorld+=worldSafe?0u:1u;output.receipt.rejectedHeightField+=terrainSafe?0u:1u;output.receipt.rejectedRoof+=candidate.roofOrTopBlocked?1u:0u;output.receipt.rejectedHeight+=heightValid?0u:1u;output.receipt.rejectedWidth+=widthValid?0u:1u;output.receipt.rejectedArea+=areaValid?0u:1u;output.receipt.rejectedHeadroom+=headroomValid?0u:1u;output.receipt.rejectedFeet+=candidate.feetOnScreen?0u:1u;
          output.receipt.rejectedBuildingUnion+=unionValid?0u:1u;output.receipt.rejectedBuildingPart+=wallValid?0u:1u;output.receipt.rejectedRoofCoverage+=roofCoverageValid?0u:1u;output.receipt.rejectedSubjectOcclusion+=subjectClear?0u:1u;
          output.receipt.rejectedActorBounds+=actorBoundsValid?0u:1u;output.receipt.rejectedActorGap+=actorGapValid?0u:1u;
          output.receipt.rejectedHorizon+=horizonValid?0u:1u;
          const bool safe=environmentSafe&&worldSafe&&terrainSafe;if(safe)++output.receipt.safeCandidates;const bool compositionAdmitted=unionValid&&wallValid&&roofCoverageValid&&subjectClear&&actorBoundsValid&&actorGapValid&&heightValid&&areaValid&&horizonValid;candidate.compositionAdmitted=compositionAdmitted;const bool framed=heightValid&&widthValid&&areaValid&&headroomValid&&candidate.feetOnScreen&&actorBoundsValid&&actorGapValid&&horizonValid;if(framed)++output.receipt.framedCandidates;if(safe&&framed&&compositionAdmitted&&!candidate.roofOrTopBlocked){++output.receipt.admittedCandidates;if(output.receipt.minimumAcceptedTopCenterRayElevation==0.f)output.receipt.minimumAcceptedTopCenterRayElevation=horizon.topCenterRayElevation;else output.receipt.minimumAcceptedTopCenterRayElevation=(std::min)(output.receipt.minimumAcceptedTopCenterRayElevation,horizon.topCenterRayElevation);}if(compositionValid)++output.receipt.compositionCandidates;
          output.receipt.projectionValid |= projected;
          ++output.receipt.completeIdentities;
          if (!candidate.environmentSphereSweep.blocked ||
              (candidate.environmentSphereSweep.generation != 0 &&
               (candidate.environmentSphereSweep.occluder.high != 0 ||
                candidate.environmentSphereSweep.occluder.low != 0)))
            ++output.receipt.blockedWithGeneration;
          output.candidates.push_back(candidate);
        }
  output.receipt.candidates = output.candidates.size();
  output.receipt.valid = output.receipt.candidates == expectedCandidates &&
                         output.receipt.environmentSweeps == expectedCandidates &&
                         output.receipt.collisionWorldConstraints == expectedCandidates &&
                         output.receipt.heightFieldConstraints == expectedCandidates &&
                         output.receipt.roofQueries == expectedCandidates &&
                         output.receipt.completeIdentities == expectedCandidates &&
                         output.receipt.blockedWithGeneration == expectedCandidates &&
                         output.receipt.compositionCandidates == expectedCandidates &&
                         output.receipt.projectionValid;
  if (!output.receipt.valid)
    output.candidates.clear();
  return output;
}

} // namespace GameplayLab
