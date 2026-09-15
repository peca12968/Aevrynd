#pragma once
#include "GameplayRules3D.h"
#include "GameplayInputReplay3D.h"
#include "GameplayAnimation3D.h"
#include "GameplayCombat3D.h"
#include "GameplayShowcaseEnvironment3D.h"
#include "GameplayHlodWorldRuntime3D.h"
#include "ShowcaseRenderAssetBindings3D.h"
#include "GameplayObjectPaletteUi3D.h"
#include "GameplayCameraOcclusionPolicy3D.h"
#include "GameplayCameraProductionBuilder3D.h"
#include "GameplayFirstPersonViewmodel3D.h"
#include "GameplayFirstPersonPose3D.h"
#include "Candidate7PresentationScale3D.h"
#include "ObjectPaletteAdapter3D.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/EnvironmentCollisionStreaming3D.h>
#include <Aevrynd/PhysicsWorld3D.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/FootPlacement3D.h>
#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkinnedRenderable3D.h>
#include <filesystem>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <optional>
#include <span>

namespace GameplayLab {
// geometry 只接受初始化時已由正式裝備頂點驗證的契約；失敗保留呼叫端輸出。
inline bool ResolveGameplaySwordTipWorld3D(const Aevrynd::CharacterSwordGeometry3D& geometry,
    const Aevrynd::Matrix4& handWorld,Aevrynd::Vec3& output) noexcept {
    if(!geometry.available)return false;
    for(const auto& row:handWorld.values)for(float value:row)if(!std::isfinite(value))return false;
    const auto tip=Aevrynd::TransformPoint(handWorld,geometry.bladeTip);
    if(!std::isfinite(tip.x)||!std::isfinite(tip.y)||!std::isfinite(tip.z))return false;
    output=tip;return true;
}

struct GameplayHudPresentationPolicy3D {
    bool instructions{};
    bool palette{};
    bool combatStatus{};
    bool interactionDiagnostics{};
};

constexpr GameplayHudPresentationPolicy3D BuildGameplayHudPresentationPolicy3D(
    bool diagnosticsVisible, bool paletteOpen,
    bool cleanCapturePresentation = false) noexcept {
    if (cleanCapturePresentation) return {};
    // 遊玩與擷取共用預設精簡模式；使用者可主動展開診斷，palette 仍依操作狀態顯示。
    return {diagnosticsVisible, paletteOpen, diagnosticsVisible, diagnosticsVisible};
}
constexpr bool GameplayHudShowsCompactInstructions3D(
    const GameplayHudPresentationPolicy3D &policy,
    bool cleanCapturePresentation = false) noexcept {
    return !cleanCapturePresentation && !policy.instructions;
}
static_assert([] {
    constexpr auto clean = BuildGameplayHudPresentationPolicy3D(true, true, true);
    return !clean.instructions && !clean.palette && !clean.combatStatus &&
           !clean.interactionDiagnostics &&
           !GameplayHudShowsCompactInstructions3D(clean, true);
}());
}
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GameplayLab {
struct GameplayCanonicalCaptureStateInput3D {
    std::size_t captureIndex{};std::vector<Entity> entities{};Aevrynd::Vec3 controllerPosition{};float yaw{},pitch{};
    CameraMode camera{};LightingPhase lighting{};PaletteCategory paletteCategory{};bool female{},paletteOpen{};
    std::uint64_t grabbedEntity{},cameraHash{},animationHash{},physicsHash{};
};
struct GameplayCanonicalCaptureStateReceipt3D {
    static constexpr std::uint32_t SchemaVersion=2;std::uint32_t schemaVersion{SchemaVersion};
    struct EntityDiagnostic {std::uint64_t stableId{};PaletteCategory category{};Aevrynd::Vec3 position{};float rotation{};std::array<std::uint32_t,4> rawBits{};bool dynamic{},alive{},initialSeed{},bodyAvailable{},sceneAvailable{};Aevrynd::Vec3 bodyPosition{},bodyVelocity{},scenePosition{};std::uint64_t topologyHash{},transformHash{};};
    bool valid{};std::vector<EntityDiagnostic> entityDiagnostics{};
    std::uint64_t entities{},topology{},transform{},controller{},yawPitch{},rules{},camera{},palette{},animation{},physics{},canonical{},legacy{};
};
struct CaptureReplayInputSnapshot3D {bool observed{},liveSuppressed{};std::uint64_t tick{},inputBits{},cameraHash{};Aevrynd::Camera3D camera{};Aevrynd::Vec2 normalizedPointer{};Aevrynd::Ray3D ray{};};
struct GameplayPaletteReplayDiagnostic3D {bool observed{},liveSuppressed{},previewActive{},previewValid{},hitValid{};std::uint64_t tick{},inputBits{},cameraHash{},cameraAuthorityInputHash{},cameraSubjectPoseHash{};float gameplayDelta{},placementPlaneHeight{},gridStep{2.f},footPlacementCameraDistance{};Aevrynd::Vec2 normalizedPointer{};Aevrynd::Vec3 cameraPosition{},cameraTarget{},cameraUp{},rayOrigin{},rayDirection{},projectedHit{},snappedTarget{};Aevrynd::ProjectionMode projectionMode{Aevrynd::ProjectionMode::Perspective};std::array<std::uint32_t,13> cameraRawBits{};std::size_t undoDepth{},redoDepth{};};
[[nodiscard]] constexpr bool ValidateGameplayFrameBoundRenderPair3D(const Aevrynd::FrameBoundDeterminismEvidence3D& object,const Aevrynd::FrameBoundDeterminismEvidence3D& depth,const Aevrynd::ReadbackRasterProvenance3D& objectRaster,const Aevrynd::ReadbackRasterProvenance3D& depthRaster,const Aevrynd::ObjectIdCaptureRequestMetadata3D& objectRequest,const Aevrynd::ObjectIdCaptureRequestMetadata3D& depthRequest)noexcept{return object.Complete()&&depth.Complete()&&object==depth&&objectRaster==depthRaster&&objectRequest==depthRequest&&object.frameSerial==objectRaster.frameSerial&&object.render.frameSerial==object.frameSerial&&object.render.sceneDrawRevision==objectRequest.drawListRevision;}
enum class GameplayCaptureEvidenceScope3D:std::uint8_t{Full,ThirdDayTargeted};
[[nodiscard]] constexpr bool GameplayCaptureScopeExpectsShot3D(GameplayCaptureEvidenceScope3D scope,std::size_t index)noexcept{return scope==GameplayCaptureEvidenceScope3D::Full?index<11:index==1;}
template<std::size_t N>[[nodiscard]] constexpr bool ValidateGameplayCaptureEvidenceScopeMask3D(GameplayCaptureEvidenceScope3D scope,const std::array<bool,N>& completed)noexcept{for(std::size_t index=0;index<N;++index)if(completed[index]!=GameplayCaptureScopeExpectsShot3D(scope,index))return false;return true;}
template<std::size_t N>[[nodiscard]] bool ValidateGameplayCaptureEvidenceScope3D(GameplayCaptureEvidenceScope3D scope,const std::array<Aevrynd::ReadbackCaptureSequenceReceipt3D,N>& receipts)noexcept{for(std::size_t index=0;index<N;++index){const bool complete=receipts[index].Complete();if(complete!=GameplayCaptureScopeExpectsShot3D(scope,index))return false;if(complete&&(receipts[index].phases.size()<2||!receipts[index].phases[1].determinism.render.Complete()))return false;}return true;}
[[nodiscard]] inline std::uint64_t HashGameplayCapturePose3D(const Aevrynd::AnimationPose3D& pose)noexcept{std::uint64_t hash=14695981039346656037ull;const auto mix=[&](std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};mix(pose.globalMatrices.size());for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row){if(!std::isfinite(value))return 0;mix(std::bit_cast<std::uint32_t>(value==0.f?0.f:value));}return hash;}
[[nodiscard]] inline std::uint64_t HashGameplayCaptureFootPlacement3D(const Aevrynd::FootPlacementResult3D& value)noexcept{std::uint64_t hash=14695981039346656037ull;const auto mix=[&](std::uint64_t item){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(item>>(byte*8));hash*=1099511628211ull;}};const auto mixFloat=[&](float item){if(!std::isfinite(item))return false;mix(std::bit_cast<std::uint32_t>(item==0.f?0.f:item));return true;};mix(unsigned(value.lod));for(const auto* foot:{&value.left,&value.right}){mix(unsigned(foot->source));mix(foot->userData);for(const Aevrynd::Vec3 vector:{foot->contactWorld,foot->targetWorld,foot->normalWorld})for(float component:{vector.x,vector.y,vector.z})if(!mixFloat(component))return 0;if(!mixFloat(foot->weight))return 0;}if(!mixFloat(value.pelvisOffsetWorld))return 0;mix(value.ikSolves);mix(value.applied?1u:0u);return hash;}
struct GameplayPresentationPoseSelection3D {const Aevrynd::AnimationPose3D* female{};const Aevrynd::AnimationPose3D* male{};const Aevrynd::AnimationPose3D* monster{};};
// Presentation/paused staging只能取得const snapshot指標；呼叫次數不可能改變simulation authority。
[[nodiscard]] constexpr GameplayPresentationPoseSelection3D SelectGameplayPresentationPoses3D(bool frozen,const Aevrynd::AnimationPose3D& female,const Aevrynd::AnimationPose3D& male,const Aevrynd::AnimationPose3D& monster,const Aevrynd::AnimationPose3D& frozenFemale,const Aevrynd::AnimationPose3D& frozenMale,const Aevrynd::AnimationPose3D& frozenMonster)noexcept{return frozen?GameplayPresentationPoseSelection3D{&frozenFemale,&frozenMale,&frozenMonster}:GameplayPresentationPoseSelection3D{&female,&male,&monster};}
[[nodiscard]] inline GameplayCanonicalCaptureStateReceipt3D BuildGameplayCanonicalCaptureState3D(const GameplayCanonicalCaptureStateInput3D& input)noexcept{
    constexpr std::uint64_t seed=14695981039346656037ull;const auto mix=[](std::uint64_t& hash,std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};const auto canonicalFloat=[](float value){return value==0.f?0.f:value;};const auto mixFloat=[&](std::uint64_t& hash,float value){mix(hash,std::bit_cast<std::uint32_t>(canonicalFloat(value)));};const auto begin=[&](std::string_view tag){std::uint64_t hash=seed;mix(hash,tag.size());for(const unsigned char value:tag){hash^=value;hash*=1099511628211ull;}return hash;};GameplayCanonicalCaptureStateReceipt3D out{};auto entities=input.entities;std::ranges::sort(entities,{},&Entity::id);
    const auto finite=[](const Entity& entity){return std::isfinite(entity.position.x)&&std::isfinite(entity.position.y)&&std::isfinite(entity.position.z)&&std::isfinite(entity.rotation);};if(!std::isfinite(input.controllerPosition.x)||!std::isfinite(input.controllerPosition.y)||!std::isfinite(input.controllerPosition.z)||!std::isfinite(input.yaw)||!std::isfinite(input.pitch)||std::ranges::any_of(entities,[&](const Entity& entity){return !entity.id||!finite(entity);}))return out;
    out.valid=true;out.topology=begin("capture-state/topology/v1");mix(out.topology,entities.size());out.transform=begin("capture-state/transform/v1");mix(out.transform,entities.size());out.entityDiagnostics.reserve(entities.size());for(const auto& entity:entities){GameplayCanonicalCaptureStateReceipt3D::EntityDiagnostic diagnostic{};diagnostic.stableId=entity.id;diagnostic.category=entity.category;diagnostic.position={canonicalFloat(entity.position.x),canonicalFloat(entity.position.y),canonicalFloat(entity.position.z)};diagnostic.rotation=canonicalFloat(entity.rotation);diagnostic.rawBits={std::bit_cast<std::uint32_t>(diagnostic.position.x),std::bit_cast<std::uint32_t>(diagnostic.position.y),std::bit_cast<std::uint32_t>(diagnostic.position.z),std::bit_cast<std::uint32_t>(diagnostic.rotation)};diagnostic.dynamic=entity.dynamic;diagnostic.alive=entity.alive;diagnostic.topologyHash=begin("capture-state/entity-topology/v1");mix(diagnostic.topologyHash,entity.id);mix(diagnostic.topologyHash,unsigned(entity.category));mix(diagnostic.topologyHash,entity.dynamic?1u:0u);mix(diagnostic.topologyHash,entity.alive?1u:0u);diagnostic.transformHash=begin("capture-state/entity-transform/v1");mix(diagnostic.transformHash,entity.id);for(const auto bits:diagnostic.rawBits)mix(diagnostic.transformHash,bits);out.entityDiagnostics.push_back(diagnostic);mix(out.topology,entity.id);mix(out.topology,unsigned(entity.category));mix(out.topology,entity.dynamic?1u:0u);mix(out.topology,entity.alive?1u:0u);mix(out.transform,entity.id);for(const auto bits:diagnostic.rawBits)mix(out.transform,bits);}out.entities=begin("capture-state/entities/v1");mix(out.entities,out.topology);mix(out.entities,out.transform);
    out.controller=begin("capture-state/controller/v1");mixFloat(out.controller,input.controllerPosition.x);mixFloat(out.controller,input.controllerPosition.y);mixFloat(out.controller,input.controllerPosition.z);out.yawPitch=begin("capture-state/yaw-pitch/v1");mixFloat(out.yawPitch,input.yaw);mixFloat(out.yawPitch,input.pitch);out.rules=begin("capture-state/rules/v1");mix(out.rules,input.captureIndex);mix(out.rules,unsigned(input.camera));mix(out.rules,unsigned(input.lighting));mix(out.rules,input.female?1u:0u);out.camera=begin("capture-state/camera/v1");mix(out.camera,input.cameraHash);out.palette=begin("capture-state/palette/v1");mix(out.palette,unsigned(input.paletteCategory));mix(out.palette,input.paletteOpen?1u:0u);mix(out.palette,input.grabbedEntity);out.animation=begin("capture-state/animation/v1");mix(out.animation,input.animationHash);out.physics=begin("capture-state/physics/v1");mix(out.physics,input.physicsHash);
    out.canonical=begin("capture-state/final/v1");mix(out.canonical,out.schemaVersion);for(const auto value:{out.entities,out.topology,out.transform,out.controller,out.yawPitch,out.rules,out.camera,out.palette,out.animation,out.physics})mix(out.canonical,value);out.legacy=seed;mix(out.legacy,input.captureIndex);mix(out.legacy,unsigned(input.camera));mix(out.legacy,unsigned(input.lighting));mix(out.legacy,input.female?1u:0u);mix(out.legacy,input.entities.size());for(const auto& entity:input.entities){mix(out.legacy,entity.id);mixFloat(out.legacy,entity.position.x);mixFloat(out.legacy,entity.position.y);mixFloat(out.legacy,entity.position.z);}mixFloat(out.legacy,input.controllerPosition.x);mixFloat(out.legacy,input.controllerPosition.y);mixFloat(out.legacy,input.controllerPosition.z);mixFloat(out.legacy,input.yaw);mixFloat(out.legacy,input.pitch);return out;
}
[[nodiscard]] constexpr std::size_t GameplayCottageFacadeExpectedTriangles3D(bool door,bool pairedWindows)noexcept{return pairedWindows?(door?84u:72u):door?60u:48u;}
[[nodiscard]] inline bool GameplayCottageGlassGapValid3D(float wallOuter,float glassCenter,float glassHalfDepth)noexcept{return std::isfinite(wallOuter)&&std::isfinite(glassCenter)&&std::isfinite(glassHalfDepth)&&glassHalfDepth>0.f&&std::abs(glassCenter-wallOuter)-glassHalfDepth>=.04f;}
enum class GameplayCaptureCamera3D : std::uint8_t { First, Third, God };
enum class GameplayCaptureLighting3D : std::uint8_t { Day, Dusk, Night };
enum class GameplayCaptureLodExpectation3D : std::uint8_t { Stable, Transition };
enum class GameplayTemporalReseedReason3D : std::uint32_t {
    None=0, CameraMode=1u<<0, Character=1u<<1, PoseState=1u<<2,
    ViewProjection=1u<<3, DrawIdentity=1u<<4, CaptureShot=1u<<5
};
// Five sealed frames are required before a production capture. Three frames
// left a one-LSB dependency on the variable startup history in the first shot;
// the extra two frames preserve the strict bit-exact HDR contract instead of
// weakening the cross-process determinism gate.
inline constexpr std::uint32_t GameplayTemporalCaptureWarmupFrames3D=5;
// The player-facing MSAA/TAA presentation is rebuilt only after the diagnostic
// readback sequence has retired.  Its request frame is counted from a fresh
// camera cut, never from the (driver-dependent) number of asynchronous readback
// wait frames that preceded it.
inline constexpr std::uint32_t GameplayPresentationCaptureWarmupFrames3D=5;
[[nodiscard]] constexpr bool GameplayPresentationCaptureRequestDue3D(
    bool diagnosticSequenceComplete,std::uint32_t presentationWarmupFrames,
    std::uint32_t asynchronousReadbackWaitFrames)noexcept{
    (void)asynchronousReadbackWaitFrames;
    return diagnosticSequenceComplete&&
        presentationWarmupFrames==GameplayPresentationCaptureWarmupFrames3D;
}
[[nodiscard]] constexpr std::uint32_t GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D reason)noexcept{return std::uint32_t(reason);}
[[nodiscard]] inline bool GameplayTemporalViewDiscontinuity3D(const Aevrynd::Camera3D& previous,const Aevrynd::Camera3D& current)noexcept{
    if(previous.projection.mode!=current.projection.mode)return true;
    if(!std::isfinite(previous.projection.verticalFieldOfView)||!std::isfinite(current.projection.verticalFieldOfView)||
       !std::isfinite(previous.projection.orthographicHeight)||!std::isfinite(current.projection.orthographicHeight))return true;
    return Aevrynd::Length(previous.position-current.position)>1.f||Aevrynd::Length(previous.target-current.target)>1.f||
           std::abs(previous.projection.verticalFieldOfView-current.projection.verticalFieldOfView)>.1f||
           std::abs(previous.projection.orthographicHeight-current.projection.orthographicHeight)>.5f;
}
[[nodiscard]] constexpr bool GameplayTemporalCaptureReady3D(bool active,bool poseFrozen,std::uint32_t remaining)noexcept{return active&&poseFrozen&&!remaining;}
[[nodiscard]] constexpr bool GameplayCaptureWarmupMayBegin3D(bool preparedSceneSubmitted,std::uint32_t bufferedFrameIndex)noexcept{return preparedSceneSubmitted&&bufferedFrameIndex==0;}
[[nodiscard]] constexpr bool GameplayUseFrozenPose3D(bool poseFrozen,bool temporalFixture,bool pairFrozen,bool warmupActive,bool readyToStage)noexcept{return poseFrozen&&(temporalFixture||pairFrozen||warmupActive||readyToStage);}
[[nodiscard]] constexpr bool GameplayGodCameraRequiresCaptureLayout3D(bool readyToStage,GameplayCaptureCamera3D expected)noexcept{return readyToStage&&expected==GameplayCaptureCamera3D::God;}
[[nodiscard]] constexpr std::uint64_t GameplayTemporalWarmupScheduleTick3D(std::uint64_t frozenTick)noexcept{return frozenTick;}
[[nodiscard]] constexpr bool GameplayCaptureProjectionRecordingActive3D(bool pairFrozen,std::size_t captureIndex,std::size_t captureCount)noexcept{return pairFrozen&&captureIndex<captureCount;}
inline void PrepareGameplayCaptureStaging3D(Rules3D& rules,bool captureMode)noexcept{if(captureMode)rules.SetCamera(CameraMode::FirstPerson);}
[[nodiscard]] inline bool GameplayCameraSelectionSettled3D(
    const CameraOcclusionPolicyOutput3D& policy,
    const ProductionCameraCandidate3D& candidate,float tolerance=.001f)noexcept{
    return policy.valid&&candidate.candidateId==policy.candidateId&&std::isfinite(tolerance)&&tolerance>=0.f&&
        std::abs(policy.yawOffsetRadians-candidate.yawOffsetRadians)<=tolerance&&
        std::abs(policy.pitchRadians-candidate.pitchRadians)<=tolerance&&
        std::abs(policy.verticalElevation-candidate.verticalElevation)<=tolerance&&
        std::abs(policy.arm-candidate.springArmDistance)<=tolerance;
}

struct GameplayAuthoritativeTrajectoryEvidence3D {
    static constexpr std::uint64_t Seed=14695981039346656037ull;
    [[nodiscard]] bool Record(std::uint64_t fixedTick,Attack3D attack,bool hitWindow,float normalizedPhase,
                              Aevrynd::Vec3 player,Aevrynd::Vec3 handBone,Aevrynd::Vec3 weaponTip) noexcept {
        if(attack==Attack3D::None||fixedTick<=lastTick||!std::isfinite(normalizedPhase)||normalizedPhase<0.f||normalizedPhase>1.f)return false;
        const auto quantize=[](float value,std::int32_t& result){if(!std::isfinite(value)||value<-524287.f||value>524287.f)return false;result=std::int32_t(std::llround(double(value)*4096.0));return true;};
        std::array<std::int32_t,10> values{};
        if(!quantize(normalizedPhase,values[0])||!quantize(player.x,values[1])||!quantize(player.y,values[2])||!quantize(player.z,values[3])||!quantize(handBone.x,values[4])||!quantize(handBone.y,values[5])||!quantize(handBone.z,values[6])||!quantize(weaponTip.x,values[7])||!quantize(weaponTip.y,values[8])||!quantize(weaponTip.z,values[9]))return false;
        const std::size_t attackIndex=std::size_t(attack)-1;if(attackIndex>=perAttackHash.size())return false;
        const auto mix=[](std::uint64_t& hash,std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};
        const auto record=[&](std::uint64_t& hash){mix(hash,fixedTick);mix(hash,std::uint64_t(attack));mix(hash,hitWindow?1u:0u);for(std::int32_t value:values)mix(hash,std::uint32_t(value));};
        record(authoritativeHash);record(perAttackHash[attackIndex]);++checkpointCount;++perAttackCheckpointCount[attackIndex];lastTick=fixedTick;return true;
    }
    std::uint64_t authoritativeHash{Seed};
    std::array<std::uint64_t,3> perAttackHash{{Seed,Seed,Seed}};
    std::uint32_t checkpointCount{};
    std::array<std::uint32_t,3> perAttackCheckpointCount{};
private:
    std::uint64_t lastTick{};
};

struct GameplayGpuContactArtifact3D {
    std::vector<std::uint8_t> rgba;
    std::uint32_t leftFootPixels{},rightFootPixels{},groundSupportPixels{},contactShadowPixels{};
    std::uint32_t leftGapPixels{std::numeric_limits<std::uint32_t>::max()},rightGapPixels{std::numeric_limits<std::uint32_t>::max()};
    float leftDepthDelta{std::numeric_limits<float>::infinity()},rightDepthDelta{std::numeric_limits<float>::infinity()};
    std::uint64_t frameSerial{},poseHash{};
    [[nodiscard]] bool Complete(unsigned width,unsigned height)const noexcept {
        const std::uint64_t classified=std::uint64_t(leftFootPixels)+rightFootPixels+groundSupportPixels+contactShadowPixels;
        return width&&height&&rgba.size()==std::size_t(width)*height*4&&leftFootPixels&&rightFootPixels&&groundSupportPixels&&contactShadowPixels&&
            leftGapPixels<=3&&rightGapPixels<=3&&std::isfinite(leftDepthDelta)&&std::isfinite(rightDepthDelta)&&
            leftDepthDelta>=-.0001f&&leftDepthDelta<=.02f&&rightDepthDelta>=-.0001f&&rightDepthDelta<=.02f&&
            classified<((std::uint64_t(width)*height/20)>512?(std::uint64_t(width)*height/20):512)&&frameSerial&&poseHash;
    }
};

// Weapon swings and attack poses can extend the upper-body silhouette far to one
// side.  Derive the left/right split from the bottom foot band instead of the
// full actor bounds so an outstretched arm cannot move both feet into one half.
struct GameplayGpuFootBand3D {
    unsigned beginY{},minimumX{},maximumX{},leftMaximumX{},rightMinimumX{};
    [[nodiscard]] constexpr bool Complete()const noexcept {
        return minimumX<=leftMaximumX&&leftMaximumX<rightMinimumX&&rightMinimumX<=maximumX;
    }
};

[[nodiscard]] constexpr GameplayGpuFootBand3D SelectGameplayGpuFootBand3D(
    unsigned width,unsigned height,std::span<const std::uint32_t> objectIds,
    std::uint32_t playerToken) noexcept {
    GameplayGpuFootBand3D result{};
    if(!width||!height||!playerToken||objectIds.size()!=std::size_t(width)*height)return result;
    unsigned minimumY=height,maximumY{},actorMinimumX=width,actorMaximumX{};bool found{};
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        if(objectIds[std::size_t(y)*width+x]!=playerToken)continue;
        found=true;minimumY=std::min(minimumY,y);maximumY=std::max(maximumY,y);
        actorMinimumX=std::min(actorMinimumX,x);actorMaximumX=std::max(actorMaximumX,x);
    }
    if(!found)return result;
    constexpr unsigned FootBandHeight=24;
    const unsigned span=maximumY-minimumY+1;
    result.beginY=maximumY-(std::min)(FootBandHeight,span)+1;
    result.minimumX=width;result.maximumX=0;
    unsigned previousX{},largestGap{},leftMaximum{},rightMinimum{};bool previous{};
    for(unsigned x=actorMinimumX;x<=actorMaximumX;++x){
        bool occupied{};for(unsigned y=result.beginY;y<=maximumY&&!occupied;++y)
            occupied=objectIds[std::size_t(y)*width+x]==playerToken;
        if(!occupied)continue;
        result.minimumX=std::min(result.minimumX,x);result.maximumX=std::max(result.maximumX,x);
        if(previous&&x>previousX+1&&x-previousX-1>largestGap){largestGap=x-previousX-1;leftMaximum=previousX;rightMinimum=x;}
        previousX=x;previous=true;
    }
    const unsigned middle=(result.minimumX+result.maximumX)/2;
    if(result.minimumX>result.maximumX||largestGap<2||middle<=leftMaximum||middle>=rightMinimum)return {};
    result.leftMaximumX=leftMaximum;result.rightMinimumX=rightMinimum;return result;
}

[[nodiscard]] constexpr bool GameplayGpuFootBandRegression3D() noexcept {
    std::array<std::uint32_t,32*32> ids{};
    // Hostile upper-body extent: the legacy full-bounds midpoint lies left of
    // both feet.  The bottom-band midpoint must still separate them.
    for(unsigned x=1;x<=12;++x)ids[3*32+x]=1;
    for(unsigned y=24;y<=30;++y)for(unsigned x=18;x<=20;++x)ids[y*32+x]=1;
    for(unsigned y=23;y<=29;++y)for(unsigned x=26;x<=28;++x)ids[y*32+x]=1;
    const auto split=SelectGameplayGpuFootBand3D(32,32,ids,1);
    if(!split.Complete()||split.beginY!=7||split.leftMaximumX!=20||split.rightMinimumX!=26)return false;
    std::array<std::uint32_t,32*32> single{};
    for(unsigned y=24;y<=30;++y)for(unsigned x=18;x<=24;++x)single[y*32+x]=1;
    return !SelectGameplayGpuFootBand3D(32,32,single,1).Complete();
}
static_assert(GameplayGpuFootBandRegression3D());

[[nodiscard]] GameplayGpuContactArtifact3D BuildGameplayGpuContactArtifact3D(
    unsigned width,unsigned height,std::span<const std::uint32_t> objectIds,
    std::span<const float> depth,std::uint32_t playerToken,std::uint32_t terrainToken,
    const Aevrynd::ReadbackRasterProvenance3D& provenance,std::uint64_t poseHash) noexcept;

// 展示敵人仍使用正式戰鬥座標；側向生成讓追蹤與近戰成立，同時避免角色輪廓互相吞沒。
[[nodiscard]] inline Aevrynd::Vec3 GameplayShowcaseMonsterSpawn3D(Aevrynd::Vec3 player,float yaw) noexcept {
    const Aevrynd::Vec3 right{std::cos(yaw),0,-std::sin(yaw)},forward{std::sin(yaw),0,std::cos(yaw)};
    return player+right*1.95f+forward*.2f;
}

// 死亡 collider 已移除後保留可讀的側向擊退姿態；戰鬥命中、生命值與擊殺位置不被竄改。
[[nodiscard]] inline Aevrynd::Vec3 GameplayShowcaseMonsterPresentationPosition3D(
    Aevrynd::Vec3 monster,float,bool) noexcept {
    // 正式AI停止距離已保留雙角色輪廓間隔；生死狀態都使用同一世界座標，避免
    // 死亡展示額外拉遠迫使相機縮小主角。
    return monster;
}

// 角色資產的正面朝向模型空間 -Z。展示敵人必須朝向玩家，而不是使用固定
// yaw 讓走路動畫在不同玩家方向下變成側向滑行。
[[nodiscard]] inline float GameplayShowcaseMonsterFacingYaw3D(
    Aevrynd::Vec3 monster,Aevrynd::Vec3 player) noexcept {
    const Aevrynd::Vec3 towardPlayer=player-monster;
    if(!std::isfinite(towardPlayer.x)||!std::isfinite(towardPlayer.z)||
       towardPlayer.x*towardPlayer.x+towardPlayer.z*towardPlayer.z<1e-8f)
        return 0.f;
    return std::atan2(-towardPlayer.x,-towardPlayer.z);
}

[[nodiscard]] inline Aevrynd::Transform3D GameplayShowcaseMonsterTransform3D(
    Aevrynd::Vec3 monster,Aevrynd::Vec3 player) noexcept {
    return {monster,{0,GameplayShowcaseMonsterFacingYaw3D(monster,player),0},
            {.98f,.98f,.98f}};
}

struct GameplayShowcaseMonsterStage3D {
    float uniformScale{.98f};
    float playerRightOffset{};
};

// God capture 需要同時讓玩家與伴隨角色在遠距正交鏡頭中保持可辨識；此策略只
// 改變錄影展示，不改動戰鬥座標、碰撞、AI 停止距離或一般遊玩比例。
[[nodiscard]] constexpr GameplayShowcaseMonsterStage3D
GameplayShowcaseMonsterStage3DForCapture(bool godCapture) noexcept {
    return godCapture?GameplayShowcaseMonsterStage3D{1.45f,.25f}:
                      GameplayShowcaseMonsterStage3D{};
}

[[nodiscard]] inline Aevrynd::Transform3D GameplayShowcaseMonsterTransform3D(
    Aevrynd::Vec3 monster,Aevrynd::Vec3 player,float playerYaw,
    bool godCapture) noexcept {
    const auto stage=GameplayShowcaseMonsterStage3DForCapture(godCapture);
    const Aevrynd::Vec3 playerRight{std::cos(playerYaw),0,-std::sin(playerYaw)};
    const Aevrynd::Vec3 position=monster+playerRight*stage.playerRightOffset;
    return {position,{0,GameplayShowcaseMonsterFacingYaw3D(position,player),0},
            {stage.uniformScale,stage.uniformScale,stage.uniformScale}};
}

struct GameplayGodCameraLayout3D {
    Aevrynd::Camera3D camera{};
    float distance{},azimuth{},elevation{},playerCoverage{},buildingUnion{},maximumBuildingPart{},roofCoverage{};
    std::uint32_t lodTransitions{},visibleWorldDescriptors{};
    bool roofTouchesTop{},valid{};
    float maximumActualTriangleCoverage{};
    std::uint32_t evaluatedCandidates{},coarseCandidates{},rasterCandidates{},rejectedActualCoverage{},rejectedLod{},rejectedWorld{},rejectedBuilding{};
    float riverCoverage{};
    std::uint32_t visibleBuildingComponents{},buildingEdgeMask{};
    bool productionLodExact{};
};

struct GameplayGodProductionGeometry3D {
    Aevrynd::MeshBounds3D grass{},river{},foundation{},front{},back{},side{},roof{};
};

[[nodiscard]] inline GameplayGodProductionGeometry3D
LegacyGameplayGodProductionGeometry3D(const Aevrynd::MeshBounds3D& grass) noexcept {
    const Aevrynd::MeshBounds3D unit{{},.8660254f,{.5f,.5f,.5f}};
    const Aevrynd::MeshBounds3D river{{},.7071068f,{.5f,0,.5f}};
    const Aevrynd::MeshBounds3D roof{{0,.14f,0},.7934734f,{.5f,.36f,.5f}};
    return {grass,river,unit,unit,unit,unit,roof};
}

struct GameplayGodWorldEvidence3D {
    float riverCoverage{},buildingCoverage{},maximumBuildingPart{},roofCoverage{};
    std::uint32_t visibleWorldDescriptors{},visibleBuildingComponents{},buildingEdgeMask{},lodTransitions{};
    bool buildingValid{true};
};

struct GameplayGodCoarseEvidence3D {
    std::uint32_t visibleWorldDescriptors{},visibleBuildingComponents{},lodTransitions{};
    bool riverVisible{};
};

[[nodiscard]] inline GameplayGodCoarseEvidence3D EvaluateGameplayGodCoarseEvidence3D(
    const Aevrynd::Camera3D& camera,std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry) noexcept {
    GameplayGodCoarseEvidence3D result{};
    for(const auto& descriptor:descriptors){
        const Aevrynd::Transform3D transform{descriptor.position,{0,descriptor.yaw,0},descriptor.scale};
        const Aevrynd::MeshBounds3D* localBounds=nullptr;
        if(descriptor.kind==ShowcaseSceneKind3D::Grass)localBounds=&geometry.grass;
        else if(descriptor.kind==ShowcaseSceneKind3D::Water)localBounds=&geometry.river;
        else if(descriptor.kind==ShowcaseSceneKind3D::Building){if(descriptor.meshAsset.find("foundation")!=std::string::npos)localBounds=&geometry.foundation;else if(descriptor.meshAsset.find("front")!=std::string::npos)localBounds=&geometry.front;else if(descriptor.meshAsset.find("back")!=std::string::npos)localBounds=&geometry.back;else if(descriptor.meshAsset.find("roof")!=std::string::npos)localBounds=&geometry.roof;else localBounds=&geometry.side;}
        const auto bounds=localBounds?Aevrynd::TransformBounds(*localBounds,transform):Aevrynd::MeshBounds3D{descriptor.position,(std::max)(.1f,Aevrynd::Length(descriptor.scale)*.5f),descriptor.scale*.5f};
        if(!Aevrynd::SphereIntersectsFrustum(bounds.center,bounds.radius,camera,1280.f/720.f))continue;
        ++result.visibleWorldDescriptors;
        if(descriptor.kind==ShowcaseSceneKind3D::Water)result.riverVisible=true;
        else if(descriptor.kind==ShowcaseSceneKind3D::Building)++result.visibleBuildingComponents;
        else if(descriptor.kind==ShowcaseSceneKind3D::Grass){const auto selection=Aevrynd::EvaluateMeshLod3D(Aevrynd::Length(bounds.center-camera.position),bounds.radius,ShowcaseGrassLodPolicy3D());result.lodTransitions+=selection.submitNear&&selection.submitFar?1u:0u;}
    }
    return result;
}

[[nodiscard]] inline GameplayGodWorldEvidence3D EvaluateGameplayGodWorldEvidence3D(
    const Aevrynd::Camera3D& camera,std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry) noexcept {
    GameplayGodWorldEvidence3D result{};constexpr std::uint32_t maskWidth=80,maskHeight=45;
    std::vector<std::uint8_t> riverMask(std::size_t(maskWidth)*maskHeight),buildingMask(riverMask.size());
    std::uint64_t stableLow{};
    const auto merge=[&](const GameplayOccluderMask3D& source,std::vector<std::uint8_t>& target){for(std::size_t pixel=0;pixel<target.size();++pixel)target[pixel]|=source.pixels[pixel];};
    const auto edgeMask=[&](const GameplayOccluderMask3D& mask){std::uint32_t value{};for(std::uint32_t y=0;y<maskHeight;++y)for(std::uint32_t x=0;x<maskWidth;++x)if(mask.pixels[std::size_t(y)*maskWidth+x]){value|=x<1?1u:0u;value|=y<1?2u:0u;value|=x+1>=maskWidth?4u:0u;value|=y+1>=maskHeight?8u:0u;}return value;};
    for(const auto& descriptor:descriptors){
        ++stableLow;const Aevrynd::Transform3D transform{descriptor.position,{0,descriptor.yaw,0},descriptor.scale};
        const Aevrynd::MeshBounds3D* localBounds=nullptr;
        if(descriptor.kind==ShowcaseSceneKind3D::Grass)localBounds=&geometry.grass;
        else if(descriptor.kind==ShowcaseSceneKind3D::Water)localBounds=&geometry.river;
        else if(descriptor.kind==ShowcaseSceneKind3D::Building){if(descriptor.meshAsset.find("foundation")!=std::string::npos)localBounds=&geometry.foundation;else if(descriptor.meshAsset.find("front")!=std::string::npos)localBounds=&geometry.front;else if(descriptor.meshAsset.find("back")!=std::string::npos)localBounds=&geometry.back;else if(descriptor.meshAsset.find("roof")!=std::string::npos)localBounds=&geometry.roof;else localBounds=&geometry.side;}
        const auto bounds=localBounds?Aevrynd::TransformBounds(*localBounds,transform):Aevrynd::MeshBounds3D{descriptor.position,(std::max)(.1f,Aevrynd::Length(descriptor.scale)*.5f),descriptor.scale*.5f};
        if(Aevrynd::SphereIntersectsFrustum(bounds.center,bounds.radius,camera,1280.f/720.f))++result.visibleWorldDescriptors;
        if(descriptor.kind==ShowcaseSceneKind3D::Grass){if(!Aevrynd::SphereIntersectsFrustum(bounds.center,bounds.radius,camera,1280.f/720.f))continue;const auto selection=Aevrynd::EvaluateMeshLod3D(Aevrynd::Length(bounds.center-camera.position),bounds.radius,ShowcaseGrassLodPolicy3D());result.lodTransitions+=selection.submitNear&&selection.submitFar?1u:0u;continue;}
        if(descriptor.kind!=ShowcaseSceneKind3D::Water&&descriptor.kind!=ShowcaseSceneKind3D::Building)continue;
        GameplayOccluderMask3D mask{};auto rasterExtents=bounds.extents;
        // 水面是零厚度的正式三角形網格；以極小數值厚度避免盒狀光柵器丟失可見面積。
        if(descriptor.kind==ShowcaseSceneKind3D::Water)rasterExtents.y=(std::max)(rasterExtents.y,.001f);
        const GameplayOccluderBox3D box{stableLow,bounds.center,rasterExtents*2.f,0};if(!AnalyzeGameplayOccluderMask3D(camera,std::span<const GameplayOccluderBox3D>{&box,1},maskWidth,maskHeight,mask))continue;
        if(descriptor.kind==ShowcaseSceneKind3D::Water){merge(mask,riverMask);continue;}
        ++result.visibleBuildingComponents;result.buildingValid&=!mask.nearPlaneClipped;result.maximumBuildingPart=(std::max)(result.maximumBuildingPart,mask.coverage);if(descriptor.meshAsset.find("roof")!=std::string::npos)result.roofCoverage=mask.coverage;result.buildingEdgeMask|=edgeMask(mask);merge(mask,buildingMask);
    }
    result.riverCoverage=float(std::ranges::count(riverMask,std::uint8_t{255}))/float(riverMask.size());
    result.buildingCoverage=float(std::ranges::count(buildingMask,std::uint8_t{255}))/float(buildingMask.size());
    return result;
}

[[nodiscard]] inline float RasterGameplayPosedMeshCoverage3D(
    const Aevrynd::Camera3D& camera,std::span<const Aevrynd::Vertex3D> vertices,
    std::span<const std::uint32_t> indices,const Aevrynd::Matrix4& world,
    unsigned width=320,unsigned height=180) noexcept {
    if(!width||!height||vertices.empty()||indices.empty()||indices.size()%3)return 0.f;
    std::vector<Aevrynd::ProjectedPoint> projected(vertices.size());
    for(std::size_t index=0;index<vertices.size();++index){const auto& vertex=vertices[index];projected[index]=Aevrynd::ProjectToViewport(Aevrynd::TransformPoint(world,{vertex.x,vertex.y,vertex.z}),camera,{float(width),float(height)});}
    std::vector<std::uint8_t> mask(std::size_t(width)*height);
    const auto edge=[](Aevrynd::Vec2 a,Aevrynd::Vec2 b,Aevrynd::Vec2 p){return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);};
    for(std::size_t triangle=0;triangle<indices.size();triangle+=3){const auto ia=indices[triangle],ib=indices[triangle+1],ic=indices[triangle+2];if(ia>=projected.size()||ib>=projected.size()||ic>=projected.size())return 0.f;const auto& a=projected[ia];const auto& b=projected[ib];const auto& c=projected[ic];if(!a.visible||!b.visible||!c.visible)continue;const float minimumX=(std::max)(0.f,std::floor((std::min)({a.position.x,b.position.x,c.position.x}))),minimumY=(std::max)(0.f,std::floor((std::min)({a.position.y,b.position.y,c.position.y}))),maximumX=(std::min)(float(width-1),std::ceil((std::max)({a.position.x,b.position.x,c.position.x}))),maximumY=(std::min)(float(height-1),std::ceil((std::max)({a.position.y,b.position.y,c.position.y})));if(minimumX>maximumX||minimumY>maximumY)continue;for(unsigned y=unsigned(minimumY);y<=unsigned(maximumY);++y)for(unsigned x=unsigned(minimumX);x<=unsigned(maximumX);++x){const Aevrynd::Vec2 point{float(x)+.5f,float(y)+.5f};const float ab=edge(a.position,b.position,point),bc=edge(b.position,c.position,point),ca=edge(c.position,a.position,point);if((ab>=0&&bc>=0&&ca>=0)||(ab<=0&&bc<=0&&ca<=0))mask[std::size_t(y)*width+x]=1;}}
    return float(std::ranges::count(mask,std::uint8_t{1}))/float(mask.size());
}

struct GameplayFirstPersonArmsEvidence3D {
    std::uint32_t submeshCount{},retainedTriangles{},leftArmTriangles{},rightArmTriangles{};
    [[nodiscard]] constexpr bool Complete()const noexcept{return submeshCount&&retainedTriangles&&leftArmTriangles&&rightArmTriangles&&retainedTriangles==leftArmTriangles+rightArmTriangles;}
};
[[nodiscard]] inline GameplayFirstPersonArmsEvidence3D BuildGameplayFirstPersonArmsEvidence3D(const Aevrynd::FirstPersonArmsMeshResult3D& result) noexcept {
    return {std::uint32_t(result.submeshes.size()),result.stats.retainedTriangles,result.stats.leftArmTriangles,result.stats.rightArmTriangles};
}

struct GameplayCharacterMaterialEvidence3D {
    std::uint32_t femaleCharacterMaterialCount{},maleCharacterMaterialCount{},characterMaterialCount{};
    [[nodiscard]] constexpr bool Complete()const noexcept {
        return femaleCharacterMaterialCount&&maleCharacterMaterialCount&&
            characterMaterialCount==femaleCharacterMaterialCount+maleCharacterMaterialCount;
    }
};

inline constexpr float GameplayGodMinimumPlayerCoverage3D=.0004f;
inline constexpr std::uint32_t GameplayCaptureViewportWidth3D=1280;
inline constexpr std::uint32_t GameplayCaptureViewportHeight3D=720;
inline constexpr float GameplayGodMinimumPlayerPixelCountExact3D=
    GameplayGodMinimumPlayerCoverage3D*float(GameplayCaptureViewportWidth3D*GameplayCaptureViewportHeight3D);
inline constexpr std::uint32_t GameplayGodMinimumPlayerPixelsFloor3D=
    std::uint32_t(GameplayGodMinimumPlayerPixelCountExact3D);
inline constexpr std::uint32_t GameplayGodMinimumPlayerPixels3D=GameplayGodMinimumPlayerPixelsFloor3D+
    (float(GameplayGodMinimumPlayerPixelsFloor3D)<GameplayGodMinimumPlayerPixelCountExact3D?1u:0u);
// 320x180 的投影聯集不是深度/材質裁切後 OID；已觀察 384 等效像素只剩 326。
// 候選預留 25%（462 像素，粗 raster 實際至少 464），正式 .0004/369 gate 不變。
inline constexpr std::uint32_t GameplayGodCandidatePlayerPixels3D=
    (GameplayGodMinimumPlayerPixels3D*5u+3u)/4u;
inline constexpr float GameplayGodCandidatePlayerCoverage3D=
    float(GameplayGodCandidatePlayerPixels3D)/float(GameplayCaptureViewportWidth3D*GameplayCaptureViewportHeight3D);
[[nodiscard]] inline bool GameplayGodCandidatePlayerCoveragePass3D(float coverage) noexcept {
    return std::isfinite(coverage)&&coverage>=GameplayGodCandidatePlayerCoverage3D&&coverage<=1.f;
}

inline constexpr float GameplayGodMinimumCornerRayOriginHeight3D=1.5f;
// 小屋的磚基座與煙囪保留足夠法線幅度；木牆另由 WoodSiding006 的載入值控制。
inline constexpr float GameplayCottageWallNormalScale3D=.9f;
inline constexpr bool GameplayCottageMaterialUsesUv3D(bool roof) noexcept { return roof; }
// 正交 God 鏡頭的下方角落若從接近地表的位置開始，局部起伏會落在
// near plane 後方，即使 y=0 平面射線可命中仍會露出 clear color。
// 保留 1.5m 的可見地表起伏裕量；正式截圖仍須由 depth/OID gate 證明全幅覆蓋。
[[nodiscard]] inline bool GameplayGodTerrainFrameCovered3D(
    const Aevrynd::Camera3D& camera) noexcept {
    if(camera.projection.mode!=Aevrynd::ProjectionMode::Orthographic)return false;
    constexpr Aevrynd::Vec2 viewport{float(GameplayCaptureViewportWidth3D),
                                     float(GameplayCaptureViewportHeight3D)};
    constexpr std::array<Aevrynd::Vec2,4> corners{{{.5f,.5f},{viewport.x-.5f,.5f},
                                                   {.5f,viewport.y-.5f},{viewport.x-.5f,viewport.y-.5f}}};
    for(const auto point:corners){
        const auto ray=Aevrynd::ScreenPointToRay(camera,viewport,point);
        if(!std::isfinite(ray.origin.y)||!std::isfinite(ray.direction.y)||ray.direction.y>=-.001f)return false;
        if(ray.origin.y<GameplayGodMinimumCornerRayOriginHeight3D+camera.projection.nearPlane)return false;
        const float t=-ray.origin.y/ray.direction.y;
        if(!std::isfinite(t)||t<0.f||t>camera.projection.farPlane)return false;
        const auto hit=ray.origin+ray.direction*t;
        if(std::abs(hit.x)>1000.f||std::abs(hit.z)>1000.f)return false;
    }
    return true;
}

struct GameplayCaptureProjectionEvidence3D {
    Aevrynd::ProjectionMode projectionMode{Aevrynd::ProjectionMode::Perspective};
    float verticalFovRadians{},orthographicHeight{};
    float godActualTriangleCoverage{},godBuildingUnion{},godMaximumBuildingPart{},godRoofCoverage{};
    std::uint32_t godVisibleWorldDescriptors{},godLodTransitions{};
    [[nodiscard]] bool Complete(GameplayCaptureCamera3D camera)const noexcept {
        const bool god=camera==GameplayCaptureCamera3D::God;
        if(god){if(projectionMode!=Aevrynd::ProjectionMode::Orthographic||!std::isfinite(orthographicHeight)||orthographicHeight<4.f||orthographicHeight>180.f)return false;}
        else if(projectionMode!=Aevrynd::ProjectionMode::Perspective||!std::isfinite(verticalFovRadians)||verticalFovRadians<=.1f||verticalFovRadians>=3.f||orthographicHeight!=0.f)return false;
        if(god)return godActualTriangleCoverage>=GameplayGodMinimumPlayerCoverage3D&&
            godVisibleWorldDescriptors>=20&&godBuildingUnion>=0.f&&godBuildingUnion<=.22f&&
            godMaximumBuildingPart>=0.f&&godMaximumBuildingPart<=.08f&&godRoofCoverage>=0.f&&
            godRoofCoverage<=.05f&&godLodTransitions>0;
        return godActualTriangleCoverage==0.f&&godVisibleWorldDescriptors==0&&godBuildingUnion==0.f&&
            godMaximumBuildingPart==0.f&&godRoofCoverage==0.f&&godLodTransitions==0;
    }
};
[[nodiscard]] inline GameplayCharacterMaterialEvidence3D BuildGameplayCharacterMaterialEvidence3D(
    std::size_t femaleCount,std::size_t maleCount) noexcept {
    if(!femaleCount||!maleCount||femaleCount>(std::numeric_limits<std::uint32_t>::max)()||
       maleCount>(std::numeric_limits<std::uint32_t>::max)()-femaleCount)return {};
    return {std::uint32_t(femaleCount),std::uint32_t(maleCount),std::uint32_t(femaleCount+maleCount)};
}

// 單一候選的廉價正式重驗；快取只能重用再次通過完整構圖/LOD/world gate 的結果。
[[nodiscard]] inline GameplayGodCameraLayout3D ValidateGameplayGodCameraLayout3D(
    Aevrynd::Vec3 focus,const Aevrynd::Camera3D& camera,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry,float distance,float azimuth,
    float elevation,std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,const Aevrynd::Matrix4& playerWorld) noexcept {
    (void)focus;GameplayGodCameraLayout3D result{};result.camera=camera;result.distance=distance;result.azimuth=azimuth;result.elevation=elevation;
    if(posedVertices.empty())return result;Aevrynd::Vec3 posedMinimum{posedVertices.front().x,posedVertices.front().y,posedVertices.front().z},posedMaximum=posedMinimum;for(const auto& vertex:posedVertices){if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z))return result;posedMinimum.x=(std::min)(posedMinimum.x,vertex.x);posedMinimum.y=(std::min)(posedMinimum.y,vertex.y);posedMinimum.z=(std::min)(posedMinimum.z,vertex.z);posedMaximum.x=(std::max)(posedMaximum.x,vertex.x);posedMaximum.y=(std::max)(posedMaximum.y,vertex.y);posedMaximum.z=(std::max)(posedMaximum.z,vertex.z);}const Aevrynd::Vec3 posedCenter=(posedMinimum+posedMaximum)*.5f,posedExtents=(posedMaximum-posedMinimum)*.5f;
    float minimumX=1280,minimumY=720,maximumX=0,maximumY=0;bool visible=true;
    for(unsigned corner=0;corner<8;++corner){const Aevrynd::Vec3 local=posedCenter+Aevrynd::Vec3{corner&1?posedExtents.x:-posedExtents.x,corner&2?posedExtents.y:-posedExtents.y,corner&4?posedExtents.z:-posedExtents.z};const auto point=Aevrynd::ProjectToViewport(Aevrynd::TransformPoint(playerWorld,local),camera,{1280,720});if(!point.visible){visible=false;continue;}minimumX=(std::min)(minimumX,point.position.x);maximumX=(std::max)(maximumX,point.position.x);minimumY=(std::min)(minimumY,point.position.y);maximumY=(std::max)(maximumY,point.position.y);}
    if(!visible)return result;
    result.playerCoverage=RasterGameplayPosedMeshCoverage3D(camera,posedVertices,posedIndices,playerWorld);
    const auto world=EvaluateGameplayGodWorldEvidence3D(camera,descriptors,geometry);result.visibleWorldDescriptors=world.visibleWorldDescriptors;result.lodTransitions=world.lodTransitions;result.buildingUnion=world.buildingCoverage;result.maximumBuildingPart=world.maximumBuildingPart;result.roofCoverage=world.roofCoverage;result.riverCoverage=world.riverCoverage;result.visibleBuildingComponents=world.visibleBuildingComponents;result.buildingEdgeMask=world.buildingEdgeMask;result.roofTouchesTop=(world.buildingEdgeMask&2u)!=0;result.productionLodExact=true;
    result.valid=GameplayGodCandidatePlayerCoveragePass3D(result.playerCoverage)&&GameplayGodTerrainFrameCovered3D(camera)&&result.lodTransitions&&world.buildingValid&&result.riverCoverage>=.002f&&result.buildingUnion>=.002f&&result.buildingUnion<=.22f&&result.visibleBuildingComponents==6&&!result.buildingEdgeMask&&result.maximumBuildingPart<=.08f&&result.roofCoverage<=.05f&&result.visibleWorldDescriptors>=20;
    return result;
}

// 將正式選中候選依固定 collision→terrain 次序封閉；二次套用必須完全冪等，避免首幀與cache走不同契約。
template<class CollisionConstraint,class TerrainConstraint>
[[nodiscard]] inline GameplayGodCameraLayout3D ResolveGameplayGodCameraLayout3D(
    Aevrynd::Vec3 focus,const GameplayGodCameraLayout3D& selected,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry,
    std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,const Aevrynd::Matrix4& playerWorld,
    CollisionConstraint&& collisionConstraint,TerrainConstraint&& terrainConstraint) noexcept {
    if(!selected.valid)return{};
    const auto constrain=[&](Aevrynd::Camera3D camera){return terrainConstraint(collisionConstraint(camera));};
    const Aevrynd::Camera3D resolved=constrain(selected.camera),revalidated=constrain(resolved);
    const auto exact=[](const Aevrynd::Camera3D& left,const Aevrynd::Camera3D& right){return left.position.x==right.position.x&&left.position.y==right.position.y&&left.position.z==right.position.z&&left.target.x==right.target.x&&left.target.y==right.target.y&&left.target.z==right.target.z&&left.up.x==right.up.x&&left.up.y==right.up.y&&left.up.z==right.up.z&&left.projection.mode==right.projection.mode&&left.projection.verticalFieldOfView==right.projection.verticalFieldOfView&&left.projection.orthographicHeight==right.projection.orthographicHeight&&left.projection.nearPlane==right.projection.nearPlane&&left.projection.farPlane==right.projection.farPlane;};
    if(!exact(resolved,revalidated))return{};
    auto result=ValidateGameplayGodCameraLayout3D(focus,resolved,descriptors,geometry,selected.distance,selected.azimuth,selected.elevation,posedVertices,posedIndices,playerWorld);
    result.maximumActualTriangleCoverage=selected.maximumActualTriangleCoverage;result.evaluatedCandidates=selected.evaluatedCandidates;result.coarseCandidates=selected.coarseCandidates;result.rasterCandidates=selected.rasterCandidates;result.rejectedActualCoverage=selected.rejectedActualCoverage;result.rejectedLod=selected.rejectedLod;result.rejectedWorld=selected.rejectedWorld;result.rejectedBuilding=selected.rejectedBuilding;
    return result;
}

[[nodiscard]] inline GameplayGodCameraLayout3D ValidateGameplayGodCameraLayout3D(
    Aevrynd::Vec3 focus,const Aevrynd::Camera3D& camera,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const Aevrynd::MeshBounds3D& grassBounds,float distance,float azimuth,
    float elevation,std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,const Aevrynd::Matrix4& playerWorld) noexcept {
    return ValidateGameplayGodCameraLayout3D(focus,camera,descriptors,
        LegacyGameplayGodProductionGeometry3D(grassBounds),distance,azimuth,elevation,
        posedVertices,posedIndices,playerWorld);
}

// 以正式草地LOD bounds與世界描述掃描相機距離；正交畫面尺度獨立，避免為放大角色破壞LOD展示。
template<class CollisionConstraint,class TerrainConstraint>
[[nodiscard]] inline GameplayGodCameraLayout3D SelectGameplayGodCameraLayoutConstrained3D(
    Aevrynd::Vec3 focus,std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry,std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,const Aevrynd::Matrix4& playerWorld,
    CollisionConstraint&& collisionConstraint,TerrainConstraint&& terrainConstraint) noexcept {
    GameplayGodCameraLayout3D best{},fallback{};float fallbackPenalty=(std::numeric_limits<float>::infinity)();constexpr std::size_t fovBuckets=16,distanceBuckets=3,preliminaryPerDirection=fovBuckets*distanceBuckets,candidatesPerDirection=8,directionCount=48;std::vector<GameplayGodCameraLayout3D> preliminaryCandidates(preliminaryPerDirection*directionCount),coarseCandidates(candidatesPerDirection*directionCount);std::array<std::uint8_t,directionCount> coarseCounts{};std::size_t coarseCandidateCount{};std::uint32_t evaluatedCandidates{},rejectedLod{},rejectedWorld{},rejectedBuilding{};
    if(posedVertices.empty())return best;Aevrynd::Vec3 posedMinimum{posedVertices.front().x,posedVertices.front().y,posedVertices.front().z},posedMaximum=posedMinimum;for(const auto& vertex:posedVertices){if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z))return best;posedMinimum.x=(std::min)(posedMinimum.x,vertex.x);posedMinimum.y=(std::min)(posedMinimum.y,vertex.y);posedMinimum.z=(std::min)(posedMinimum.z,vertex.z);posedMaximum.x=(std::max)(posedMaximum.x,vertex.x);posedMaximum.y=(std::max)(posedMaximum.y,vertex.y);posedMaximum.z=(std::max)(posedMaximum.z,vertex.z);}const Aevrynd::Vec3 posedCenter=(posedMinimum+posedMaximum)*.5f,posedExtents=(posedMaximum-posedMinimum)*.5f;
    constexpr std::array<float,6> elevations{.52359878f,.61086524f,.78539816f,.95993109f,1.13446401f,1.30899694f};
    for(unsigned elevationIndex=0;elevationIndex<elevations.size();++elevationIndex)for(unsigned azimuthIndex=0;azimuthIndex<8;++azimuthIndex)for(unsigned heightStep=0;heightStep<=60;++heightStep)for(unsigned step=0;step<=48;++step){
        const float distance=18.f+float(step)*.25f;
        const float elevation=elevations[elevationIndex],azimuth=float(azimuthIndex)*.78539816f;
        const Aevrynd::Vec3 direction{std::cos(azimuth)*std::cos(elevation),std::sin(elevation),std::sin(azimuth)*std::cos(elevation)};
        Aevrynd::CameraProjection projection{};projection.mode=Aevrynd::ProjectionMode::Orthographic;projection.orthographicHeight=6.f+float(heightStep)*.5f;
        const Aevrynd::Camera3D camera{focus+direction*distance,focus,{0,1,0},projection};
        float minimumX=1280,minimumY=720,maximumX=0,maximumY=0;
        bool visible=true;
        for(unsigned corner=0;corner<8;++corner){
            const Aevrynd::Vec3 local=posedCenter+Aevrynd::Vec3{corner&1?posedExtents.x:-posedExtents.x,corner&2?posedExtents.y:-posedExtents.y,corner&4?posedExtents.z:-posedExtents.z};const auto point=Aevrynd::ProjectToViewport(Aevrynd::TransformPoint(playerWorld,local),camera,{1280,720});
            if(!point.visible){visible=false;continue;}
            minimumX=(std::min)(minimumX,point.position.x);maximumX=(std::max)(maximumX,point.position.x);
            minimumY=(std::min)(minimumY,point.position.y);maximumY=(std::max)(maximumY,point.position.y);
        }
        if(!visible)continue;
        const float coverage=(std::max)(0.f,maximumX-minimumX)*(std::max)(0.f,maximumY-minimumY)/(1280.f*720.f);
        ++evaluatedCandidates;if(coverage<.0002f)continue;
        GameplayGodCameraLayout3D preliminary{};preliminary.camera=camera;preliminary.distance=distance;preliminary.azimuth=azimuth;preliminary.elevation=elevation;preliminary.playerCoverage=coverage;preliminary.valid=true;
        const std::size_t directionIndex=std::size_t(elevationIndex)*8+azimuthIndex,fovBucket=(std::min)(std::size_t(heightStep/4),fovBuckets-1),distanceBucket=(std::min)(std::size_t(step/17),distanceBuckets-1),slot=directionIndex*preliminaryPerDirection+fovBucket*distanceBuckets+distanceBucket;if(!preliminaryCandidates[slot].valid||preliminaryCandidates[slot].playerCoverage<coverage)preliminaryCandidates[slot]=preliminary;
    }
    for(std::size_t preliminaryIndex=0;preliminaryIndex<preliminaryCandidates.size();++preliminaryIndex){const auto& preliminary=preliminaryCandidates[preliminaryIndex];if(!preliminary.valid)continue;const auto cheap=EvaluateGameplayGodCoarseEvidence3D(preliminary.camera,descriptors,geometry);if(!cheap.lodTransitions){++rejectedLod;continue;}if(cheap.visibleWorldDescriptors<20){++rejectedWorld;continue;}if(!cheap.riverVisible||cheap.visibleBuildingComponents<6){++rejectedBuilding;continue;}const auto world=EvaluateGameplayGodWorldEvidence3D(preliminary.camera,descriptors,geometry);const bool hardWorld=GameplayGodTerrainFrameCovered3D(preliminary.camera)&&world.buildingValid&&world.riverCoverage>=.002f&&world.buildingCoverage>=.002f&&world.buildingCoverage<=.22f&&world.visibleBuildingComponents==6&&!world.buildingEdgeMask&&world.maximumBuildingPart<=.08f&&world.roofCoverage<=.05f&&world.visibleWorldDescriptors>=20&&world.lodTransitions;if(!hardWorld){++rejectedBuilding;continue;}auto coarse=preliminary;coarse.buildingUnion=world.buildingCoverage;coarse.maximumBuildingPart=world.maximumBuildingPart;coarse.roofCoverage=world.roofCoverage;coarse.lodTransitions=world.lodTransitions;coarse.visibleWorldDescriptors=world.visibleWorldDescriptors;coarse.roofTouchesTop=(world.buildingEdgeMask&2u)!=0;coarse.riverCoverage=world.riverCoverage;coarse.visibleBuildingComponents=world.visibleBuildingComponents;coarse.buildingEdgeMask=world.buildingEdgeMask;coarse.productionLodExact=true;const float penalty=(std::max)(0.f,GameplayGodMinimumPlayerCoverage3D-coarse.playerCoverage)*1000.f;if(penalty<fallbackPenalty){fallbackPenalty=penalty;fallback=coarse;}const std::size_t directionIndex=preliminaryIndex/preliminaryPerDirection,begin=directionIndex*candidatesPerDirection;auto& count=coarseCounts[directionIndex];if(count<candidatesPerDirection){coarseCandidates[begin+count++]=coarse;++coarseCandidateCount;}else{auto first=coarseCandidates.begin()+std::ptrdiff_t(begin),last=first+std::ptrdiff_t(candidatesPerDirection);auto minimum=std::ranges::min_element(std::ranges::subrange(first,last),{},&GameplayGodCameraLayout3D::playerCoverage);if(minimum!=last&&minimum->playerCoverage<coarse.playerCoverage)*minimum=coarse;}}
    std::array<GameplayGodCameraLayout3D,directionCount> refinementBases{};const auto coarseFallback=fallback;best={};fallback={};std::uint32_t rejectedActualCoverage{},rasterCandidates{};float maximumActualCoverage{};for(const auto& stored:coarseCandidates){if(!stored.valid)continue;++rasterCandidates;auto candidate=ResolveGameplayGodCameraLayout3D(focus,stored,descriptors,geometry,posedVertices,posedIndices,playerWorld,collisionConstraint,terrainConstraint);const auto directionIndex=std::size_t(&stored-coarseCandidates.data())/candidatesPerDirection;if(candidate.playerCoverage>refinementBases[directionIndex].playerCoverage)refinementBases[directionIndex]=candidate;maximumActualCoverage=(std::max)(maximumActualCoverage,candidate.playerCoverage);if(candidate.playerCoverage>fallback.playerCoverage)fallback=candidate;if(!candidate.valid){++rejectedActualCoverage;continue;}constexpr float preferredAzimuth=5.49778714f;const auto angularDistance=[](float left,float right){return std::abs(std::atan2(std::sin(left-right),std::cos(left-right)));};const float preference=angularDistance(candidate.azimuth,preferredAzimuth),bestPreference=angularDistance(best.azimuth,preferredAzimuth);if(!best.valid||candidate.lodTransitions>best.lodTransitions||(candidate.lodTransitions==best.lodTransitions&&candidate.playerCoverage>best.playerCoverage+.00001f)||(candidate.lodTransitions==best.lodTransitions&&std::abs(candidate.playerCoverage-best.playerCoverage)<=.00001f&&preference<bestPreference))best=candidate;}
    // 正交投影不可用距離調整角色尺寸；微縮高度時同步搜尋小幅平移，並重跑全部world hard gates避免裁切建築。
    // 裕量不能靠放寬 world gate；保留八個不同視向的最佳 raster 候選作有界局部重驗。
    if(!best.valid&&fallback.playerCoverage>0.f){std::ranges::sort(refinementBases,[](const auto& a,const auto& b){return a.playerCoverage>b.playerCoverage;});for(std::size_t baseIndex=0;baseIndex<8&&!best.valid;++baseIndex){const auto refinementBase=refinementBases[baseIndex];if(refinementBase.playerCoverage<=0.f)continue;for(unsigned refinement=1;refinement<=24&&!best.valid;++refinement)for(unsigned panStep=0;panStep<=8&&!best.valid;++panStep)for(unsigned axis=0;axis<(panStep?2u:1u)&&!best.valid;++axis)for(const float sign:{-1.f,1.f}){if(!panStep&&sign>0)continue;auto refined=refinementBase;refined.valid=true;refined.camera.projection.orthographicHeight=refinementBase.camera.projection.orthographicHeight-float(refinement)*.125f;if(refined.camera.projection.orthographicHeight<4.f)continue;const float pan=sign*float(panStep)*.25f;const Aevrynd::Vec3 offset=axis?Aevrynd::Vec3{0,0,pan}:Aevrynd::Vec3{pan,0,0};refined.camera.position=refined.camera.position+offset;refined.camera.target=refined.camera.target+offset;auto candidate=ResolveGameplayGodCameraLayout3D(focus,refined,descriptors,geometry,posedVertices,posedIndices,playerWorld,collisionConstraint,terrainConstraint);++rasterCandidates;maximumActualCoverage=(std::max)(maximumActualCoverage,candidate.playerCoverage);if(candidate.playerCoverage>fallback.playerCoverage)fallback=candidate;if(!candidate.valid){++rejectedActualCoverage;continue;}best=candidate;break;}}}
    const auto stampDiagnostics=[&](GameplayGodCameraLayout3D& value){value.maximumActualTriangleCoverage=maximumActualCoverage;value.evaluatedCandidates=evaluatedCandidates;value.coarseCandidates=std::uint32_t(coarseCandidateCount);value.rasterCandidates=rasterCandidates;value.rejectedActualCoverage=rejectedActualCoverage;value.rejectedLod=rejectedLod;value.rejectedWorld=rejectedWorld;value.rejectedBuilding=rejectedBuilding;};
    if(best.valid){stampDiagnostics(best);return best;}
    if(!rasterCandidates)fallback=coarseFallback;fallback.valid=false;stampDiagnostics(fallback);
    return fallback;
}

struct GameplayAuthoritativeGodCameraInput3D {
    Aevrynd::Vec3 controllerPosition{};
    float yaw{},pitch{};
};

struct GameplayAuthoritativeGodCameraReceipt3D {
    GameplayGodCameraLayout3D layout{};
    std::uint64_t inputHash{},cameraHash{},subjectPoseHash{};
    bool valid{};
};

[[nodiscard]] inline std::uint64_t HashGameplayAuthoritativeGodCameraInput3D(const GameplayAuthoritativeGodCameraInput3D& input)noexcept{
    if(!std::isfinite(input.controllerPosition.x)||!std::isfinite(input.controllerPosition.y)||!std::isfinite(input.controllerPosition.z)||!std::isfinite(input.yaw)||!std::isfinite(input.pitch))return 0;
    std::uint64_t hash=14695981039346656037ull;const auto mix=[&](std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};
    for(float value:{input.controllerPosition.x,input.controllerPosition.y,input.controllerPosition.z,input.yaw,input.pitch})mix(std::bit_cast<std::uint32_t>(value==0.f?0.f:value));return hash;
}

// God 視角的權威輸入只能來自固定模擬狀態與穩定排序的碰撞查詢。
// renderer frame、wall-clock dt、presentation cache 與 process-local handle 都不在輸入契約內。
template<class CollisionConstraint,class TerrainConstraint>
[[nodiscard]] inline GameplayAuthoritativeGodCameraReceipt3D BuildGameplayAuthoritativeGodCamera3D(
    const GameplayAuthoritativeGodCameraInput3D& input,
    std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry,
    std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,
    CollisionConstraint&& collisionConstraint,TerrainConstraint&& terrainConstraint,
    const Aevrynd::Matrix4* presentationWorld=nullptr) noexcept {
    GameplayAuthoritativeGodCameraReceipt3D result{};
    if(!std::isfinite(input.controllerPosition.x)||!std::isfinite(input.controllerPosition.y)||
       !std::isfinite(input.controllerPosition.z)||!std::isfinite(input.yaw)||!std::isfinite(input.pitch))return result;
    const Aevrynd::Vec3 focus=input.controllerPosition+Aevrynd::Vec3{0,1.1f,0};
    const Aevrynd::Matrix4 legacyWorld=Aevrynd::CreateTransformMatrix(input.controllerPosition,{0,input.yaw,0},{1,1,1});
    const Aevrynd::Matrix4& playerWorld=presentationWorld?*presentationWorld:legacyWorld;
    result.layout=SelectGameplayGodCameraLayoutConstrained3D(focus,descriptors,geometry,posedVertices,posedIndices,playerWorld,
        std::forward<CollisionConstraint>(collisionConstraint),std::forward<TerrainConstraint>(terrainConstraint));
    const auto mix=[](std::uint64_t& hash,std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};
    const auto scalar=[&](std::uint64_t& hash,float value){mix(hash,std::bit_cast<std::uint32_t>(value==0.f?0.f:value));};
    result.inputHash=HashGameplayAuthoritativeGodCameraInput3D(input);
    result.cameraHash=14695981039346656037ull;
    const auto& camera=result.layout.camera;
    for(const auto value:{camera.position,camera.target,camera.up})for(float component:{value.x,value.y,value.z})scalar(result.cameraHash,component);
    mix(result.cameraHash,unsigned(camera.projection.mode));
    for(float value:{camera.projection.verticalFieldOfView,camera.projection.orthographicHeight,camera.projection.nearPlane,camera.projection.farPlane})scalar(result.cameraHash,value);
    result.valid=result.layout.valid&&result.inputHash&&result.cameraHash;
    return result;
}

[[nodiscard]] inline GameplayGodCameraLayout3D SelectGameplayGodCameraLayout3D(
    Aevrynd::Vec3 focus,std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const GameplayGodProductionGeometry3D& geometry,std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,const Aevrynd::Matrix4& playerWorld) noexcept {
    const auto identity=[](Aevrynd::Camera3D camera){return camera;};
    return SelectGameplayGodCameraLayoutConstrained3D(focus,descriptors,geometry,posedVertices,posedIndices,playerWorld,identity,identity);
}

[[nodiscard]] inline GameplayGodCameraLayout3D SelectGameplayGodCameraLayout3D(
    Aevrynd::Vec3 focus,std::span<const ShowcaseSceneDescriptor3D> descriptors,
    const Aevrynd::MeshBounds3D& grassBounds,std::span<const Aevrynd::Vertex3D> posedVertices,
    std::span<const std::uint32_t> posedIndices,const Aevrynd::Matrix4& playerWorld) noexcept {
    return SelectGameplayGodCameraLayout3D(focus,descriptors,
        LegacyGameplayGodProductionGeometry3D(grassBounds),posedVertices,posedIndices,playerWorld);
}

// Controlled replay只選一個無碰撞的螢幕點；實際預覽與commit仍完整通過production pointer adapter。
[[nodiscard]] inline std::optional<Aevrynd::Vec2> FindGameplayClearPlacementPointer3D(
    const Aevrynd::Camera3D& camera,const Aevrynd::CollisionWorld3D& collision) noexcept {
    for(float y=180;y<=460;y+=40)for(float x=420;x<=1100;x+=40){
        const auto ray=Aevrynd::ScreenPointToRay(camera,{1280,720},{x,y});
        if(std::abs(ray.direction.y)<.0001f)continue;
        const float t=-ray.origin.y/ray.direction.y;if(t<0||t>200)continue;
        auto point=ray.origin+ray.direction*t;point.x=std::round(point.x*.5f)*2;point.z=std::round(point.z*.5f)*2;point.y=.5f;
        if(!collision.OverlapsBox(point,{.48f,.48f,.48f}))return Aevrynd::Vec2{x,y};
    }
    return std::nullopt;
}

enum class GameplayInteractionCameraSource3D:std::uint8_t{FinalRendered,FrozenAuthoritativeGod,CaptureFirstPerson};
[[nodiscard]] inline std::uint64_t HashGameplayInteractionCamera3D(const Aevrynd::Camera3D& camera)noexcept{std::uint64_t hash=14695981039346656037ull;const auto mix=[&](std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};for(const auto value:{camera.position,camera.target,camera.up})for(float component:{value.x,value.y,value.z})mix(std::bit_cast<std::uint32_t>(component==0.f?0.f:component));mix(unsigned(camera.projection.mode));for(float value:{camera.projection.verticalFieldOfView,camera.projection.orthographicHeight,camera.projection.nearPlane,camera.projection.farPlane})mix(std::bit_cast<std::uint32_t>(value==0.f?0.f:value));return hash;}
[[nodiscard]] inline std::uint64_t HashGameplayInteractionRay3D(const Aevrynd::Ray3D& ray)noexcept{std::uint64_t hash=14695981039346656037ull;for(float value:{ray.origin.x,ray.origin.y,ray.origin.z,ray.direction.x,ray.direction.y,ray.direction.z}){for(unsigned byte=0;byte<4;++byte){hash^=std::uint8_t(std::bit_cast<std::uint32_t>(value==0.f?0.f:value)>>(byte*8));hash*=1099511628211ull;}}return hash;}
struct GameplayInteractionCameraSelection3D {Aevrynd::Camera3D camera{};GameplayInteractionCameraSource3D source{GameplayInteractionCameraSource3D::FinalRendered};std::uint64_t finalRenderedCameraHash{},interactionCameraHash{};};
// 選擇發生於final camera之後；24-26 replay優先於258-268 fixture，live永遠使用final rendered camera。
[[nodiscard]] inline GameplayInteractionCameraSelection3D SelectGameplayInteractionCamera3D(const Aevrynd::Camera3D& finalRendered,const Aevrynd::Camera3D* frozenAuthoritativeGod,const Aevrynd::Camera3D& captureFirstPerson,bool capture,std::uint64_t tick)noexcept{
    GameplayInteractionCameraSelection3D result{};result.finalRenderedCameraHash=HashGameplayInteractionCamera3D(finalRendered);
    if(capture&&tick>=24&&tick<=26&&frozenAuthoritativeGod){result.camera=*frozenAuthoritativeGod;result.source=GameplayInteractionCameraSource3D::FrozenAuthoritativeGod;}
    else if(capture&&tick>=258&&tick<=268){result.camera=captureFirstPerson;result.source=GameplayInteractionCameraSource3D::CaptureFirstPerson;}
    else result.camera=finalRendered;
    result.interactionCameraHash=HashGameplayInteractionCamera3D(result.camera);return result;
}

struct GameplayActorScreenSeparation3D {
    std::uint32_t playerPixels{},companionPixels{},gapPixels{};
    bool complete{},safe{};
};

[[nodiscard]] inline GameplayActorScreenSeparation3D EvaluateGameplayActorScreenSeparation3D(
    unsigned width,unsigned height,std::span<const std::uint32_t> pixels,
    std::uint32_t playerToken,std::uint32_t companionToken) noexcept {
    GameplayActorScreenSeparation3D result{};
    if(!width||!height||pixels.size()!=std::size_t(width)*height||!playerToken||!companionToken||playerToken==companionToken)return result;
    struct Bounds{unsigned minimumX{},maximumX{};bool valid{};} player{width,0,false},companion{width,0,false};
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto token=pixels[std::size_t(y)*width+x];Bounds* bounds=nullptr;if(token==playerToken){bounds=&player;++result.playerPixels;}else if(token==companionToken){bounds=&companion;++result.companionPixels;}if(bounds){bounds->minimumX=std::min(bounds->minimumX,x);bounds->maximumX=std::max(bounds->maximumX,x);bounds->valid=true;}}
    result.complete=player.valid&&companion.valid;
    if(!result.complete)return result;
    if(player.maximumX<companion.minimumX)result.gapPixels=companion.minimumX-player.maximumX-1;
    else if(companion.maximumX<player.minimumX)result.gapPixels=player.minimumX-companion.maximumX-1;
    result.safe=result.gapPixels>=std::uint32_t(std::ceil(float(width)*.05f));
    return result;
}

struct GameplayCaptureExpectation3D {
    std::wstring_view name{};
    GameplayCaptureCamera3D camera{};
    GameplayCaptureLighting3D lighting{};
    std::array<Aevrynd::RenderObjectIdentity3D,2> requiredIdentities{};
    std::size_t requiredIdentityCount{};
    std::array<Aevrynd::RenderObjectIdentity3D,2> forbiddenIdentities{};
    std::size_t forbiddenIdentityCount{};
    GameplayCaptureLodExpectation3D lodExpectation{GameplayCaptureLodExpectation3D::Stable};
};

inline constexpr std::uint64_t GameplayCaptureEnvironmentIdentityHigh3D=GameplayEnvironmentIdentityDomain3D;
inline constexpr std::uint64_t GameplayCaptureCharacterIdentityHigh3D=0xaec0474348415201ull;
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCapturePlayerIdentity3D{{GameplayCaptureCharacterIdentityHigh3D,1},1};
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCaptureFirstPersonArmsIdentity3D{{GameplayCaptureCharacterIdentityHigh3D,3},1};
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCaptureWeaponIdentity3D{{GameplayCaptureCharacterIdentityHigh3D,4},1};
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCaptureTerrainIdentity3D{{GameplayCaptureEnvironmentIdentityHigh3D,1},1};
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCaptureForbiddenPlayerGeneration3D{{GameplayCaptureCharacterIdentityHigh3D,1},2};
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCaptureForbiddenFirstPersonArmsGeneration3D{{GameplayCaptureCharacterIdentityHigh3D,3},2};
inline constexpr Aevrynd::RenderObjectIdentity3D GameplayCaptureForbiddenWeaponGeneration3D{{GameplayCaptureCharacterIdentityHigh3D,4},2};

constexpr GameplayCaptureExpectation3D MakeGameplayCaptureExpectation3D(
    std::wstring_view name,GameplayCaptureCamera3D camera,
    GameplayCaptureLighting3D lighting,bool requireTerrain,
    GameplayCaptureLodExpectation3D lodExpectation) noexcept {
    GameplayCaptureExpectation3D result{};
    result.name=name;result.camera=camera;result.lighting=lighting;
    if(camera==GameplayCaptureCamera3D::First){
        result.requiredIdentities={GameplayCaptureFirstPersonArmsIdentity3D,GameplayCaptureWeaponIdentity3D};result.requiredIdentityCount=2;
        result.forbiddenIdentities={GameplayCaptureForbiddenFirstPersonArmsGeneration3D,GameplayCaptureForbiddenWeaponGeneration3D};result.forbiddenIdentityCount=2;
    }else{
        result.requiredIdentities[0]=GameplayCapturePlayerIdentity3D;result.requiredIdentityCount=1;
        if(requireTerrain){result.requiredIdentities[1]=GameplayCaptureTerrainIdentity3D;++result.requiredIdentityCount;}
        result.forbiddenIdentities[0]=GameplayCaptureForbiddenPlayerGeneration3D;result.forbiddenIdentityCount=1;
    }
    result.lodExpectation=lodExpectation;
    return result;
}

struct GameplayCaptureProductionRoute3D {
    CameraMode camera{};
    LightingPhase lighting{};
    Aevrynd::ProjectionMode projection{Aevrynd::ProjectionMode::Perspective};
};
enum class GameplayActiveCaptureRoutePhase3D:std::uint8_t{Inactive,Prepared,Warming,Readback};
[[nodiscard]] constexpr bool GameplayActiveCaptureRoutePhaseAccepted3D(GameplayActiveCaptureRoutePhase3D phase)noexcept{switch(phase){case GameplayActiveCaptureRoutePhase3D::Prepared:case GameplayActiveCaptureRoutePhase3D::Warming:case GameplayActiveCaptureRoutePhase3D::Readback:return true;case GameplayActiveCaptureRoutePhase3D::Inactive:return false;default:return false;}}
struct GameplayActiveCaptureRoute3D {std::size_t index{std::numeric_limits<std::size_t>::max()};GameplayCaptureProductionRoute3D route{};GameplayActiveCaptureRoutePhase3D phase{GameplayActiveCaptureRoutePhase3D::Inactive};[[nodiscard]] constexpr bool Active()const noexcept{return GameplayActiveCaptureRoutePhaseAccepted3D(phase);}constexpr void Clear()noexcept{*this={};}};

[[nodiscard]] constexpr GameplayCaptureProductionRoute3D
BuildGameplayCaptureProductionRoute3D(const GameplayCaptureExpectation3D& expectation) noexcept {
    const CameraMode camera=expectation.camera==GameplayCaptureCamera3D::First?CameraMode::FirstPerson:
        expectation.camera==GameplayCaptureCamera3D::Third?CameraMode::ThirdPerson:CameraMode::GodView;
    const LightingPhase lighting=expectation.lighting==GameplayCaptureLighting3D::Day?LightingPhase::Day:
        expectation.lighting==GameplayCaptureLighting3D::Dusk?LightingPhase::Dusk:LightingPhase::Night;
    return{camera,lighting,camera==CameraMode::GodView?Aevrynd::ProjectionMode::Orthographic:Aevrynd::ProjectionMode::Perspective};
}

[[nodiscard]] constexpr bool ValidateGameplayCaptureProductionRoute3D(
    const GameplayCaptureProductionRoute3D& route) noexcept {
    return route.projection==(route.camera==CameraMode::GodView?Aevrynd::ProjectionMode::Orthographic:Aevrynd::ProjectionMode::Perspective);
}
[[nodiscard]] constexpr bool ValidateGameplayActiveCaptureRoute3D(const GameplayActiveCaptureRoute3D& active,std::size_t slot,const std::optional<std::size_t>& prepared,CameraMode rulesCamera)noexcept{return active.Active()&&prepared&&active.index==slot&&*prepared==active.index&&ValidateGameplayCaptureProductionRoute3D(active.route)&&active.route.camera==rulesCamera;}

[[nodiscard]] inline bool ApplyGameplayCaptureProductionProjection3D(
    Aevrynd::Camera3D& camera,const GameplayCaptureProductionRoute3D& route) noexcept {
    if(!ValidateGameplayCaptureProductionRoute3D(route))return false;
    camera.projection.mode=route.projection;
    if(route.projection==Aevrynd::ProjectionMode::Perspective){camera.projection.orthographicHeight=0.f;return true;}
    return std::isfinite(camera.projection.orthographicHeight)&&camera.projection.orthographicHeight>=4.f&&camera.projection.orthographicHeight<=180.f;
}

[[nodiscard]] inline bool ValidateGameplayCaptureProductionProjectionMatrix3D(
    const Aevrynd::Camera3D& camera,const GameplayCaptureProductionRoute3D& route,
    float aspectRatio,const Aevrynd::Matrix4& observed) noexcept {
    if(!ValidateGameplayCaptureProductionRoute3D(route)||camera.projection.mode!=route.projection||
       !std::isfinite(aspectRatio)||aspectRatio<=0.f)return false;
    if(route.projection==Aevrynd::ProjectionMode::Orthographic){
        if(!std::isfinite(camera.projection.orthographicHeight)||camera.projection.orthographicHeight<4.f||camera.projection.orthographicHeight>180.f)return false;
    }else if(!std::isfinite(camera.projection.verticalFieldOfView)||camera.projection.verticalFieldOfView<=.1f||
             camera.projection.verticalFieldOfView>=3.f||camera.projection.orthographicHeight!=0.f)return false;
    const auto expected=Aevrynd::CreateViewProjectionMatrix(camera,aspectRatio);bool nonzero=false;
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
        const float actual=observed[row][column],reference=expected[row][column];
        if(!std::isfinite(actual)||!std::isfinite(reference)||std::abs(actual-reference)>1e-6f)return false;
        nonzero|=std::abs(actual)>1e-7f;
    }
    return nonzero;
}

inline constexpr std::array<GameplayCaptureExpectation3D,11> GameplayCaptureExpectations3D{{
    MakeGameplayCaptureExpectation3D(L"first-day.png",GameplayCaptureCamera3D::First,GameplayCaptureLighting3D::Day,false,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"third-day.png",GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Day,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"god-day.png",GameplayCaptureCamera3D::God,GameplayCaptureLighting3D::Day,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"first-dusk.png",GameplayCaptureCamera3D::First,GameplayCaptureLighting3D::Dusk,false,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"third-dusk.png",GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Dusk,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"god-dusk.png",GameplayCaptureCamera3D::God,GameplayCaptureLighting3D::Dusk,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"first-night.png",GameplayCaptureCamera3D::First,GameplayCaptureLighting3D::Night,false,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"third-night.png",GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Night,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"god-night.png",GameplayCaptureCamera3D::God,GameplayCaptureLighting3D::Night,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"palette-building.png",GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Day,true,GameplayCaptureLodExpectation3D::Transition),
    MakeGameplayCaptureExpectation3D(L"melee-combat.png",GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Dusk,true,GameplayCaptureLodExpectation3D::Transition)
}};

[[nodiscard]] constexpr bool ValidateGameplayCaptureExpectationTable3D() noexcept {
    for(std::size_t index=0;index<GameplayCaptureExpectations3D.size();++index){
        const auto& value=GameplayCaptureExpectations3D[index];
        if(value.name.empty()||!value.requiredIdentityCount||value.requiredIdentityCount>value.requiredIdentities.size()||!value.forbiddenIdentityCount||value.forbiddenIdentityCount>value.forbiddenIdentities.size())return false;
        for(std::size_t other=0;other<index;++other)if(value.name==GameplayCaptureExpectations3D[other].name)return false;
        for(std::size_t required=0;required<value.requiredIdentityCount;++required){if(!value.requiredIdentities[required])return false;for(std::size_t forbidden=0;forbidden<value.forbiddenIdentityCount;++forbidden)if(value.requiredIdentities[required]==value.forbiddenIdentities[forbidden])return false;}
    }
    return true;
}
static_assert(ValidateGameplayCaptureExpectationTable3D());

[[nodiscard]] inline bool EvaluateGameplayCaptureSemantic3D(
    const GameplayCaptureExpectation3D& expectation,
    const Aevrynd::ObjectIdCapture3D& capture,
    GameplayCaptureCamera3D camera,GameplayCaptureLighting3D lighting,
    std::uint32_t& requiredMatched,std::uint32_t& forbiddenMatched,
    std::uint32_t& generationConflicts) noexcept {
    requiredMatched=0;forbiddenMatched=0;generationConflicts=0;
    for(std::size_t index=0;index<expectation.requiredIdentityCount;++index){const auto expected=expectation.requiredIdentities[index];requiredMatched+=capture.RequiredIdentityPixels(expected)>0?1u:0u;generationConflicts+=capture.HasWrongGenerationTokenOrPixels(expected.id,expected.generation)?1u:0u;}
    for(std::size_t index=0;index<expectation.forbiddenIdentityCount;++index)forbiddenMatched+=capture.Semantic(expectation.forbiddenIdentities[index])?1u:0u;
    return requiredMatched==expectation.requiredIdentityCount&&!forbiddenMatched&&!generationConflicts&&camera==expectation.camera&&lighting==expectation.lighting;
}

[[nodiscard]] inline bool EvaluateGameplayCaptureShadowEvidence3D(
    const Aevrynd::DirectionalShadowFrameProvenance3D& shadow,
    const Aevrynd::ReadbackRasterProvenance3D& effectiveRaster) noexcept {
    return shadow.Complete()&&effectiveRaster.Complete()&&shadow.frameSerial==effectiveRaster.frameSerial&&shadow.activeCascades&&shadow.resourceGeneration&&shadow.casters&&shadow.drawCalls&&shadow.cascadeDrawCalls;
}

[[nodiscard]] constexpr bool EvaluateGameplayCaptureLodEvidence3D(
    const GameplayCaptureExpectation3D& expectation,
    std::uint64_t evidenceFrameSerial,std::uint64_t effectiveFrameSerial,
    std::uint32_t instances,std::uint32_t transitions,
    std::uint32_t dualSubmissions,std::uint32_t budgetFallbacks) noexcept {
    const bool transitionExpected=expectation.lodExpectation==GameplayCaptureLodExpectation3D::Transition;
    return evidenceFrameSerial&&evidenceFrameSerial==effectiveFrameSerial&&instances&&!budgetFallbacks&&(transitions>0)==transitionExpected&&(dualSubmissions>0)==transitionExpected;
}

// 三種讀回必須在同一個渲染階段全部接受，任何部分成功都視為失敗並取消序列。
[[nodiscard]] constexpr bool GameplayCapturePhaseRequestsAtomic3D(
    bool depthRequested,bool objectIdRequested,bool colorRequested) noexcept {
    return depthRequested&&objectIdRequested&&colorRequested;
}

// 階段在 OnUpdate 排隊後、OnRender 正式送出前仍必須套用對應畫面狀態。
[[nodiscard]] constexpr bool GameplayCaptureQueuedPhaseActive3D(
    bool pairFrozen,bool phaseSelected) noexcept {
    return pairFrozen&&phaseSelected;
}

// 只有超過發布遮擋門檻的建築才需要reference visibility override；清晰構圖
// 必須維持raw/effective/reference相同opacity fingerprint。
inline constexpr float GameplayReferenceBuildingRemovalThreshold3D=.08f;
struct GameplayReferenceBuildingOverridePlan3D {
    enum class Mode : std::uint8_t { NoOverride, RemoveAll };
    float removalThreshold{},opacity{1.f};Mode mode{Mode::NoOverride};
    [[nodiscard]] constexpr bool Removes()const noexcept{return mode==Mode::RemoveAll&&opacity==0.f;}
    [[nodiscard]] constexpr std::uint32_t RequestedCount()const noexcept{return Removes()?6u:0u;}
};

struct GameplayCaptureScenarioClock3D {
    enum class Phase : std::uint8_t { Transition, Staging, Readback, Complete, Failed };
    static constexpr std::uint32_t InitialSimulationSteps=30;
    static constexpr std::uint32_t TransitionSimulationSteps=12;
    std::size_t scenario{};
    std::uint64_t simulationTick{};
    std::uint32_t remainingSimulationSteps{InitialSimulationSteps};
    Phase phase{Phase::Transition};
    [[nodiscard]] constexpr bool AdvancesSimulation()const noexcept{return phase==Phase::Transition;}
    [[nodiscard]] constexpr bool ReadyToStage()const noexcept{return phase==Phase::Staging;}
    [[nodiscard]] constexpr bool WaitingForReadback()const noexcept{return phase==Phase::Readback;}
    constexpr bool RecordSimulationStep(bool scenarioReady)noexcept{
        if(!AdvancesSimulation())return false;
        ++simulationTick;
        if(remainingSimulationSteps)--remainingSimulationSteps;
        if(!remainingSimulationSteps&&scenarioReady)phase=Phase::Staging;
        return true;
    }
    constexpr bool BeginReadback(std::size_t expectedScenario)noexcept{
        if(!ReadyToStage()||scenario!=expectedScenario)return false;
        phase=Phase::Readback;return true;
    }
    constexpr bool CompleteReadback(std::size_t expectedScenario,std::size_t scenarioCount)noexcept{
        if(!WaitingForReadback()||scenario!=expectedScenario||!scenarioCount||scenario>=scenarioCount)return false;
        ++scenario;
        if(scenario==scenarioCount){phase=Phase::Complete;remainingSimulationSteps=0;}
        else{phase=Phase::Transition;remainingSimulationSteps=TransitionSimulationSteps;}
        return true;
    }
    constexpr bool FailReadback(std::size_t expectedScenario)noexcept{
        if((!WaitingForReadback()&&!ReadyToStage())||scenario!=expectedScenario)return false;
        phase=Phase::Failed;return true;
    }
};
[[nodiscard]] constexpr GameplayReferenceBuildingOverridePlan3D GameplayReferenceBuildingOverridePlan(float coverage)noexcept{
    return coverage>GameplayReferenceBuildingRemovalThreshold3D?
        GameplayReferenceBuildingOverridePlan3D{GameplayReferenceBuildingRemovalThreshold3D,0.f,GameplayReferenceBuildingOverridePlan3D::Mode::RemoveAll}:
        GameplayReferenceBuildingOverridePlan3D{GameplayReferenceBuildingRemovalThreshold3D,1.f,GameplayReferenceBuildingOverridePlan3D::Mode::NoOverride};
}
[[nodiscard]] constexpr float GameplayReferenceBuildingOpacity3D(float coverage) noexcept {
    return GameplayReferenceBuildingOverridePlan(coverage).opacity;
}

// 以讀回序列與階段鎖住真正送進 Scene 的統計，禁止後續 OnRender 覆寫證據。
struct GameplayCaptureScenePhaseLedger3D {
    [[nodiscard]] bool Begin(Aevrynd::ReadbackCaptureSequenceToken3D token,std::size_t captureIndex,std::uint32_t phaseCount) noexcept {
        Clear();
        if(!token||phaseCount<2||phaseCount>phases.size())return false;
        token_=token;captureIndex_=captureIndex;phaseCount_=phaseCount;return true;
    }
    [[nodiscard]] bool Record(Aevrynd::ReadbackCaptureSequenceToken3D token,std::size_t captureIndex,std::uint32_t phase,const Aevrynd::Scene3DStats& stats) noexcept {
        if(token!=token_||captureIndex!=captureIndex_||phase>=phaseCount_||recorded[phase]||!stats.submissionSerial||!stats.drawListRevision||!stats.acceptedIdentityOrderFingerprint)return false;
        phases[phase]=stats;recorded[phase]=1;return true;
    }
    [[nodiscard]] const Aevrynd::Scene3DStats* Resolve(Aevrynd::ReadbackCaptureSequenceToken3D token,std::size_t captureIndex,std::uint32_t phase)const noexcept {
        return token==token_&&captureIndex==captureIndex_&&phase<phaseCount_&&recorded[phase]?&phases[phase]:nullptr;
    }
    [[nodiscard]] bool BindRequest(Aevrynd::ReadbackCaptureSequenceToken3D token,std::size_t captureIndex,std::uint32_t phase,Aevrynd::ObjectIdCaptureRequestMetadata3D& metadata)const noexcept {
        const auto* stats=Resolve(token,captureIndex,phase);
        if(!stats||metadata.sequence.token!=token||metadata.sequence.index!=phase||!metadata.cameraHash||!metadata.stateHash||!metadata.sequenceInvariantHash||metadata.emptyReason!=Aevrynd::ReadbackEmptyReason3D::None)return false;
        metadata.drawListRevision=stats->drawListRevision;return true;
    }
    void Clear() noexcept {token_={};captureIndex_=(std::numeric_limits<std::size_t>::max)();phaseCount_=0;phases={};recorded={};}
    std::array<Aevrynd::Scene3DStats,3> phases{};
    std::array<std::uint8_t,3> recorded{};
private:
    Aevrynd::ReadbackCaptureSequenceToken3D token_{};
    std::size_t captureIndex_{(std::numeric_limits<std::size_t>::max)()};
    std::uint32_t phaseCount_{};
};

// GPU 讀回即使 fail-closed，也必須先保留該幀真正的完整性與 canonical 證據。
// 寫檔接線會在 frame-bound renderer API 定案後使用，避免誤讀下一幀統計。
struct GameplayCaptureIntegrityDiagnosticSnapshot3D {
    std::size_t captureIndex{};
    std::uint32_t phaseIndex{};
    Aevrynd::FrameBoundDeterminismEvidence3D objectId{};
    Aevrynd::FrameBoundDeterminismEvidence3D depth{};
    bool recorded{};

    [[nodiscard]] constexpr bool Complete() const noexcept {
        return recorded && objectId.Complete() && depth.Complete() && objectId == depth &&
            objectId.integrity.requestedMode == Aevrynd::DeterminismIntegrityMode3D::SealedCapture &&
            objectId.integrity.actualMode == Aevrynd::DeterminismIntegrityMode3D::SealedCapture;
    }
};

enum class GameplayHdrCaptureState3D : std::uint8_t { Empty, Armed, SceneBound, BundleRequested, AwaitGpu, Taken, Published, Verified, Failed };

struct GameplayHdrCaptureReceipt3D {
    GameplayHdrCaptureState3D state{GameplayHdrCaptureState3D::Empty};
    std::filesystem::path path{};
    std::array<std::uint8_t,32> fileHash{},stableContentHash{},payloadHash{},rootHash{};
    std::uint64_t byteCount{},frameSerial{},sequenceToken{};
    std::uint32_t sequenceIndex{};
    bool verified{};
};
}

std::string CreateGameplayLabRunId();

class GameplayLabGame final:public Aevrynd::IGame {
    bool diagnosticsVisible_{};
public:
    explicit GameplayLabGame(std::filesystem::path captureDirectory={},std::uint32_t temporalFixtureWarmupFrames=0,bool temporalFixtureCloudOff=false):combat_(std::make_unique<GameplayLab::GameplayCombat3D>(collisions_,physics_)),palette_(std::make_unique<GameplayLab::ObjectPaletteAdapter3D>(rules_,collisions_,physics_,sceneEcs_)),paletteUi_(std::make_unique<GameplayLab::GameplayObjectPaletteUi3D>(*palette_)),captureDirectory_(std::move(captureDirectory)),runId_(CreateGameplayLabRunId()),temporalFixtureWarmupFrames_(temporalFixtureWarmupFrames),temporalFixtureCloudOff_(temporalFixtureCloudOff){}
    void OnStart(Aevrynd::Engine&)override;void OnUpdate(Aevrynd::Engine&,float)override;void OnRender(Aevrynd::Engine&,float)override;void OnStop(Aevrynd::Engine&)override;
    Aevrynd::ReusableMemoryStats GetReusableMemoryStats()const override;std::size_t TrimReusableMemory()override;
private:
    void PreserveProjectionFailure(Aevrynd::Engine&,std::string reason);
    void FinishProjectionFailure(Aevrynd::Engine&);
    std::string projectionFailureReason_{};
    std::filesystem::path projectionFailureImage_{};
    std::uint64_t projectionFailureCaptureCount_{},projectionFailureRejectedCount_{};
    bool projectionFailurePending_{};
    bool projectionFailureStopFinished_{};
    void PublishFirstPersonPose(float simulationDelta);
    struct FirstPersonPosePublication {
        GameplayLab::GameplayFirstPersonPoseOutput3D output{};
        GameplayLab::AnimationState3D state{GameplayLab::AnimationState3D::Idle};
        float phase{},enterBlend{};
        std::uint64_t tick{},sourcePoseHash{},cameraHash{},hash{};
        bool female{},active{};
    };
    GameplayLab::GameplayFirstPersonPoseWorkspace3D firstPersonPoseWorkspace_{};
    FirstPersonPosePublication firstPersonPose_{},captureFrozenFirstPersonPose_{};
    void BuildWorld(Aevrynd::Engine&);void AddShowcaseEnvironment();void QueueTemporalHistoryReseed(GameplayLab::GameplayTemporalReseedReason3D)noexcept;void ResetProductionCameraCut(GameplayLab::GameplayTemporalReseedReason3D=GameplayLab::GameplayTemporalReseedReason3D::CameraMode)noexcept;void UpdateCamera(Aevrynd::Engine&);void UpdateCapture(Aevrynd::Engine&,bool fixedSimulationStep);void RequestCapturePhase(Aevrynd::Engine&);bool ConsumeObjectIdCapture(Aevrynd::Engine&,std::size_t,bool raw,bool reference);bool WriteCaptureIntegrityDiagnostic(bool fatal=false)const;std::filesystem::path GameplayHdrCapturePath(std::size_t)const;void CleanupGameplayHdrCaptureArtifacts(std::size_t,bool removeFinal)noexcept;void FailGameplayHdrCapture(Aevrynd::Engine&,std::size_t)noexcept;bool WriteGameplayHdrCaptureReceipt()const;bool InitializeEnvironmentCollision();bool SyncEnvironmentCollision(Aevrynd::Vec3 observer);void ReleaseEnvironmentGameplayColliders();
    std::uint32_t temporalFixtureWarmupFrames_{};bool temporalFixtureCloudOff_{};
    std::uint8_t captureLightingIsolation_{};bool captureShadowMapDiagnostic_{},captureRawHdrDiagnostic_{},captureAtmosphereOutputDiagnostic_{};
    GameplayLab::Rules3D rules_{};Aevrynd::CharacterController3D controller_{};Aevrynd::CollisionWorld3D collisions_{};Aevrynd::PhysicsWorld3D physics_{};Aevrynd::SceneECSRegistry3D sceneEcs_{};Aevrynd::Scene3D scene_{};
    GameplayLab::ShowcaseRenderAssetBindings3D showcaseRenderAssets_{};
    GameplayLab::ShowcaseProductionRenderReceipt3D showcaseProductionReceipt_{};
    bool inputFocused_{true},cursorCaptured_{};Aevrynd::Input captureReplayInput_{};std::optional<Aevrynd::Camera3D> captureReplayInteractionCamera_{};std::uint64_t captureReplayInteractionCameraInputHash_{},captureReplayInteractionCameraSubjectPoseHash_{};
    // 僅保留未來除錯剛體的可視化清單；所有可互動物件的擁有權都在 palette_。
    Aevrynd::MeshResource3D boxMesh_{},sphereMesh_{};std::vector<Aevrynd::RigidBodyHandle3D> bodies_{};Aevrynd::Camera3D camera_{};float yaw_{},pitch_{.2f},simulationDelta_{1.f/60.f};
    GameplayLab::GameplayCameraOcclusionPolicy3D cameraOcclusionPolicy_{};
    float cameraOccluderCoverage_{},cameraRawOccluderCoverage_{},thirdPersonOrbitOffset_{},thirdPersonDistance_{3.4f},cameraFadeAlpha_{1.f};std::uint64_t cameraOccluderStableLow_{};Aevrynd::RenderObjectIdentity3D cameraPolicyFadeIdentity_{},appliedOcclusionIdentity_{};Aevrynd::ObjectOcclusionOpacityUpdate3D appliedOcclusionUpdate_{};float appliedOcclusionOpacity_{1.f};bool cameraFadeSettled_{},cameraOcclusionAdjusted_{};
    GameplayLab::GameplayCameraProductionBuildReceipt3D cameraBuilderReceipt_{};GameplayLab::CameraOcclusionPolicyOutput3D cameraPolicyReceipt_{};GameplayLab::ProductionCameraCandidate3D cameraSelectedCandidate_{};Aevrynd::Camera3D cameraBuilderExpected_{};bool cameraBuilderMatrixExact_{};
    GameplayLab::GameplayCameraProductionBuildOutput3D cameraBuildCache_{};GameplayLab::GameplayGodCameraLayout3D godCameraBuildCache_{};GameplayLab::GameplayAuthoritativeGodCameraReceipt3D godCameraAuthorityReceipt_{};Aevrynd::Vec3 cameraBuildCacheFeet_{},cameraBuildCacheCompanionPosition_{},godCameraBuildCacheFocus_{};Aevrynd::Vec2 cameraBuildCacheViewport_{};float cameraBuildCacheYaw_{},cameraBuildCacheDesiredArm_{4.5f},cameraBuildCacheMinimumSubjectScreenArea_{.05f};GameplayLab::AnimationState3D cameraBuildCachePlayerAnimation_{GameplayLab::AnimationState3D::Idle},cameraBuildCacheCompanionAnimation_{GameplayLab::AnimationState3D::Idle};std::uint64_t godCameraSearchCount_{},godCameraCacheHitCount_{};bool cameraBuildCacheValid_{},cameraBuildCacheFemale_{},cameraBuildCacheCrouched_{},cameraBuildCachePaletteOpen_{},godCameraBuildCacheValid_{},godCameraBuildCacheFemale_{};
    Aevrynd::Mesh3D cameraPoseVisualMesh_{},cameraCompanionVisualMesh_{},godCameraPoseVisualMesh_{};Aevrynd::MeshBounds3D cameraBuildCacheVisualBounds_{},cameraBuildCacheCompanionBounds_{};bool cameraBuildCacheHasCompanion_{},temporalCameraInitialized_{},temporalHistoryCutPending_{};std::uint32_t temporalHistoryPendingReasonMask_{};std::uint64_t temporalSceneIdentityFingerprint_{};
    GameplayLab::GameplayShowcaseEnvironment3D showcaseEnvironment_{};Aevrynd::HeightField3D showcaseTerrain_{};std::unique_ptr<Aevrynd::EnvironmentCollisionStreaming3D> environmentCollisions_{};std::vector<Aevrynd::EnvironmentColliderDescriptor3D> environmentCollisionDescriptors_{};std::vector<Aevrynd::ColliderHandle3D> environmentGameplayColliders_{};Aevrynd::EnvironmentCollisionStreamingReceipt3D environmentCollisionActiveReceipt_{};Aevrynd::MeshResource3D showcaseTerrainMesh_{},showcaseRiverMesh_{},showcaseCottageFrontMesh_{},showcaseCottageBackMesh_{},showcaseCottageSideMesh_{},showcaseCottageRoofMesh_{},showcaseCylinderMesh_{},showcaseConeHighMesh_{},showcaseConeLowMesh_{},showcaseGrassHighMesh_{},showcaseGrassLowMesh_{},showcaseFernHighMesh_{},showcaseFernLowMesh_{},showcaseShrubHighMesh_{},showcaseShrubLowMesh_{},showcaseTreeLowMesh_{},showcaseRockLowMesh_{},showcaseQuiverTreeMesh_{},showcaseRock09Mesh_{};Aevrynd::MaterialLibrary3D showcaseMaterialLibrary_{},showcaseModelMaterialLibrary_{};Aevrynd::Material3D showcaseGroundMaterial_{},showcaseWoodMaterial_{},showcaseBarkMaterial_{},showcaseRockMaterial_{},showcaseQuiverTreeMaterial_{},showcaseRock09Material_{};Aevrynd::RendererStats showcaseRendererBaseline_{};std::size_t showcaseSubmittedDescriptors_{};bool showcaseExecutionObserved_{},environmentCollisionReloadStable_{},environmentStableIdEvidence_{},environmentRayEvidence_{},environmentMeleeQueryEvidence_{};
    bool candidate7Active_{};
    GameplayLab::CharacterPresentationScale3D candidate7PresentationScale_{};
    std::array<Aevrynd::SkinnedModel3D,3> candidate7AttachmentModels_{};
    std::array<Aevrynd::SkinnedMeshResource3D,3> candidate7AttachmentMeshes_{};
    std::array<std::vector<Aevrynd::SkinnedSubmesh3D>,3> candidate7AttachmentSubmeshes_{};
    std::array<std::vector<Aevrynd::Material3D>,4> candidate7Materials_{};
    std::array<Aevrynd::MaterialLibrary3D,4> candidate7MaterialLibraries_{};
    Aevrynd::SkinnedModel3D femaleModel_{},maleModel_{},femaleFirstPersonArmsModel_{},maleFirstPersonArmsModel_{};GameplayLab::GameplayFirstPersonArmsEvidence3D femaleFirstPersonArmsEvidence_{},maleFirstPersonArmsEvidence_{};GameplayLab::GameplayCharacterMaterialEvidence3D characterMaterialEvidence_{};std::vector<Aevrynd::SkinnedSubmesh3D> femaleSubmeshes_{},maleSubmeshes_{},femaleFirstPersonArmsSubmeshes_{},maleFirstPersonArmsSubmeshes_{},femaleWeaponSubmeshes_{},maleWeaponSubmeshes_{};std::vector<Aevrynd::Material3D> femaleMaterials_{},maleMaterials_{};std::array<Aevrynd::Material3D,2> femaleWeaponMaterials_{},maleWeaponMaterials_{};Aevrynd::MaterialLibrary3D femaleMaterialLibrary_{},maleMaterialLibrary_{};Aevrynd::SkinnedMeshResource3D femaleMesh_{},maleMesh_{},femaleFirstPersonArmsMesh_{},maleFirstPersonArmsMesh_{},femaleWeaponMesh_{},maleWeaponMesh_{};Aevrynd::AnimationPose3D femalePose_{},malePose_{},captureFrozenFemalePose_{},captureFrozenMalePose_{},captureFrozenMonsterPose_{};Aevrynd::SkinningWorkspace3D skinning_{};bool capturePoseFrozen_{};
    std::unique_ptr<GameplayLab::GameplayCombat3D> combat_{};GameplayLab::GameplayAnimation3D femaleAnimation_{},maleAnimation_{},monsterAnimation_{};Aevrynd::AnimationPose3D monsterPose_{};Aevrynd::FootPlacement3D femaleFootPlacement_{},maleFootPlacement_{};Aevrynd::SkinnedRenderableGroupHistory3D playerHistory_{},monsterHistory_{};Aevrynd::SkinnedRenderableGroupStats3D skinnedGroupStats_{};std::array<std::uint64_t,11> femalePoseHashes_{},malePoseHashes_{};
    std::unique_ptr<GameplayLab::ObjectPaletteAdapter3D> palette_{};std::unique_ptr<GameplayLab::GameplayObjectPaletteUi3D> paletteUi_{};std::vector<std::uint64_t> initialSeedEntityIds_{};std::uint64_t paletteUiDrawnFrames_{},captureCleanHudFrames_{};bool palettePointerInteraction_{};
    std::uint64_t captureFrozenFootPlacementHash_{};std::array<std::uint64_t,11> captureRawFemalePoseHash_{},captureRawMalePoseHash_{},captureRawMonsterPoseHash_{},captureEffectiveFemalePoseHash_{},captureEffectiveMalePoseHash_{},captureEffectiveMonsterPoseHash_{},captureReferenceFemalePoseHash_{},captureReferenceMalePoseHash_{},captureReferenceMonsterPoseHash_{},captureRawFootPlacementHash_{},captureEffectiveFootPlacementHash_{},captureReferenceFootPlacementHash_{};
    std::array<std::uint32_t,11> captureDrawIdentityCount_{},captureDrawTerrainCount_{},captureDrawTerrainIdentityCount_{},captureDrawGroundIdentityCount_{},captureDrawGroundOpaqueCount_{},captureDrawGroundShadowOnlyCount_{};
    std::array<std::uint32_t,11> captureGroundMeshHandle_{},captureGroundCastsShadow_{},captureGroundAlphaMode_{},captureRendererSubmittedMeshes_{},captureRendererSubmittedTerrain_{};
    std::array<std::uint32_t,11> captureShadowCasters_{},captureShadowDrawCalls_{},captureShadowCascadeDrawCalls_{},captureShadowCascadeRendered_{},captureShadowCascadeReused_{},captureShadowActiveCascades_{},captureLodInstances_{},captureLodTransitions_{},captureLodDualSubmissions_{},captureLodBudgetFallbacks_{},captureSemanticRequiredMatched_{},captureSemanticForbiddenMatched_{},captureSemanticGenerationConflicts_{};
    std::array<std::array<std::uint64_t,2>,11> captureSemanticRequiredPixels_{};
    std::array<std::array<double,2>,11> captureSemanticRequiredCoverage_{};
    std::array<Aevrynd::DirectionalShadowFrameProvenance3D,11> captureShadowProvenance_{};
    std::array<std::uint64_t,11> captureShadowFrameSerial_{},captureLodFrameSerial_{},captureSemanticFrameSerial_{},captureSceneSubmissionSerial_{},captureSceneDrawRevision_{},captureSceneIdentityOrderFingerprint_{};
    std::array<std::uint8_t,11> captureSemanticExpectationPassed_{},captureShadowEvidencePassed_{},captureLodEvidencePassed_{},captureLodPredictionMatched_{},captureObservedCamera_{},captureObservedLighting_{};
    std::array<std::uint32_t,11> captureGroundFrustumAccepted_{};std::array<float,11> captureGroundBoundsRadius_{},captureGroundDistanceMinusRadius_{},captureGroundViewDepth_{},captureGroundProjectedRadius_{};
    std::array<Aevrynd::ReadbackRasterProvenance3D,11> captureRawRasterProvenance_{},captureEffectiveRasterProvenance_{},captureReferenceRasterProvenance_{};
    std::array<Aevrynd::ReadbackCaptureSequenceReceipt3D,11> captureSequenceReceipts_{};std::array<std::array<Aevrynd::AtmosphereReadbackProvenance3D,3>,11> captureAtmospherePhaseProvenance_{},captureTemporalPhaseProvenance_{};std::array<std::uint64_t,11> captureTemporalHistoryGeneration_{};std::array<std::uint32_t,11> captureTemporalReseedReasonMask_{},captureTemporalWarmupFrames_{};
    std::array<std::uint32_t,11> captureReferenceRequestedCount_{},captureReferenceRemovedCount_{};std::array<float,11> captureReferenceRemovalThreshold_{},captureReferenceRemovedCoverage_{};std::array<std::uint8_t,11> captureReferenceOverrideMode_{};
    std::array<std::array<std::uint8_t,6>,11> captureReferenceRequestedById_{},captureReferenceMatchedById_{},captureReferenceMissingById_{},captureReferenceConflictById_{};std::array<std::array<float,6>,11> captureReferenceAppliedOpacityById_{};
    GameplayLab::GameplayCaptureScenePhaseLedger3D captureScenePhaseLedger_{};
    GameplayLab::GameplayCaptureScenarioClock3D captureScenarioClock_{};
    std::array<std::wstring,11> captureRequestedEffectiveNames_{},captureCommittedEffectiveNames_{};
    std::array<std::filesystem::path,11> captureRequestedEffectivePaths_{},captureCommittedEffectivePaths_{};
    Aevrynd::ReadbackCaptureSequenceToken3D captureSequenceToken_{};std::uint32_t captureSequencePhaseCount_{};
    std::array<GameplayLab::GameplayHdrCaptureState3D,11> gameplayHdrCaptureStates_{};
    std::array<GameplayLab::GameplayHdrCaptureReceipt3D,11> gameplayHdrCaptureReceipts_{};
    std::uint64_t gameplayHdrEnginePeakBytes_{};
    std::uint32_t gameplayHdrInFlight_{},gameplayHdrMaxInFlight_{};
    GameplayLab::GameplayAuthoritativeTrajectoryEvidence3D authoritativeTrajectory_{};
    Aevrynd::CharacterSwordGeometry3D femaleSwordGeometry_{},maleSwordGeometry_{};
    GameplayLab::GameplayFirstPersonWeaponProjectionGeometry3D femaleWeaponProjection_{},maleWeaponProjection_{};
    std::array<std::uint32_t,11> captureContactLeftPixels_{},captureContactRightPixels_{},captureContactGroundSupportPixels_{},captureContactShadowPixels_{};
    std::array<std::uint64_t,11> captureContactFrameSerial_{},captureContactPoseHash_{};
    std::array<std::array<std::byte,32>,11> captureContactArtifactSha256_{};
    std::array<std::uint32_t,11> captureCompanionPixels_{},capturePlayerPixels_{},captureActorGapPixels_{};
    std::array<std::uint8_t,11> captureActorSeparationPassed_{};
    std::array<std::uint8_t,11> captureProjectionMode_{};
    std::array<float,11> captureVerticalFovRadians_{},captureOrthographicHeight_{},captureGodActualTriangleCoverage_{},captureGodBuildingUnion_{},captureGodMaximumBuildingPart_{},captureGodRoofCoverage_{},captureGodRiverCoverage_{};
    std::array<std::uint32_t,11> captureGodVisibleWorldDescriptors_{},captureGodLodTransitions_{},captureGodVisibleBuildingComponents_{},captureGodBuildingEdgeMask_{};
    std::array<std::uint64_t,11> captureFrameSceneDrawRevision_{},captureFrameWorldRevision_{},captureFrameSubmittedMeshes_{},captureFrameSubmittedSkinned_{},captureFrameSubmittedTerrain_{},captureFrameSubmittedWorldVertices_{},captureFrameSubmittedWorldIndices_{};
    std::array<GameplayLab::ShowcaseCameraVisibility3D,11> captureEnvironmentVisibility_{};
    std::array<std::vector<GameplayLab::ShowcaseLodSample3D>,11> captureEnvironmentLodSamples_{};
    std::vector<GameplayLab::ShowcaseSubmittedLodIdentity3D> showcaseSubmittedLodItems_{};
    std::array<std::vector<GameplayLab::ShowcaseLodRenderRow3D>,11> captureSubmittedLodRows_{},captureActualLodRows_{};
    std::array<GameplayLab::ShowcaseLodContractReceipt3D,11> captureLodContracts_{};
    std::array<GameplayLab::GameplayCanonicalCaptureStateReceipt3D,11> captureCanonicalState_{};std::array<GameplayLab::GameplayPaletteReplayDiagnostic3D,3> capturePaletteReplayDiagnostics_{};std::uint64_t authorityFootPlacementHash_{};float authorityFootPlacementCameraDistanceInput_{};
    std::array<Aevrynd::DeterminismIntegrityReceipt3D,11> captureDeterminismIntegrity_{};
    std::array<Aevrynd::CanonicalDrawFingerprintReceipt3D,11> captureCanonicalDrawFingerprint_{};
    std::array<std::uint64_t,11> captureDeterminismFrameSerial_{};
    std::array<std::array<GameplayLab::GameplayCaptureIntegrityDiagnosticSnapshot3D,3>,11> captureIntegrityDiagnostics_{};
    std::array<std::uint32_t,11> captureFirstPersonArmsToken_{},captureFirstPersonArmsPixels_{},captureFirstPersonArmsMinimumX_{},captureFirstPersonArmsMinimumY_{},captureFirstPersonArmsMaximumX_{},captureFirstPersonArmsMaximumY_{},captureFirstPersonWeaponToken_{},captureFirstPersonWeaponPixels_{},captureFirstPersonWeaponMinimumX_{},captureFirstPersonWeaponMinimumY_{},captureFirstPersonWeaponMaximumX_{},captureFirstPersonWeaponMaximumY_{};
    bool captureTemporalWarmupActive_{},capturePreparedSceneSubmitted_{};std::uint32_t captureTemporalWarmupRemaining_{},captureTemporalWarmupFramesCurrent_{},captureTemporalReseedReasonCurrent_{};
    std::array<float,11> captureBuilderTopCenterRayElevation_{},
        captureBuilderMinimumTopCenterRayElevation_{},
        captureBuilderMinimumSubjectScreenArea_{};
    std::array<std::uint64_t,11> captureBuilderRejectedHorizon_{};
    std::array<std::uint8_t,11> captureBuilderPaletteOpen_{};
    std::array<std::array<std::byte,32>,11> captureRawDepthFinalFileSha256_{},captureEffectiveDepthFinalFileSha256_{},captureReferenceDepthFinalFileSha256_{};
    std::array<std::array<std::byte,32>,11> captureRawTokenMetadataSha256_{},captureEffectiveTokenMetadataSha256_{};
    std::array<std::uint64_t,11> captureRawDepthFinalFileBytes_{},captureEffectiveDepthFinalFileBytes_{},captureReferenceDepthFinalFileBytes_{};
    bool capturePresentationWarmupActive_{},capturePresentationRequestPending_{},capturePresentationPending_{};
    std::uint32_t capturePresentationWarmupRemaining_{},capturePresentationWarmupFramesCurrent_{},captureDiagnosticAsyncWaitFrames_{};
    Aevrynd::ReadbackCaptureSequenceToken3D capturePresentationLogicalToken_{};
    std::array<std::uint64_t,11> capturePresentationFrameSerial_{},capturePresentationCameraHash_{},capturePresentationStateHash_{},capturePresentationInvariantHash_{};
    std::array<std::uint8_t,11> capturePresentationMsaa_{},capturePresentationTaa_{},capturePresentationHistoryUsed_{},capturePresentationPaired_{};
    std::uint64_t captureUltraSettleLastSubmittedFrame_{};
    std::uint32_t captureUltraSettleFrames_{};
    std::string captureReadbackFailure_{};
    std::filesystem::path captureDirectory_{};std::optional<Aevrynd::Vec3> captureValidGhost_{},captureInvalidGhost_{};std::optional<std::size_t> capturePreparedIndex_{};GameplayLab::GameplayActiveCaptureRoute3D activeCaptureRoute_{};std::size_t captureIndex_{};bool capturePending_{},capturePhaseRequestPending_{},captureRawPhase_{},captureReferencePhase_{},capturePairFrozen_{},captureCrouchTapPending_{},combatSpawned_{},crouchFeetInvariant_{true},weaponAttachmentPassed_{},weaponHitWindowEvidence_{},weaponAnimationSyncPassed_{true},firstPersonBodyHidden_{true},captureGrabSeeded_{},sourceFingerprintPassed_{true};Aevrynd::ObjectIdCaptureRequestMetadata3D capturePairMetadata_{};Aevrynd::ColliderHandle3D captureCeiling_{},captureEvidenceCollider_{};float minimumControllerHeight_{1.8f},maximumWeaponPhaseError_{};double nextCapture_{.5};std::uint64_t captureSuccess_{},captureReject_{},femaleGpuDraws_{},maleGpuDraws_{},weaponPbrDraws_{},captureAuthoritativePhysicsSteps_{},weaponTrajectoryHash_{14695981039346656037ull};std::array<std::uint64_t,3> attackTrajectoryHashes_{{14695981039346656037ull,14695981039346656037ull,14695981039346656037ull}};std::array<std::uint32_t,11> captureBodyMaterialSlots_{},captureWeaponMaterialRanges_{},captureObjectIdSubmitted_{},captureOpacityMatchedStatic_{},captureOpacityMatchedMultipart_{},captureReferenceOpacityMatches_{},captureDepthGeneration_{},captureLeftFootGroundGapPixels_{},captureRightFootGroundGapPixels_{},capturePlayerToken_{},captureTerrainToken_{},captureFootRoiDominantToken_{};std::array<float,11> captureWeaponAnchorErrors_{},captureWeaponScreenCoverage_{},captureWeaponMinimumDepth_{},captureWeaponGripX_{},captureWeaponGripY_{},captureWeaponBladeY_{},captureOccluderCoverage_{},captureRawOccluderCoverage_{},capturePolicyFadeOpacity_{},captureFeetGroundError_{},captureRawBuildingUnion_{},captureEffectiveBuildingUnion_{},captureRawMaximumVisibleCoverage_{},captureEffectiveMaximumVisibleCoverage_{},captureCharacterReferenceCoverage_{},captureCharacterEffectiveCoverage_{},captureCharacterVisibility_{},captureCharacterSilhouetteIou_{},captureLeftFootDepthDelta_{},captureRightFootDepthDelta_{},captureFootRoiDepthMin_{},captureFootRoiDepthMax_{},captureBuilderScreenHeight_{},captureBuilderScreenWidth_{},captureBuilderScreenArea_{},captureBuilderHeadroom_{},captureBuilderYaw_{},captureBuilderPitch_{},captureBuilderElevation_{},captureBuilderArm_{},captureBuilderPositionX_{},captureBuilderPositionY_{},captureBuilderPositionZ_{};std::array<std::array<float,6>,11> captureRawBuildingCoverage_{},captureEffectiveBuildingCoverage_{};std::vector<std::uint8_t> captureEffectiveCharacterMask_{};std::array<std::array<std::byte,32>,11> captureRawDepthSha256_{},captureEffectiveDepthSha256_{},captureReferenceDepthSha256_{};std::array<std::uint64_t,11> captureOccluderStableHigh_{},captureOccluderStableLow_{},captureOccluderGeneration_{},captureFadedStableHigh_{},captureFadedStableLow_{},captureFadedGeneration_{},capturePolicyFadeStableHigh_{},capturePolicyFadeStableLow_{},capturePolicyFadeGeneration_{},captureGroundStableLow_{},captureObjectIdGeneration_{},captureObjectIdMaskHash_{},captureRawObjectIdMaskHash_{},captureRawAcceptedExcludingOpacity_{},captureRawAcceptedIncludingOpacity_{},captureRawIdentityOrderHash_{},captureEffectiveAcceptedExcludingOpacity_{},captureEffectiveAcceptedIncludingOpacity_{},captureEffectiveIdentityOrderHash_{},captureReferenceAcceptedExcludingOpacity_{},captureReferenceAcceptedIncludingOpacity_{},captureReferenceIdentityOrderHash_{},captureRequestDrawRevision_{},captureRequestCameraHash_{},captureRequestStateHash_{},captureRawMaximumVisibleStableHigh_{},captureRawMaximumVisibleStableLow_{},captureRawMaximumVisibleGeneration_{},captureEffectiveMaximumVisibleStableHigh_{},captureEffectiveMaximumVisibleStableLow_{},captureEffectiveMaximumVisibleGeneration_{},captureBuilderCandidateLow_{},captureBuilderExpectedCameraHash_{},captureFootRoiDominantHigh_{},captureFootRoiDominantLow_{},captureFootRoiDominantGeneration_{};std::array<std::uint8_t,11> captureWeaponCenterExcluded_{},captureWeaponNearClipClear_{},captureWeaponHudOverlap_{},captureWeaponGripEdgeAnchored_{},captureWeaponBladeAboveGrip_{},captureOcclusionAdjusted_{},captureEffectiveObjectIdSupported_{},captureFeetContactPassed_{},captureObjectIdOpaqueComplete_{},captureObjectIdMaskedComplete_{},captureObjectIdSkinnedComplete_{},captureOpacityMissing_{},captureOpacityGenerationConflict_{},captureCharacterReferenceSupported_{},captureDepthSupported_{},captureGpuFeetContactPassed_{},captureBuilderValid_{},captureBuilderPolicyValid_{},captureBuilderMatrixExact_{},captureBuilderFeetOnScreen_{};std::array<std::string,11> captureFootRoiHistogramJson_{};std::size_t released_{};std::uint64_t trims_{};std::string runId_{},expectedSourceFingerprint_{},actualSourceFingerprint_{AEVRYND_CHARACTER_BUILD_FINGERPRINT};
};
