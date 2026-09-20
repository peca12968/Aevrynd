#include "GameplayLabGame.h"
#include "GameplayGrassCommunityMesh3D.h"
#include "GameplayForestGroundCoverMesh3D.h"
#include "GameplayCandidate7Package3D.h"
#include "GameplayBuildingIdentities3D.h"
#include "GameplayOccluderMask3D.h"
#include "GameplayProductionHdrSidecar3D.h"
#include "ShowcaseTerrainMaterials3D.h"
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/FrameCapture3D.h>
#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>
#include <Aevrynd/MeshLoader.h>
#include <array>
#include <bit>
#include <cctype>
#include <format>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <objbase.h>
#include <ranges>
#include <sstream>
#include <tuple>
#include <unordered_set>
#include <bcrypt.h>

using namespace Aevrynd;using namespace ::GameplayLab;
namespace {constexpr auto Names=[](){std::array<std::wstring_view,GameplayCaptureExpectations3D.size()> result{};for(std::size_t index=0;index<result.size();++index)result[index]=GameplayCaptureExpectations3D[index].name;return result;}();constexpr Color Cyan{.08f,.9f,.8f,1},Gold{1,.68f,.18f,1};std::string RunId(){GUID g{};if(FAILED(CoCreateGuid(&g)))throw std::runtime_error("Gameplay capture GUID unavailable");return std::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",g.Data1,g.Data2,g.Data3,g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);}}
namespace {
Matrix4 GameplayPlayerWorld3D(bool candidate7,bool female,
    const CharacterPresentationScale3D& presentation,Vec3 feet,float yaw){
  Matrix4 world{};
  if(candidate7&&female){
    if(!BuildCharacterPresentationWorld3D(presentation,feet,yaw,world))
      throw std::runtime_error("Candidate7 presentation world contract failed");
    return world;
  }
  return CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
}
Matrix4 GameplayPlayerWorldInverse3D(bool candidate7,bool female,
    const CharacterPresentationScale3D& presentation,Vec3 feet,float yaw){
  Matrix4 inverse{};
  if(candidate7&&female){
    if(!BuildCharacterPresentationWorldInverse3D(presentation,feet,yaw,inverse))
      throw std::runtime_error("Candidate7 presentation inverse contract failed");
    return inverse;
  }
  return CreateRotationMatrix({0,-yaw,0})*CreateTranslationMatrix(feet*-1.f);
}
std::ostream& operator<<(std::ostream& out,const RenderResourceIdentity3D& identity){return out<<"{\"generation\":"<<identity.generation<<",\"width\":"<<identity.width<<",\"height\":"<<identity.height<<",\"format\":"<<identity.format<<",\"sampleCount\":"<<identity.sampleCount<<",\"state\":"<<identity.state<<'}';}
void WriteAtmosphereReadbackProvenanceJson(std::ostream& out,const AtmosphereReadbackProvenance3D& provenance){
  out<<"{\"fingerprint\":"<<provenance.fingerprint
     <<",\"resourceCreates\":"<<provenance.resourceCreates
     <<",\"resourceReleases\":"<<provenance.resourceReleases
     <<",\"outputIdentity\":"<<provenance.outputIdentity
     <<",\"submissionFence\":"<<provenance.submissionFence
     <<",\"historyGeneration\":"<<provenance.historyGeneration
     <<",\"outputWidth\":"<<provenance.outputWidth
     <<",\"outputHeight\":"<<provenance.outputHeight
     <<",\"historyIndex\":"<<provenance.historyIndex
     <<",\"rejection\":"<<unsigned(provenance.rejection)
     <<",\"dispatched\":"<<unsigned(provenance.dispatched)
     <<",\"reused\":"<<unsigned(provenance.reused)
     <<",\"complete\":"<<unsigned(provenance.Complete())<<'}';
}
constexpr std::uint64_t EnvironmentPartitionId=GameplayEnvironmentIdentityDomain3D;
constexpr std::uint64_t GameplayCharacterIdentityHigh=0xaec0474348415201ull;
constexpr std::uint64_t GameplayWorldIdentityHigh=0xaec047574f524c01ull;
constexpr std::uint64_t GameplayPlacedIdentityHigh=0xaec047504c414301ull;
constexpr std::uint64_t GameplayPreviewIdentityHigh=0xaec0475052455601ull;
constexpr std::uint64_t GameplayCameraCandidateHigh=0xaec04743414d4301ull;
constexpr std::uint64_t EnvironmentMirrorUserData=0xe000000000000000ull;
// The shared gabled prism is centered inside the palette's unchanged 1 m box.
// These scalar constants are also consumed by the executable geometry smoke gate.
constexpr float GameplayPaletteBuildingOffsetX3D=0.f;
constexpr float GameplayPaletteBuildingOffsetY3D=-.109f;
constexpr float GameplayPaletteBuildingOffsetZ3D=.02f;
constexpr float GameplayPaletteBuildingScaleX3D=1.f;
constexpr float GameplayPaletteBuildingScaleY3D=.83f;
constexpr float GameplayPaletteBuildingScaleZ3D=1.05f;
constexpr float GameplayPaletteBuildingLocalMinimumX3D=-.33f;
constexpr float GameplayPaletteBuildingLocalMinimumY3D=-.47f;
constexpr float GameplayPaletteBuildingLocalMinimumZ3D=-.31f;
constexpr float GameplayPaletteBuildingLocalMaximumX3D=.33f;
constexpr float GameplayPaletteBuildingLocalMaximumY3D=.44f;
constexpr float GameplayPaletteBuildingLocalMaximumZ3D=.31f;
constexpr std::uint32_t GameplayPaletteBuildingDrawCap3D=1;
constexpr float GameplayPaletteCaptureGhostMinimumSeparation3D=1.5f;
bool GameplayPaletteCaptureGhostsDistinct3D(Vec3 valid,Vec3 invalid){
  const auto finite=[](Vec3 value){return std::isfinite(value.x)&&
      std::isfinite(value.y)&&std::isfinite(value.z);};
  if(!finite(valid)||!finite(invalid))return false;
  const float x=invalid.x-valid.x,z=invalid.z-valid.z;
  return x*x+z*z>=GameplayPaletteCaptureGhostMinimumSeparation3D*
      GameplayPaletteCaptureGhostMinimumSeparation3D;
}
bool GameplayPaletteBuildingTransform3D(Vec3 center,float yaw,Transform3D& result){
  const auto finite=[](Vec3 value){return std::isfinite(value.x)&&
      std::isfinite(value.y)&&std::isfinite(value.z);};
  if(!finite(center)||!std::isfinite(yaw))return false;
  const Vec3 localOffset{GameplayPaletteBuildingOffsetX3D,
      GameplayPaletteBuildingOffsetY3D,GameplayPaletteBuildingOffsetZ3D};
  const Vec3 rotatedOffset=TransformPoint(CreateRotationMatrix({0,yaw,0}),localOffset);
  result={center+rotatedOffset,{0,yaw,0},{GameplayPaletteBuildingScaleX3D,
      GameplayPaletteBuildingScaleY3D,GameplayPaletteBuildingScaleZ3D}};
  if(!finite(result.position)||!finite(result.rotation)||!finite(result.scale))return false;
  // The generated merged house validates these exact local bounds at upload.
  const float sine=std::abs(std::sin(yaw)),cosine=std::abs(std::cos(yaw));
  constexpr float localExtentX=(GameplayPaletteBuildingLocalMaximumX3D-GameplayPaletteBuildingLocalMinimumX3D)*.5f;
  constexpr float localExtentZ=(GameplayPaletteBuildingLocalMaximumZ3D-GameplayPaletteBuildingLocalMinimumZ3D)*.5f;
  const float extentX=cosine*localExtentX*result.scale.x+sine*localExtentZ*result.scale.z;
  const float extentZ=sine*localExtentX*result.scale.x+cosine*localExtentZ*result.scale.z;
  const Vec3 relative=result.position-center;
  constexpr float epsilon=.00001f;
  return std::abs(relative.x)+extentX<=.5f+epsilon&&
      std::abs(relative.z)+extentZ<=.5f+epsilon&&
      relative.y+GameplayPaletteBuildingLocalMinimumY3D*result.scale.y>=-.5f-epsilon&&
      relative.y+GameplayPaletteBuildingLocalMaximumY3D*result.scale.y<=.5f+epsilon;
}
const bool GameplayPaletteBuildingGeometryRuntimeGate3D=[](){
  static_assert(GameplayPaletteBuildingDrawCap3D==1);
  Transform3D zero{},quarter{};
  if(!GameplayPaletteBuildingTransform3D({2,3,4},0,zero)||
     !GameplayPaletteBuildingTransform3D({2,3,4},1.57079632679f,quarter)||
     zero.rotation.y!=0||quarter.rotation.y!=1.57079632679f||
     std::abs(zero.position.x-quarter.position.x)<.01f)
    throw std::runtime_error("Palette building yaw transform gate failed");
  for(unsigned step=0;step<24;++step){Transform3D transform{};
    if(!GameplayPaletteBuildingTransform3D({},float(step)*.2617993878f,transform))
      throw std::runtime_error("Palette building exceeded the placement footprint");
  }
  constexpr float builtWidthX=(GameplayPaletteBuildingLocalMaximumX3D-
      GameplayPaletteBuildingLocalMinimumX3D)*GameplayPaletteBuildingScaleX3D;
  constexpr float builtWidthZ=(GameplayPaletteBuildingLocalMaximumZ3D-
      GameplayPaletteBuildingLocalMinimumZ3D)*GameplayPaletteBuildingScaleZ3D;
  constexpr float builtHeight=(GameplayPaletteBuildingLocalMaximumY3D-
      GameplayPaletteBuildingLocalMinimumY3D)*GameplayPaletteBuildingScaleY3D;
  if(builtWidthX<.65f||builtWidthZ<.65f||builtHeight<.75f)
    throw std::runtime_error("Palette building visual occupancy gate failed");
  Transform3D rejected{};
  if(GameplayPaletteBuildingTransform3D({},std::numeric_limits<float>::infinity(),rejected))
    throw std::runtime_error("Palette building accepted a non-finite yaw");
  if(!GameplayPaletteCaptureGhostsDistinct3D({}, {0,0,1.5f})||
     GameplayPaletteCaptureGhostsDistinct3D({}, {0,0,1.499f})||
     GameplayPaletteCaptureGhostsDistinct3D({},
       {std::numeric_limits<float>::infinity(),0,2.f}))
    throw std::runtime_error("Palette capture ghost separation gate failed");
  return true;
}();
const char* CaptureCameraName(GameplayCaptureCamera3D value){switch(value){case GameplayCaptureCamera3D::First:return "first";case GameplayCaptureCamera3D::Third:return "third";case GameplayCaptureCamera3D::God:return "god";}return "invalid";}
const char* CaptureLightingName(GameplayCaptureLighting3D value){switch(value){case GameplayCaptureLighting3D::Day:return "day";case GameplayCaptureLighting3D::Dusk:return "dusk";case GameplayCaptureLighting3D::Night:return "night";}return "invalid";}
const char* CaptureProjectionName(ProjectionMode value){switch(value){case ProjectionMode::Perspective:return "Perspective";case ProjectionMode::Orthographic:return "Orthographic";}return "Invalid";}
const char* IntegrityModeName(DeterminismIntegrityMode3D value){switch(value){case DeterminismIntegrityMode3D::Runtime:return "Runtime";case DeterminismIntegrityMode3D::SealedCapture:return "SealedCapture";}return "Unknown";}
const char* IntegrityReasonName(DeterminismIntegrityRejectionReason3D value){switch(value){case DeterminismIntegrityRejectionReason3D::None:return "None";case DeterminismIntegrityRejectionReason3D::UnknownMode:return "UnknownMode";case DeterminismIntegrityRejectionReason3D::MissingResidentMaterial:return "MissingResidentMaterial";case DeterminismIntegrityRejectionReason3D::MissingContentIntegrity:return "MissingContentIntegrity";}return "Unknown";}
std::string GameplayProductionHashHex3D(const std::array<std::uint8_t,32>& value){constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(64);for(const std::uint8_t byte:value){result.push_back(digits[byte>>4]);result.push_back(digits[byte&15]);}return result;}
const char* GameplayHdrStateName3D(GameplayHdrCaptureState3D value){switch(value){case GameplayHdrCaptureState3D::Empty:return "Empty";case GameplayHdrCaptureState3D::Armed:return "Armed";case GameplayHdrCaptureState3D::SceneBound:return "SceneBound";case GameplayHdrCaptureState3D::BundleRequested:return "BundleRequested";case GameplayHdrCaptureState3D::AwaitGpu:return "AwaitGpu";case GameplayHdrCaptureState3D::Taken:return "Taken";case GameplayHdrCaptureState3D::Published:return "Published";case GameplayHdrCaptureState3D::Verified:return "Verified";case GameplayHdrCaptureState3D::Failed:return "Failed";}return "Invalid";}
void WriteIntegrityJson(std::ostream& out,std::uint64_t frameSerial,const DeterminismIntegrityReceipt3D& integrity,const CanonicalDrawFingerprintReceipt3D& fingerprint,const FrameBoundRenderProvenance3D* render=nullptr){out<<",\"determinismFrameSerial\":"<<frameSerial<<",\"determinismIntegrityRequested\":\""<<IntegrityModeName(integrity.requestedMode)<<"\",\"determinismIntegrityActual\":\""<<IntegrityModeName(integrity.actualMode)<<"\",\"determinismIntegrityReason\":\""<<IntegrityReasonName(integrity.rejectionReason)<<"\",\"determinismIntegritySampled\":"<<integrity.sampledTexturedMaterials<<",\"determinismIntegrityVerified\":"<<integrity.verifiedTexturedMaterials<<",\"determinismIntegrityRejected\":"<<integrity.rejectedTexturedMaterials<<",\"determinismIntegrityAcceptedFrame\":"<<(integrity.accepted?1:0)<<",\"canonicalDrawFingerprint\":{\"schemaVersion\":"<<fingerprint.schemaVersion<<",\"topology\":"<<fingerprint.topology<<",\"material\":"<<fingerprint.material<<",\"transform\":"<<fingerprint.transform<<",\"palette\":"<<fingerprint.palette<<",\"identity\":"<<fingerprint.identity<<",\"order\":"<<fingerprint.order<<",\"legacyHash\":"<<fingerprint.legacyHash<<'}';if(render)out<<",\"renderProvenance\":{\"frameSerial\":"<<render->frameSerial<<",\"sceneDrawRevision\":"<<render->sceneDrawRevision<<",\"worldRevision\":"<<render->worldRevision<<",\"submittedMeshInstances\":"<<render->submittedMeshInstances<<",\"submittedSkinnedInstances\":"<<render->submittedSkinnedInstances<<",\"submittedTerrainInstances\":"<<render->submittedTerrainInstances<<",\"submittedWorldVertices\":"<<render->submittedWorldVertices<<",\"submittedWorldIndices\":"<<render->submittedWorldIndices<<'}';}
GameplayCaptureCamera3D CaptureCamera(CameraMode value){switch(value){case CameraMode::FirstPerson:return GameplayCaptureCamera3D::First;case CameraMode::ThirdPerson:return GameplayCaptureCamera3D::Third;case CameraMode::GodView:return GameplayCaptureCamera3D::God;}return GameplayCaptureCamera3D::First;}
GameplayCaptureLighting3D CaptureLighting(LightingPhase value){switch(value){case LightingPhase::Day:return GameplayCaptureLighting3D::Day;case LightingPhase::Dusk:return GameplayCaptureLighting3D::Dusk;case LightingPhase::Night:return GameplayCaptureLighting3D::Night;}return GameplayCaptureLighting3D::Day;}
void MixCaptureHash(std::uint64_t& hash,std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}}
// COMPARE_COLLISION_CPU_BEGIN
bool CompareCaptureCollisionClear(const Aevrynd::CollisionWorld3D& world,Aevrynd::Vec3 camera,
    Aevrynd::Vec3 center,Aevrynd::Vec3 extents,bool terrainClear){
  // Controller 是查詢體，不會註冊自身 collider；保留世界障礙與貼地容差檢查。
  return terrainClear&&!world.OverlapsBox(camera,{.1f,.1f,.1f})&&
      !world.OverlapsBox(center,extents-Aevrynd::Vec3{.001f,.001f,.001f});
}
// COMPARE_COLLISION_CPU_END
void MixCaptureFloat(std::uint64_t& hash,float value){MixCaptureHash(hash,std::bit_cast<std::uint32_t>(value));}
std::uint64_t CaptureCameraHash(const Camera3D& camera){std::uint64_t hash=14695981039346656037ull;for(const Vec3 value:{camera.position,camera.target,camera.up}){MixCaptureFloat(hash,value.x);MixCaptureFloat(hash,value.y);MixCaptureFloat(hash,value.z);}MixCaptureHash(hash,unsigned(camera.projection.mode));MixCaptureFloat(hash,camera.projection.verticalFieldOfView);MixCaptureFloat(hash,camera.projection.orthographicHeight);MixCaptureFloat(hash,camera.projection.nearPlane);MixCaptureFloat(hash,camera.projection.farPlane);return hash;}
constexpr std::uint64_t CaptureStateSeed=14695981039346656037ull;
void MixCaptureTag(std::uint64_t& hash,std::string_view tag){MixCaptureHash(hash,tag.size());for(const unsigned char value:tag){hash^=value;hash*=1099511628211ull;}}
std::string CaptureShaHex(const std::array<std::byte,32>& bytes){std::string result;result.reserve(64);for(const auto value:bytes)result+=std::format("{:02x}",std::to_integer<unsigned>(value));return result;}
std::string GameplayUltraDiagnosticJson(const Engine& engine){
  const auto& receipt=engine.DevelopmentUltraReceipt();const auto& gpu=receipt.gpuTruth;const auto& terminal=engine.DevelopmentUltraTerminalState();
  // 只記錄原始狀態，不依診斷結果改變品質或 transition。 (diagnostic only).
  std::ostringstream out;out<<std::boolalpha<<"{\"schemaVersion\":2";
  const auto field=[&](const char* name,auto value){out<<",\""<<name<<"\":"<<value;};
  field("epoch",receipt.epoch);
  field("downgradeCount",receipt.downgradeCount);
  field("requested",unsigned(receipt.requested));
  field("actual",unsigned(receipt.actual));
  field("reason",unsigned(receipt.reason));
  field("phase",unsigned(receipt.phase));
  field("firstFalseReason",unsigned(receipt.firstFalseReason));
  field("requestActive",receipt.requestActive);
  field("noOp",receipt.noOp);
  field("applyAttempted",receipt.applyAttempted);
  field("applySucceeded",receipt.applySucceeded);
  field("postApplyPassed",receipt.postApplyPassed);
  field("rollbackAttempted",receipt.rollbackAttempted);
  field("rollbackSucceeded",receipt.rollbackSucceeded);
  field("rendererApplied",receipt.rendererApplied);
  field("transitionPolicyApplied",receipt.transitionPolicyApplied);
  field("shadowResourcesApplied",receipt.shadowResourcesApplied);
  field("volumetricResolutionApplied",receipt.volumetricResolutionApplied);
  field("cloudSamplesApplied",receipt.cloudSamplesApplied);
  field("cloudShadowSamplesApplied",receipt.cloudShadowSamplesApplied);
  field("lightShaftSamplesApplied",receipt.lightShaftSamplesApplied);
  field("vegetationDensityApplied",receipt.vegetationDensityApplied);
  field("textureBudgetApplied",receipt.textureBudgetApplied);
  field("meshBudgetApplied",receipt.meshBudgetApplied);
  out<<",\"requestedFingerprint\":\""<<receipt.requestedFingerprint<<"\"";
  out<<",\"normalizedFingerprint\":\""<<receipt.normalizedFingerprint<<"\"";
  out<<",\"actualFingerprint\":\""<<receipt.actualFingerprint<<"\"";
  field("gpu_epoch",gpu.epoch);
  field("gpu_observationIndex",gpu.observationIndex);
  field("gpu_submittedFrameSerial",gpu.submittedFrameSerial);
  field("gpu_completedFrameSerial",gpu.completedFrameSerial);
  field("gpu_timingSampleSerial",gpu.timingSampleSerial);
  field("gpu_lastGpuFrameNanoseconds",gpu.lastGpuFrameNanoseconds);
  field("gpu_telemetryAgeNanoseconds",gpu.telemetryAgeNanoseconds);
  field("gpu_dedicatedVideoMemoryBytes",gpu.dedicatedVideoMemoryBytes);
  field("gpu_currentVideoMemoryUsageBytes",gpu.currentVideoMemoryUsageBytes);
  field("gpu_rendererAdapterLuidLow",gpu.rendererAdapterLuidLow);
  field("gpu_rendererAdapterLuidHigh",gpu.rendererAdapterLuidHigh);
  field("gpu_telemetryAdapterLuidLow",gpu.telemetryAdapterLuidLow);
  field("gpu_telemetryAdapterLuidHigh",gpu.telemetryAdapterLuidHigh);
  field("gpu_hardVramBudgetBytes",gpu.hardVramBudgetBytes);
  field("gpu_pendingGpuReleaseBytes",gpu.pendingGpuReleaseBytes);
  field("gpu_debugErrorCount",gpu.debugErrorCount);
  field("gpu_debugWarningCount",gpu.debugWarningCount);
  field("gpu_stableFrames",gpu.stableFrames);
  field("gpu_requiredStableFrames",gpu.requiredStableFrames);
  field("gpu_observed",gpu.observed);
  field("gpu_resourcesTrue",gpu.resourcesTrue);
  field("gpu_telemetryTrue",gpu.telemetryTrue);
  field("gpu_rendererAdapterIdentityValid",gpu.rendererAdapterIdentityValid);
  field("gpu_adapterIdentityTrue",gpu.adapterIdentityTrue);
  field("gpu_validationTrue",gpu.validationTrue);
  field("gpu_timingProvenance",unsigned(gpu.timingProvenance));
  field("gpu_timingRecorded",gpu.timingRecorded);
  field("gpu_timingSerialAligned",gpu.timingSerialAligned);
  field("gpu_productionFrameBudgetApplied",gpu.productionFrameBudgetApplied);
  field("gpu_frameBudgetTrue",gpu.frameBudgetTrue);
  field("gpu_fresh",gpu.fresh);
  field("gpu_framePassed",gpu.framePassed);
  field("gpu_committed",gpu.committed);
  field("terminalOutcome",unsigned(terminal.outcome));field("terminalReason",unsigned(terminal.reason));field("terminalEpoch",terminal.epoch);
  const auto stats=engine.GetRendererStats();
  field("renderer_completedFrameSerial",stats.completedGpuFrameSerial);
  field("renderer_totalNanoseconds",stats.lastGpuFrameNanoseconds);
  field("renderer_scalarSerial",stats.lastCompletedGpuTimingReceipt.serial);
  field("renderer_scalarAlignedValid",stats.completedGpuFrameSerial!=0&&stats.lastCompletedGpuTimingReceipt.serial==stats.completedGpuFrameSerial);
  field("renderer_lastVolumetricAtmosphereGpuNanoseconds",stats.lastVolumetricAtmosphereGpuNanoseconds);
  field("renderer_lastHzbPrepassGpuNanoseconds",stats.lastHzbPrepassGpuNanoseconds);
  field("renderer_lastHzbBuildGpuNanoseconds",stats.lastHzbBuildGpuNanoseconds);
  field("renderer_lastHzbCullGpuNanoseconds",stats.lastHzbCullGpuNanoseconds);
  field("renderer_lastHzbColorGpuNanoseconds",stats.lastHzbColorGpuNanoseconds);
  // 原始 pass validity 與完成幀對齊分開輸出；不將剩餘時間歸因於任何 pass。 (alignment).
  const auto pass=[&](const char* prefix,const RendererStats::CompletedGpuPassTimingReceipt3D& timing){
    field((std::string(prefix)+"_serial").c_str(),timing.serial);
    field((std::string(prefix)+"_beginTick").c_str(),timing.beginTick);
    field((std::string(prefix)+"_endTick").c_str(),timing.endTick);
    field((std::string(prefix)+"_frequency").c_str(),timing.frequency);
    field((std::string(prefix)+"_nanoseconds").c_str(),timing.nanoseconds);
    field((std::string(prefix)+"_recorded").c_str(),timing.recorded);
    field((std::string(prefix)+"_valid").c_str(),timing.valid);
    field((std::string(prefix)+"_alignedValid").c_str(),timing.valid&&timing.serial==stats.completedGpuFrameSerial);
  };
  pass("renderer_shadow",stats.lastShadowRenderingGpuTiming);pass("renderer_captureCopy",stats.lastProductionCaptureCopyGpuTiming);
  out<<'}';return out.str();
}
bool CaptureFileSha256(const std::filesystem::path& path,std::array<std::byte,32>& result){
  BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
  const auto close=[&]{if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);};if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){close();return false;}
  std::ifstream input(path,std::ios::binary);std::array<unsigned char,64*1024> bytes{};while(input){input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());const auto count=input.gcount();if(count>0&&BCryptHashData(hash,bytes.data(),ULONG(count),0)<0){close();return false;}}if(!input.eof()||BCryptFinishHash(hash,reinterpret_cast<unsigned char*>(result.data()),ULONG(result.size()),0)<0){close();return false;}close();return true;
}
}
namespace {
bool EncodeDepthCapture(const std::filesystem::path& path,const DepthCapture3D& capture,PublishedFrameCapture3D& publication){std::vector<std::uint8_t> rgba(capture.pixels.size()*4);for(std::size_t pixel=0;pixel<capture.pixels.size();++pixel){const auto value=std::uint8_t(std::lround(Clamp(1.f-capture.pixels[pixel],0.f,1.f)*255.f));rgba[pixel*4]=rgba[pixel*4+1]=rgba[pixel*4+2]=value;rgba[pixel*4+3]=255;}return EncodeFrameCapturePng3D(path,capture.width,capture.height,std::size_t(capture.width)*4,rgba,publication);}
EnvironmentColliderSemantic3D EnvironmentSemantic(ShowcaseSceneKind3D kind){switch(kind){case ShowcaseSceneKind3D::Terrain:return EnvironmentColliderSemantic3D::Terrain;case ShowcaseSceneKind3D::Water:return EnvironmentColliderSemantic3D::Water;case ShowcaseSceneKind3D::Tree:return EnvironmentColliderSemantic3D::Tree;case ShowcaseSceneKind3D::Grass:return EnvironmentColliderSemantic3D::Grass;case ShowcaseSceneKind3D::Rock:return EnvironmentColliderSemantic3D::Rock;case ShowcaseSceneKind3D::Building:return EnvironmentColliderSemantic3D::Building;}return EnvironmentColliderSemantic3D::Decoration;}
EnvironmentColliderDescriptor3D EnvironmentProxy(const ShowcaseSceneDescriptor3D& source,std::size_t index){EnvironmentColliderDescriptor3D result{};result.id={EnvironmentPartitionId,index+1};result.semantic=EnvironmentSemantic(source.kind);result.yaw=source.yaw;result.layer=1;result.collisionMask=CollisionLayerAll3D;result.userData=EnvironmentMirrorUserData|result.id.low;switch(source.kind){
  case ShowcaseSceneKind3D::Terrain:result.center={source.position.x,-.5f,source.position.z};result.extents={(std::max)(1.f,source.scale.x*.5f),.5f,(std::max)(1.f,source.scale.z*.5f)};break;
  case ShowcaseSceneKind3D::Tree:{const float radius=Clamp(source.scale.y*.11f,.24f,.82f),halfHeight=(std::max)(1.f,source.scale.y*.45f);result.center=source.position+Vec3{0,halfHeight,0};result.extents={radius,halfHeight,radius};break;}
  case ShowcaseSceneKind3D::Rock:result.extents={(std::max)(.35f,source.scale.x*.5f),(std::max)(.3f,source.scale.y*.48f),(std::max)(.35f,source.scale.z*.5f)};result.center=source.position+Vec3{0,result.extents.y,0};break;
  case ShowcaseSceneKind3D::Building:result.center=source.position;result.extents={(std::max)(.1f,source.scale.x*.5f),(std::max)(.1f,source.scale.y*.5f),(std::max)(.1f,source.scale.z*.5f)};break;
  case ShowcaseSceneKind3D::Water:
  case ShowcaseSceneKind3D::Grass:result.center=source.position;result.extents={(std::max)(.05f,source.scale.x*.5f),(std::max)(.05f,source.scale.y*.5f),(std::max)(.05f,source.scale.z*.5f)};break;
  }return result;}
BoxCollider3D RotatedEnvironmentProxy(const EnvironmentColliderDescriptor3D& value){const float sine=std::abs(std::sin(value.yaw)),cosine=std::abs(std::cos(value.yaw));return{value.center,{cosine*value.extents.x+sine*value.extents.z,value.extents.y,sine*value.extents.x+cosine*value.extents.z}};}
bool EnvironmentSolid(EnvironmentColliderSemantic3D semantic){return semantic==EnvironmentColliderSemantic3D::Terrain||semantic==EnvironmentColliderSemantic3D::Tree||semantic==EnvironmentColliderSemantic3D::Rock||semantic==EnvironmentColliderSemantic3D::Building;}
void AppendShowcaseQuad(Mesh3D& mesh,Vec3 a,Vec3 b,Vec3 c,Vec3 d,float uScale=1.f,float vScale=1.f){
  const std::uint32_t base=std::uint32_t(mesh.vertices.size());
  const std::uint32_t normal=PackNormalSnorm8(Normalize(Cross(c-a,b-a)));
  const std::uint32_t color=PackColorRgba8({1,1,1,1});
  mesh.vertices.insert(mesh.vertices.end(),{{a.x,a.y,a.z,normal,color,0,vScale},{b.x,b.y,b.z,normal,color,uScale,vScale},{c.x,c.y,c.z,normal,color,uScale,0},{d.x,d.y,d.z,normal,color,0,0}});
  mesh.indices.insert(mesh.indices.end(),{base,base+2,base+1,base,base+3,base+2});
}
void AppendShowcaseTriangle(Mesh3D& mesh,Vec3 a,Vec3 b,Vec3 c){
  const std::uint32_t base=std::uint32_t(mesh.vertices.size());
  const std::uint32_t normal=PackNormalSnorm8(Normalize(Cross(c-a,b-a)));
  const std::uint32_t color=PackColorRgba8({1,1,1,1});
  mesh.vertices.insert(mesh.vertices.end(),{{a.x,a.y,a.z,normal,color,0,1},{b.x,b.y,b.z,normal,color,1,1},{c.x,c.y,c.z,normal,color,.5f,0}});
  mesh.indices.insert(mesh.indices.end(),{base,base+2,base+1});
}
void AppendShowcaseBox(Mesh3D& mesh,Vec3 minimum,Vec3 maximum){
  const Vec3 a{minimum.x,minimum.y,minimum.z},b{maximum.x,minimum.y,minimum.z},c{maximum.x,maximum.y,minimum.z},d{minimum.x,maximum.y,minimum.z};
  const Vec3 e{minimum.x,minimum.y,maximum.z},f{maximum.x,minimum.y,maximum.z},g{maximum.x,maximum.y,maximum.z},h{minimum.x,maximum.y,maximum.z};
  AppendShowcaseQuad(mesh,a,b,c,d);AppendShowcaseQuad(mesh,f,e,h,g);AppendShowcaseQuad(mesh,e,a,d,h);AppendShowcaseQuad(mesh,b,f,g,c);AppendShowcaseQuad(mesh,d,c,g,h);AppendShowcaseQuad(mesh,e,f,b,a);
}
void CarveShowcaseRiverShore3D(HeightField3D& terrain){
  // 碰撞保持單一高度場；以原三角面重採樣，遠區不重新生成噪聲或改變幾何。
  if(terrain.Settings().cellSize==4.f){
    const auto coarse=terrain;const auto s=coarse.Settings();
    terrain.Configure({s.origin,(s.width-1)*4+1,(s.depth-1)*4+1,1.f});
    for(unsigned z=0;z<terrain.Settings().depth;++z)for(unsigned x=0;x<terrain.Settings().width;++x){
      const unsigned ix=(std::min)(x/4,s.width-2),iz=(std::min)(z/4,s.depth-2);
      const float u=float(x-ix*4)/4.f,v=float(z-iz*4)/4.f;
      const float a=coarse.Height(ix,iz),b=coarse.Height(ix+1,iz),c=coarse.Height(ix,iz+1),d=coarse.Height(ix+1,iz+1);
      terrain.SetHeight(x,z,u+v<=1?a+(b-a)*u+(c-a)*v:d+(c-d)*(1-u)+(b-d)*(1-v));
    }
  }
  // 岸脊至少一個地形 cell 寬，讓天然低窪不會穿過河道局部邊界形成截斷水面。
  const auto settings=terrain.Settings();
  for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x){
    const Vec3 p=terrain.SamplePosition(x,z);const float center=-4.f+std::sin(p.z*.035f)*5.f+std::sin(p.z*.011f)*2.f;
    const float width=12.f+std::sin(p.z*.057f+.7f)*2.6f;
    const float inward=(std::min)(width*.5f+7.f-std::abs(p.x-center),136.f-std::abs(p.z));
    if(inward<=-4.f)continue;
    const float bedBlend=Clamp((inward-4.f)/4.f,0.f,1.f),bedWeight=bedBlend*bedBlend*(3.f-2.f*bedBlend);
    const float target=.15f+(-2.4f-.15f)*bedWeight;
    const float outerBlend=Clamp((inward+4.f)/4.f,0.f,1.f),weight=outerBlend*outerBlend*(3.f-2.f*outerBlend);
    terrain.SetHeight(x,z,terrain.Height(x,z)*(1.f-weight)+target*weight);
  }
  // 保留既有出生與屋前平整區，不增加玩家碰撞或改變通行分類。
  terrain.FlattenCircle({-16,0,8},8,0,8);
  terrain.FlattenCircle({-12,0,24},7.2f,0,4.5f);
  terrain.RecalculateRange();
}
Mesh3D CreateShowcaseRiverRefinedTerrainMesh3D(const HeightField3D& terrain,HeightFieldMeshSettings3D settings={}){
  // 只有河道包圍區使用一米面；交界的粗三角形補共享邊點，禁止 T 接縫。
  if(terrain.Settings().cellSize!=1.f)throw std::runtime_error("Refined terrain requires 1m samples");
  const auto dense=terrain.CreateMesh(settings);const unsigned width=terrain.Settings().width;
  Mesh3D result;std::unordered_map<unsigned,unsigned> shared;
  const auto vertex=[&](unsigned x,unsigned z){const unsigned key=z*width+x;auto found=shared.find(key);if(found!=shared.end())return found->second;const unsigned id=unsigned(result.vertices.size());result.vertices.push_back(dense.vertices[key]);shared.emplace(key,id);return id;};
  const auto fine=[&](int x,int z){const float wx=terrain.Settings().origin.x+float(x),wz=terrain.Settings().origin.z+float(z);return wx>=-40&&wx<40&&wz>=-148&&wz<148;};
  const auto triangle=[&](unsigned a,unsigned b,unsigned c){result.indices.insert(result.indices.end(),{a,b,c});};
  for(unsigned z=0;z+4<terrain.Settings().depth;z+=4)for(unsigned x=0;x+4<width;x+=4){
    if(fine(int(x),int(z))){for(unsigned j=0;j<4;++j)for(unsigned i=0;i<4;++i){const auto a=vertex(x+i,z+j),b=vertex(x+i+1,z+j),c=vertex(x+i,z+j+1),d=vertex(x+i+1,z+j+1);triangle(a,c,b);triangle(b,c,d);}continue;}
    const bool left=fine(int(x)-4,int(z)),right=fine(int(x)+4,int(z)),top=fine(int(x),int(z)-4),bottom=fine(int(x),int(z)+4);
    if(!left&&!right&&!top&&!bottom){const auto a=vertex(x,z),b=vertex(x+4,z),c=vertex(x,z+4),d=vertex(x+4,z+4);triangle(a,c,b);triangle(b,c,d);continue;}
    const auto fan=[&](std::vector<unsigned> ring){
      auto center=result.vertices[ring[0]];center.x=center.y=center.z=0;center.u=center.v=0;
      for(unsigned id:ring){const auto&v=result.vertices[id];center.x+=v.x/float(ring.size());center.y+=v.y/float(ring.size());center.z+=v.z/float(ring.size());center.u+=v.u/float(ring.size());center.v+=v.v/float(ring.size());}
      center.normal=PackNormalSnorm8(terrain.SampleNormal(center.x,center.z));
      const unsigned mid=unsigned(result.vertices.size());result.vertices.push_back(center);for(std::size_t i=0;i<ring.size();++i)triangle(mid,ring[i],ring[(i+1)%ring.size()]);
    };
    std::vector<unsigned> first{vertex(x,z)};if(left)for(unsigned j=1;j<4;++j)first.push_back(vertex(x,z+j));first.push_back(vertex(x,z+4));first.push_back(vertex(x+4,z));if(top)for(unsigned j=1;j<4;++j)first.push_back(vertex(x+4-j,z));fan(first);
    std::vector<unsigned> second{vertex(x+4,z),vertex(x,z+4)};if(bottom)for(unsigned j=1;j<4;++j)second.push_back(vertex(x+j,z+4));second.push_back(vertex(x+4,z+4));if(right)for(unsigned j=1;j<4;++j)second.push_back(vertex(x+4,z+4-j));fan(second);
  }
  return result;
}
float RiverWaterDisplacementWeight3D(float boundaryDistance){
  constexpr float fadeDistance=.20f;const float t=Clamp(boundaryDistance/fadeDistance,0.f,1.f);return t*t*(3.f-2.f*t);
}
Mesh3D CreateRiverHeroCellMesh3D(const HeightField3D& terrain){
  // 水面取自實際地形三角形的等高線；共享索引避免獨立透明 cell 重疊。
  Mesh3D mesh;const auto ground=terrain.CreateMesh();constexpr float waterY=-.42f;
  std::unordered_map<std::uint64_t,std::uint32_t> shared;
  const auto add=[&](Vec3 p){
    const auto qx=std::int32_t(std::llround(double(p.x)*10000.0)),qz=std::int32_t(std::llround(double(p.z)*10000.0));
    const std::uint64_t key=(std::uint64_t(std::uint32_t(qx))<<32)|std::uint32_t(qz);
    const auto found=shared.find(key);if(found!=shared.end())return found->second;
    const auto index=std::uint32_t(mesh.vertices.size());shared.emplace(key,index);
    mesh.vertices.push_back({p.x/30.f,0,p.z/272.f,PackNormalSnorm8({0,1,0}),PackColorRgba8({1,1,1,1}),p.x/30.f,(p.z/272.f+.5f)*8.f});return index;
  };
  for(std::size_t i=0;i<ground.indices.size();i+=3){
    std::vector<Vec3> input;for(unsigned j=0;j<3;++j){const auto&v=ground.vertices[ground.indices[i+j]];input.push_back({v.x,v.y,v.z});}
    const Vec3 center=(input[0]+input[1]+input[2])*(1.f/3.f);
    const float riverCenter=-4.f+std::sin(center.z*.035f)*5.f+std::sin(center.z*.011f)*2.f;
    const float width=12.f+std::sin(center.z*.057f+.7f)*2.6f;
    if(std::abs(center.z)>134.f||std::abs(center.x-riverCenter)>width*.5f+5.f)continue;
    std::vector<Vec3> wet;
    for(unsigned j=0;j<3;++j){const Vec3 a=input[j],b=input[(j+1)%3];const bool inA=a.y<=waterY,inB=b.y<=waterY;if(inA)wet.push_back(a);if(inA!=inB){const float t=(waterY-a.y)/(b.y-a.y);wet.push_back(a+(b-a)*t);}}
    for(std::size_t j=1;j+1<wet.size();++j){const auto a=add(wet[0]),b=add(wet[j]),c=add(wet[j+1]);const auto&pa=mesh.vertices[a];const auto&pb=mesh.vertices[b];const auto&pc=mesh.vertices[c];const float area=(pb.x-pa.x)*(pc.z-pa.z)-(pb.z-pa.z)*(pc.x-pa.x);if(a!=b&&b!=c&&a!=c&&std::abs(area)>1e-12f)mesh.indices.insert(mesh.indices.end(),{a,b,c});}
  }
  if(mesh.indices.empty())throw std::runtime_error("River terrain contour is empty");
  // normal.w 對 water 材質不參與 foliage wind；改承載岸界位移權重，避免擴充
  // 共用頂點格式。以實際最終拓樸的所有開放邊為岸界，包含河道兩端。
  std::unordered_map<std::uint64_t,unsigned> edgeUse;
  const auto edgeKey=[](std::uint32_t a,std::uint32_t b){if(a>b)std::swap(a,b);return(std::uint64_t(a)<<32)|b;};
  for(std::size_t i=0;i<mesh.indices.size();i+=3)for(unsigned j=0;j<3;++j)++edgeUse[edgeKey(mesh.indices[i+j],mesh.indices[i+(j+1)%3])];
  std::vector<std::pair<Vec2,Vec2>> boundarySegments;
  for(const auto&[key,count]:edgeUse)if(count==1){
    const auto& a=mesh.vertices[std::uint32_t(key>>32)];const auto& b=mesh.vertices[std::uint32_t(key)];
    boundarySegments.push_back({{a.x*30.f,a.z*272.f},{b.x*30.f,b.z*272.f}});
  }
  for(auto& vertex:mesh.vertices){
    const Vec2 point{vertex.x*30.f,vertex.z*272.f};float minimumDistanceSquared=std::numeric_limits<float>::max();
    for(const auto&[a,b]:boundarySegments){const Vec2 edge=b-a,toPoint=point-a;const float lengthSquared=edge.x*edge.x+edge.y*edge.y;const float projection=toPoint.x*edge.x+toPoint.y*edge.y;const float t=lengthSquared>.0000001f?Clamp(projection/lengthSquared,0.f,1.f):0.f;const Vec2 delta=toPoint-edge*t;minimumDistanceSquared=(std::min)(minimumDistanceSquared,delta.x*delta.x+delta.y*delta.y);}
    const float weight=RiverWaterDisplacementWeight3D(std::sqrt(minimumDistanceSquared));
    vertex.normal=PackNormalSnorm8({0,1,0},weight);
  }
  return mesh;
}
Mesh3D CreateCottageFacadeMesh3D(bool door,bool pairedWindows){
  Mesh3D mesh;constexpr float front=-.5f,back=.5f;
  const auto box=[&](float x0,float y0,float x1,float y1){AppendShowcaseBox(mesh,{x0,y0,front},{x1,y1,back});};
  if(door){box(-.5f,-.5f,-.12f,-.18f);box(.12f,-.5f,.5f,-.18f);}else box(-.5f,-.5f,.5f,-.18f);
  if(pairedWindows){box(-.5f,-.18f,-.34f,.18f);box(-.16f,-.18f,door?-.12f:.16f,.18f);box(door?.12f:.16f,-.18f,.34f,.18f);box(.34f,-.18f,.5f,.18f);}
  else{box(-.5f,-.18f,-.18f,.18f);box(.18f,-.18f,.5f,.18f);}
  box(-.5f,.18f,.5f,.5f);
  if(mesh.indices.size()/3!=GameplayCottageFacadeExpectedTriangles3D(door,pairedWindows))throw std::runtime_error("Cottage facade duplicate topology contract failed");
  return mesh;
}
Mesh3D CreateCottageRoofMesh3D(){
  Mesh3D mesh;const Vec3 lf{- .5f,-.22f,-.5f},lb{-.5f,-.22f,.5f},rf{.5f,-.22f,-.5f},rb{.5f,-.22f,.5f},ridgeF{0,.5f,-.5f},ridgeB{0,.5f,.5f};
  AppendShowcaseQuad(mesh,lf,ridgeF,ridgeB,lb,3.f,2.f);AppendShowcaseQuad(mesh,ridgeF,rf,rb,ridgeB,3.f,2.f);AppendShowcaseTriangle(mesh,lf,rf,ridgeF);AppendShowcaseTriangle(mesh,rb,lb,ridgeB);AppendShowcaseQuad(mesh,lb,rb,rf,lf);
  return mesh;
}
Mesh3D CreateGameplayPaletteBuildingMesh3D(){
  Mesh3D mesh;
  const auto shade=[&](std::size_t first,Color color){
    const auto packed=PackColorRgba8(color);
    for(std::size_t index=first;index<mesh.vertices.size();++index)
      mesh.vertices[index].color=packed;
  };
  const auto box=[&](Vec3 minimum,Vec3 maximum,Color color){
    const auto first=mesh.vertices.size();AppendShowcaseBox(mesh,minimum,maximum);shade(first,color);
  };
  // Wall shell, door, two recessed-looking windows and chimney are merged with
  // the roof into one immutable GPU mesh. Grayscale vertex shades retain the
  // green/red preview tint while keeping facade features legible.
  box({-.27f,-.47f,-.27f},{.27f,.03f,.27f},{.58f,.34f,.18f,1});
  box({-.075f,-.46f,-.285f},{.075f,-.13f,-.27f},{.16f,.09f,.045f,1});
  box({-.225f,-.27f,-.287f},{-.12f,-.09f,-.27f},{.035f,.03f,.025f,1});
  box({.12f,-.27f,-.287f},{.225f,-.09f,-.27f},{.035f,.03f,.025f,1});
  const Vec3 lf{-.33f,0,-.31f},lb{-.33f,0,.31f},rf{.33f,0,-.31f},rb{.33f,0,.31f};
  const Vec3 ridgeF{0,.36f,-.31f},ridgeB{0,.36f,.31f};
  const auto roofFirst=mesh.vertices.size();
  AppendShowcaseQuad(mesh,lf,ridgeF,ridgeB,lb);AppendShowcaseQuad(mesh,ridgeF,rf,rb,ridgeB);
  AppendShowcaseTriangle(mesh,lf,rf,ridgeF);AppendShowcaseTriangle(mesh,rb,lb,ridgeB);
  AppendShowcaseQuad(mesh,lb,rb,rf,lf);shade(roofFirst,{.22f,.20f,.18f,1});
  box({-.22f,.23f,.08f},{-.14f,.44f,.18f},{.25f,.14f,.08f,1});
  const auto bounds=mesh.Bounds();
  const auto approximately=[](float a,float b){return std::abs(a-b)<=.00001f;};
  if(!mesh.Valid()||mesh.vertices.size()!=138||mesh.indices.size()!=204||
     mesh.indices.size()/3!=68||
     !approximately(bounds.center.x,0)||!approximately(bounds.center.y,-.015f)||!approximately(bounds.center.z,0)||
     !approximately(bounds.extents.x,.33f)||!approximately(bounds.extents.y,.455f)||!approximately(bounds.extents.z,.31f))
    throw std::runtime_error("Palette building merged mesh bounds/topology gate failed");
  return mesh;
}
struct GameplayCottageMaterialResponse3D {
  float roughness{}, normalScale{}, textureScale{};
  bool useUv{};
};
constexpr float GameplayCottageWoodRoughness3D=.68f;
constexpr float GameplayCottageWoodNormalScale3D=.62f;
constexpr float GameplayCottageWoodTextureScale3D=.20f;
constexpr float GameplayCottageRoofRoughness3D=.78f;
constexpr float GameplayCottageRoofNormalScale3D=.72f;
constexpr float GameplayCottageRoofTextureScale3D=.26f;
constexpr Color GameplayCottageRoofTint3D{.54f,.31f,.19f,1.f};
constexpr bool GameplayCottageRoofTintLuminanceGate3D(Color value) noexcept {
  const float luminance=value.r*.2126f+value.g*.7152f+value.b*.0722f;
  return luminance>=.32f&&luminance<=.38f;
}
constexpr bool GameplayCottageRoofTintChromaGate3D(Color value) noexcept {
  return value.r>=value.g*1.55f&&value.g>=value.b*1.45f&&value.a==1.f;
}
constexpr float GameplayCottageBrickRoughness3D=.82f;
constexpr float GameplayCottageBrickNormalScale3D=.72f;
constexpr float GameplayCottageBrickTextureScale3D=.26f;
constexpr GameplayCottageMaterialResponse3D GameplayCottageMaterialResponse(
    GameplayCottageMaterialRole3D role) noexcept {
  if(role==GameplayCottageMaterialRole3D::RoofTile)
    return {GameplayCottageRoofRoughness3D,GameplayCottageRoofNormalScale3D,
            GameplayCottageRoofTextureScale3D,true};
  if(role==GameplayCottageMaterialRole3D::Brick)
    return {GameplayCottageBrickRoughness3D,GameplayCottageBrickNormalScale3D,
            GameplayCottageBrickTextureScale3D,false};
  return {GameplayCottageWoodRoughness3D,GameplayCottageWoodNormalScale3D,
          GameplayCottageWoodTextureScale3D,false};
}
bool ApplyGameplayCottageMaterialResponse3D(
    Material3D& material,GameplayCottageMaterialRole3D role) noexcept {
  const auto response=GameplayCottageMaterialResponse(role);
  const bool expectedUseUv=GameplayCottageMaterialUsesUv3D(
      role==GameplayCottageMaterialRole3D::RoofTile);
  const auto in=[](float value,float minimum,float maximum){
    return value>=minimum&&value<=maximum;
  };
  const bool roleResponseInRange=
      role==GameplayCottageMaterialRole3D::RoofTile
          ? in(response.roughness,.70f,.88f)&&
            in(response.normalScale,.55f,.80f)&&
            in(response.textureScale,.20f,.32f)
      : role==GameplayCottageMaterialRole3D::Brick
          ? in(response.roughness,.72f,.90f)&&
            in(response.normalScale,.55f,.80f)&&
            in(response.textureScale,.20f,.32f)
          : in(response.roughness,.60f,.76f)&&
            in(response.normalScale,.50f,.70f)&&
            in(response.textureScale,.16f,.26f);
  // 這些界限是正式小屋的材質契約：粗糙度禁止退回塑膠高光，normal 與
  // 貼圖頻率禁止在 God view 形成高頻閃爍；非有限值與錯誤 UV 路徑一律拒絕。
  if(!std::isfinite(response.roughness)||!std::isfinite(response.normalScale)||
     !std::isfinite(response.textureScale)||response.roughness<.55f||
     response.roughness>.92f||response.normalScale<.45f||
     response.normalScale>.82f||response.textureScale<.12f||
     response.textureScale>.34f||!roleResponseInRange||
     response.useUv!=expectedUseUv)
    return false;
  const auto tint=role==GameplayCottageMaterialRole3D::RoofTile?GameplayCottageRoofTint3D:material.tint;
  if(!std::isfinite(material.tint.r)||!std::isfinite(material.tint.g)||
     !std::isfinite(material.tint.b)||!std::isfinite(material.tint.a)||
     (role==GameplayCottageMaterialRole3D::RoofTile&&
      (!GameplayCottageRoofTintLuminanceGate3D(tint)||
       !GameplayCottageRoofTintChromaGate3D(tint))))
    return false;
  material.surface.roughness=response.roughness;
  material.normalScale=response.normalScale;
  material.textureScale=response.textureScale;
  material.useUv=response.useUv;
  material.tint=tint;
  return true;
}

struct GameplayCottageWindowResponse3D {
  Color tint{};
  float roughness{},metallic{},emissive{},opacity{};
};

constexpr GameplayCottageWindowResponse3D GameplayCottageWindowResponse(
    LightingPhase phase) noexcept {
  switch(phase){
  case LightingPhase::Day:
  case LightingPhase::Dusk:
  case LightingPhase::Night:
    return {{.055f,.050f,.042f,1.f},.12f,.04f,.012f,1.f};
  }
  return {};
}

bool ApplyGameplayCottageWindowResponse3D(
    Material3D& material,LightingPhase phase) noexcept {
  const auto response=GameplayCottageWindowResponse(phase);
  const bool finite=std::isfinite(response.tint.r)&&
      std::isfinite(response.tint.g)&&std::isfinite(response.tint.b)&&
      std::isfinite(response.tint.a)&&std::isfinite(response.roughness)&&
      std::isfinite(response.metallic)&&std::isfinite(response.emissive)&&
      std::isfinite(response.opacity);
  if(!finite||response.tint.r<0.f||response.tint.r>.35f||
     response.tint.g<0.f||response.tint.g>.18f||
     response.tint.b<0.f||response.tint.b>.08f||response.tint.a!=1.f||
     response.roughness<.08f||response.roughness>.20f||
     response.metallic<0.f||response.metallic>.08f||
     !(response.emissive>=0.f&&response.emissive<=.02f)||
     response.opacity!=1.f)
    return false;
  material.textures={};
  material.tint={response.tint.r,response.tint.g,response.tint.b,response.opacity};
  material.surface.roughness=response.roughness;
  material.surface.metallic=response.metallic;
  material.surface.emissive=response.emissive;
  material.surface.occlusion=1.f;
  material.textureScale=1.f;
  material.normalScale=0.f;
  material.useUv=false;
  material.alphaMode=MaterialAlphaMode3D::Opaque;
  material.doubleSided=true;
  material.castsShadow=false;
  return true;
}

constexpr Color GameplayCottageChimneyStoneTint3D{.38f,.32f,.26f,1.f};
constexpr bool GameplayCottageChimneyStoneLuminanceGate3D(
    Color value) noexcept {
  const float luminance=value.r*.2126f+value.g*.7152f+value.b*.0722f;
  return luminance>=.30f&&luminance<=.36f&&value.a==1.f;
}
constexpr bool GameplayCottageChimneyStoneChromaGate3D(
    Color value) noexcept {
  return value.r>=value.g*1.12f&&value.g>=value.b*1.15f;
}
bool ApplyGameplayCottageChimneyStoneResponse3D(
    Material3D& material) noexcept {
  constexpr auto tint=GameplayCottageChimneyStoneTint3D;
  if(!material.textures||!std::isfinite(tint.r)||!std::isfinite(tint.g)||
     !std::isfinite(tint.b)||!std::isfinite(tint.a)||
     !GameplayCottageChimneyStoneLuminanceGate3D(tint)||
     !GameplayCottageChimneyStoneChromaGate3D(tint))
    return false;
  material.tint=tint;
  material.surface.roughness=.86f;
  material.surface.metallic=.015f;
  material.surface.emissive=0.f;
  material.surface.occlusion=1.f;
  material.normalScale=.76f;
  material.textureScale=.22f;
  material.useUv=false;
  material.alphaMode=MaterialAlphaMode3D::Opaque;
  material.doubleSided=false;
  material.castsShadow=true;
  return true;
}
}
std::string CreateGameplayLabRunId(){return RunId();}
namespace {std::uint64_t HashPose(const AnimationPose3D& pose){return HashGameplayCapturePose3D(pose);}std::uint64_t HashFootPlacement(const FootPlacementResult3D& value){return HashGameplayCaptureFootPlacement3D(value);}}
namespace {
bool GameplayGodPoseAuthorityPass3D(bool activeRoute,bool warmup,bool pairFrozen,bool ready,bool poseFrozen,bool receiptValid,bool layoutValid,std::uint64_t subjectHash,std::uint64_t activeHash) noexcept {
  // 非正式 prelude 可等待有效構圖；正式 capture 不接受缺失權威，有效權威一律精確對應姿勢。
  const bool formal=activeRoute||warmup||pairFrozen||ready||poseFrozen;
  return receiptValid?(layoutValid&&subjectHash&&subjectHash==activeHash):!formal;
}
}
namespace {Matrix4 InverseAffineViewmodelSocket(const Matrix4& source){const double a=source[0][0],b=source[0][1],c=source[0][2],d=source[1][0],e=source[1][1],f=source[1][2],g=source[2][0],h=source[2][1],i=source[2][2],det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1e-12)throw std::runtime_error("Viewmodel grip socket is singular");Matrix4 inverse=Matrix4::Identity();inverse[0][0]=float((e*i-f*h)/det);inverse[0][1]=float((c*h-b*i)/det);inverse[0][2]=float((b*f-c*e)/det);inverse[1][0]=float((f*g-d*i)/det);inverse[1][1]=float((a*i-c*g)/det);inverse[1][2]=float((c*d-a*f)/det);inverse[2][0]=float((d*h-e*g)/det);inverse[2][1]=float((b*g-a*h)/det);inverse[2][2]=float((a*e-b*d)/det);const float tx=source[0][3],ty=source[1][3],tz=source[2][3];inverse[0][3]=-(inverse[0][0]*tx+inverse[0][1]*ty+inverse[0][2]*tz);inverse[1][3]=-(inverse[1][0]*tx+inverse[1][1]*ty+inverse[1][2]*tz);inverse[2][3]=-(inverse[2][0]*tx+inverse[2][1]*ty+inverse[2][2]*tz);return inverse;}}
namespace {void ApplyCombatFinishInput(Input& input,std::uint64_t tick){if(tick==132||tick==166||(tick>=200&&tick<=440&&tick%40==0)){input.SetMouseButton(true);input.SetMouseButton(false);}}}
namespace {
void ApplyReplayInput(Input &input, std::uint64_t tick) {
  const auto tap = [&](WPARAM key) {
    input.SetKey(unsigned(key), true);
    input.SetKey(unsigned(key), false);
  };
  switch (tick) {
  case 2:
    tap('1');
    break;
  case 4:
    tap('2');
    break;
  case 10:
    input.SetKey('W', true);
    break;
  case 12:
    input.SetKey(VK_SHIFT, true);
    break;
  case 14:
    tap(VK_SPACE);
    break;
  case 16:
    tap('C');
    break;
  case 18:
    input.SetKey(VK_SHIFT, false);
    tap('C');
    break;
  case 20:
    input.SetKey('W', false);
    tap('3');
    break;
  case 21:
    tap('C');
    break;
  case 22:
    tap(VK_TAB);
    break;
  case 24:
    input.SetMouse({80,515},true);
    input.SetMouseButton(true);
    break;
  case 25:
    input.SetMouse({700,390},true);
    break;
  case 26:
    tap('R');
    input.SetMouseButton(false);
    break;
  case 27:
    // palette 指標回放已保留 tick 24 的 God authority；後續 transition 不再重跑昂貴全域搜尋。
    tap('1');
    break;
  case 28:
    input.SetRightMouseButton(true);
    input.SetRightMouseButton(false);
    break;
  case 30:
    tap(VK_DELETE);
    break;
  case 32:
    input.SetKey(VK_CONTROL, true);
    tap('Z');
    input.SetKey(VK_CONTROL, false);
    break;
  case 34:
    input.SetKey(VK_CONTROL, true);
    tap('Y');
    input.SetKey(VK_CONTROL, false);
    break;
  case 36:
    tap(VK_F2);
    break;
  case 38:
    tap(VK_F3);
    break;
  case 40:
    tap(VK_F5);
    break;
  case 42:
    tap(VK_F6);
    break;
  case 44:
    tap(VK_F7);
    break;
  case 46:
    tap('T');
    break;
  case 48:
    tap('4');
    break;
  case 50:
    tap('5');
    break;
  case 52:
    tap('6');
    break;
  case 54:
    tap('7');
    break;
  case 56:
    tap('8');
    break;
  case 58:
    tap('9');
    break;
  case 59:
    tap(VK_TAB);
    break;
  case 60:
  case 78:
  case 96:
  case 114:
    input.SetMouseButton(true);
    input.SetMouseButton(false);
    break;
  case 260:
    input.SetKey('G', true);
    break;
  case 268:
    input.SetKey('G', false);
    break;
  default:
    break;
  }
}
} // namespace
ReusableMemoryStats GameplayLabGame::GetReusableMemoryStats()const{const auto c=collisions_.MemoryStats(),p=physics_.MemoryStats(),s=scene_.MemoryStats(),k=skinning_.MemoryStats(),fa=femaleAnimation_.MemoryStats(),ma=maleAnimation_.MemoryStats(),mo=monsterAnimation_.MemoryStats(),ff=femaleFootPlacement_.MemoryStats(),mf=maleFootPlacement_.MemoryStats(),ph=playerHistory_.MemoryStats(),mh=monsterHistory_.MemoryStats();const std::size_t environmentBytes=(environmentCollisions_?environmentCollisions_->Receipt().retainedBytes:0)+environmentCollisionDescriptors_.capacity()*sizeof(EnvironmentColliderDescriptor3D)+environmentGameplayColliders_.capacity()*sizeof(ColliderHandle3D);return{c.retainedCapacityBytes+p.retainedCapacityBytes+s.retainedCapacityBytes+k.retainedCapacityBytes+fa.retainedCapacityBytes+ma.retainedCapacityBytes+mo.retainedCapacityBytes+ff.retainedCapacityBytes+mf.retainedCapacityBytes+ph.retainedCapacityBytes+mh.retainedCapacityBytes+environmentBytes,c.reclaimableCapacityBytes+p.reclaimableCapacityBytes+s.reclaimableCapacityBytes+k.reclaimableCapacityBytes+fa.reclaimableCapacityBytes+ma.reclaimableCapacityBytes+mo.reclaimableCapacityBytes+ff.reclaimableCapacityBytes+mf.reclaimableCapacityBytes+ph.reclaimableCapacityBytes+mh.reclaimableCapacityBytes,released_,trims_};}
std::size_t GameplayLabGame::TrimReusableMemory(){const auto before=GetReusableMemoryStats().retainedCapacityBytes;collisions_.TrimUnusedMemory();physics_.TrimUnusedMemory();scene_.TrimUnusedMemory();skinning_.TrimUnusedMemory();femaleAnimation_.TrimUnusedMemory();maleAnimation_.TrimUnusedMemory();monsterAnimation_.TrimUnusedMemory();femaleFootPlacement_.TrimUnusedMemory();maleFootPlacement_.TrimUnusedMemory();playerHistory_.TrimUnusedMemory();monsterHistory_.TrimUnusedMemory();const auto after=GetReusableMemoryStats().retainedCapacityBytes;released_=before>after?before-after:0;++trims_;return released_;}
void GameplayLabGame::ReleaseEnvironmentGameplayColliders(){for(const auto handle:environmentGameplayColliders_)collisions_.Destroy(handle);environmentGameplayColliders_.clear();}
bool GameplayLabGame::SyncEnvironmentCollision(Vec3 observer){if(!environmentCollisions_||!environmentCollisions_->UpdateObserver(observer))return false;const bool active=environmentCollisions_->IsPartitionActive(EnvironmentPartitionId);if(!active){ReleaseEnvironmentGameplayColliders();return true;}if(!environmentGameplayColliders_.empty())return true;std::vector<ColliderHandle3D> created;try{created.reserve(environmentCollisionActiveReceipt_.solidDescriptors?environmentCollisionActiveReceipt_.solidDescriptors:91);for(const auto& descriptor:environmentCollisionDescriptors_){if(!EnvironmentSolid(descriptor.semantic))continue;const auto handle=collisions_.CreateBox({RotatedEnvironmentProxy(descriptor),descriptor.layer,descriptor.collisionMask,descriptor.userData,false});if(!handle)throw std::runtime_error("Gameplay environment mirror collider creation failed");created.push_back(handle);}}catch(...){for(const auto handle:created)collisions_.Destroy(handle);return false;}environmentGameplayColliders_=std::move(created);return true;}
bool GameplayLabGame::InitializeEnvironmentCollision(){const auto world=showcaseEnvironment_.Receipt();const std::size_t total=world.terrain+world.water+world.trees+world.grass+world.rocks+world.buildings,solid=world.terrain+world.trees+world.rocks+world.buildings,nonSolid=world.water+world.grass;if(total!=showcaseEnvironment_.Descriptors().size()||total>world.descriptorCapacity||solid+nonSolid!=total)return false;environmentCollisionDescriptors_.clear();environmentCollisionDescriptors_.reserve(total);std::size_t index{};for(const auto& descriptor:showcaseEnvironment_.Descriptors())environmentCollisionDescriptors_.push_back(EnvironmentProxy(descriptor,index++));environmentCollisions_=std::make_unique<EnvironmentCollisionStreaming3D>(physics_,EnvironmentCollisionStreamingSettings3D{2,world.descriptorCapacity,solid,1.2f,{.78f,.02f}});if(!environmentCollisions_->RegisterPartition({EnvironmentPartitionId,{0,0,0},320},environmentCollisionDescriptors_))return false;const auto registered=environmentCollisions_->Receipt();if(registered.registeredDescriptors!=total||registered.solidDescriptors!=solid||registered.nonSolidDescriptors!=nonSolid)return false;if(!environmentCollisions_->UpdateObserver(controller_.Position()))return false;const auto firstActive=environmentCollisions_->Receipt();const auto terrainHit=environmentCollisions_->Raycast(MakeRay3D({0,4,0},{0,-1,0}),12);environmentRayEvidence_=terrainHit&&terrainHit.object.id==StableObjectId3D{EnvironmentPartitionId,1};const auto tree=std::ranges::find_if(environmentCollisionDescriptors_,[](const auto& value){return value.semantic==EnvironmentColliderSemantic3D::Tree;});if(tree==environmentCollisionDescriptors_.end())return false;const auto treeObjects=environmentCollisions_->QueryBox(tree->center,tree->extents+Vec3{.01f,.01f,.01f});environmentStableIdEvidence_=std::ranges::any_of(treeObjects,[&](const auto& value){return value.id==tree->id;});const auto rock=std::ranges::find_if(environmentCollisionDescriptors_,[](const auto& value){return value.semantic==EnvironmentColliderSemantic3D::Rock;});if(rock==environmentCollisionDescriptors_.end())return false;const auto meleeObjects=environmentCollisions_->QueryBox(rock->center,rock->extents+Vec3{.25f,.25f,.25f});environmentMeleeQueryEvidence_=std::ranges::any_of(meleeObjects,[&](const auto& value){return value.id==rock->id;});if(!environmentCollisions_->UpdateObserver({10000,10000,10000})||environmentCollisions_->IsPartitionActive(EnvironmentPartitionId))return false;if(!SyncEnvironmentCollision(controller_.Position()))return false;environmentCollisionActiveReceipt_=environmentCollisions_->Receipt();environmentCollisionReloadStable_=firstActive.stateHash==environmentCollisionActiveReceipt_.stateHash;return environmentCollisionReloadStable_&&environmentStableIdEvidence_&&environmentRayEvidence_&&environmentMeleeQueryEvidence_&&environmentGameplayColliders_.size()==solid;}
void GameplayLabGame::OnStart(Engine& engine){
  if(!captureDirectory_.empty()){
    std::array<char,32> isolation{};const DWORD isolationLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_CAPTURE_LIGHTING_ISOLATION",isolation.data(),DWORD(isolation.size()));
    if(isolationLength>=isolation.size())throw std::runtime_error("Gameplay capture lighting isolation environment was too long");
    const std::string_view selected(isolation.data(),isolationLength);
    if(selected=="shadows-off")captureLightingIsolation_=1;
    else if(selected=="ao-off")captureLightingIsolation_=2;
    else if(selected=="shadows-ao-off")captureLightingIsolation_=3;
    else if(selected=="ibl-off")captureLightingIsolation_=4;
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
    else if(selected=="contact-off")captureLightingIsolation_=8;
#endif
    else if(!selected.empty())throw std::runtime_error("Gameplay capture lighting isolation environment was invalid");
    std::array<char,8> rawHdrDiagnostic{};const DWORD rawHdrDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_CAPTURE_RAW_HDR_DIAGNOSTIC",rawHdrDiagnostic.data(),DWORD(rawHdrDiagnostic.size()));
    if(rawHdrDiagnosticLength){if(rawHdrDiagnosticLength>=rawHdrDiagnostic.size()||std::string_view(rawHdrDiagnostic.data(),rawHdrDiagnosticLength)!="1")throw std::runtime_error("Gameplay raw HDR diagnostic environment was invalid");captureRawHdrDiagnostic_=true;}
    std::array<char,8> atmosphereOutputDiagnostic{};const DWORD atmosphereOutputDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_ATMOSPHERE_OUTPUT_DIAGNOSTIC",atmosphereOutputDiagnostic.data(),DWORD(atmosphereOutputDiagnostic.size()));
    if(atmosphereOutputDiagnosticLength){if(atmosphereOutputDiagnosticLength>=atmosphereOutputDiagnostic.size()||std::string_view(atmosphereOutputDiagnostic.data(),atmosphereOutputDiagnosticLength)!="1")throw std::runtime_error("Gameplay atmosphere output diagnostic environment was invalid");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      captureAtmosphereOutputDiagnostic_=true;
#else
      throw std::runtime_error("Gameplay atmosphere output diagnostic requires renderer test hooks");
#endif
    }
    std::array<char,8> preAtmosphereHdrDiagnostic{};const DWORD preAtmosphereHdrDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_PRE_ATMOSPHERE_HDR_DIAGNOSTIC",preAtmosphereHdrDiagnostic.data(),DWORD(preAtmosphereHdrDiagnostic.size()));
    if(preAtmosphereHdrDiagnosticLength){if(preAtmosphereHdrDiagnosticLength>=preAtmosphereHdrDiagnostic.size()||std::string_view(preAtmosphereHdrDiagnostic.data(),preAtmosphereHdrDiagnosticLength)!="1")throw std::runtime_error("Gameplay pre-atmosphere HDR diagnostic environment was invalid");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      if(!EngineAtmosphereDiagnosticAccess3D::SetPreAtmosphereHdrCapture(engine,true))throw std::runtime_error("Gameplay pre-atmosphere HDR diagnostic could not be enabled");
#else
      throw std::runtime_error("Gameplay pre-atmosphere HDR diagnostic requires atmosphere diagnostics");
#endif
    }
    std::array<char,8> postAtmosphereHdrDiagnostic{};const DWORD postAtmosphereHdrDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_POST_ATMOSPHERE_HDR_DIAGNOSTIC",postAtmosphereHdrDiagnostic.data(),DWORD(postAtmosphereHdrDiagnostic.size()));
    if(postAtmosphereHdrDiagnosticLength){if(postAtmosphereHdrDiagnosticLength>=postAtmosphereHdrDiagnostic.size()||std::string_view(postAtmosphereHdrDiagnostic.data(),postAtmosphereHdrDiagnosticLength)!="1")throw std::runtime_error("Gameplay post-atmosphere HDR diagnostic environment was invalid");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      if(!EngineAtmosphereDiagnosticAccess3D::SetPostAtmosphereHdrCapture(engine,true))throw std::runtime_error("Gameplay post-atmosphere HDR diagnostic could not be enabled");
#else
      throw std::runtime_error("Gameplay post-atmosphere HDR diagnostic requires atmosphere diagnostics");
#endif
    }
    std::array<char,8> postTransparentHdrDiagnostic{};const DWORD postTransparentHdrDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_POST_TRANSPARENT_HDR_DIAGNOSTIC",postTransparentHdrDiagnostic.data(),DWORD(postTransparentHdrDiagnostic.size()));
    if(postTransparentHdrDiagnosticLength){if(postTransparentHdrDiagnosticLength>=postTransparentHdrDiagnostic.size()||std::string_view(postTransparentHdrDiagnostic.data(),postTransparentHdrDiagnosticLength)!="1")throw std::runtime_error("Gameplay post-transparent HDR diagnostic environment was invalid");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      if(!EngineAtmosphereDiagnosticAccess3D::SetPostTransparentHdrCapture(engine,true))throw std::runtime_error("Gameplay post-transparent HDR diagnostic could not be enabled");
#else
      throw std::runtime_error("Gameplay post-transparent HDR diagnostic requires atmosphere diagnostics");
#endif
    }
    std::array<char,8> skipWaterDiagnostic{};const DWORD skipWaterDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_SKIP_WATER_DIAGNOSTIC",skipWaterDiagnostic.data(),DWORD(skipWaterDiagnostic.size()));
    if(skipWaterDiagnosticLength){if(skipWaterDiagnosticLength>=skipWaterDiagnostic.size()||std::string_view(skipWaterDiagnostic.data(),skipWaterDiagnosticLength)!="1")throw std::runtime_error("Gameplay skip-water diagnostic environment was invalid");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      if(!EngineAtmosphereDiagnosticAccess3D::SetSkipWaterTransparentDraw(engine,true))throw std::runtime_error("Gameplay skip-water diagnostic could not be enabled");
#else
      throw std::runtime_error("Gameplay skip-water diagnostic requires atmosphere diagnostics");
#endif
    }
    std::array<char,8> skipTransparentDiagnostic{};const DWORD skipTransparentDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_SKIP_TRANSPARENT_DIAGNOSTIC",skipTransparentDiagnostic.data(),DWORD(skipTransparentDiagnostic.size()));
    if(skipTransparentDiagnosticLength){if(skipTransparentDiagnosticLength>=skipTransparentDiagnostic.size()||std::string_view(skipTransparentDiagnostic.data(),skipTransparentDiagnosticLength)!="1")throw std::runtime_error("Gameplay skip-transparent diagnostic environment was invalid");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      if(!EngineAtmosphereDiagnosticAccess3D::SetSkipAllTransparentDraw(engine,true))throw std::runtime_error("Gameplay skip-transparent diagnostic could not be enabled");
#else
      throw std::runtime_error("Gameplay skip-transparent diagnostic requires atmosphere diagnostics");
#endif
    }
#if AEVRYND_SHADOW_MAP_DIAGNOSTICS
    std::array<char,8> shadowDiagnostic{};const DWORD shadowDiagnosticLength=GetEnvironmentVariableA("AEVRYND_GAMEPLAY_CAPTURE_SHADOW_EVIDENCE",shadowDiagnostic.data(),DWORD(shadowDiagnostic.size()));
    if(shadowDiagnosticLength){if(shadowDiagnosticLength>=shadowDiagnostic.size()||std::string_view(shadowDiagnostic.data(),shadowDiagnosticLength)!="1")throw std::runtime_error("Gameplay capture shadow evidence environment was invalid");captureShadowMapDiagnostic_=true;}
#endif
  }
  engine.SetRenderQuality(RenderQuality3D::Ultra);
  std::array<char,128> expectedFingerprint{};
  const DWORD expectedLength=GetEnvironmentVariableA("AEVRYND_EXPECTED_CHARACTER_FINGERPRINT",expectedFingerprint.data(),DWORD(expectedFingerprint.size()));
  if(expectedLength&&expectedLength<expectedFingerprint.size())expectedSourceFingerprint_.assign(expectedFingerprint.data(),expectedLength);
  sourceFingerprintPassed_=expectedSourceFingerprint_.empty()||expectedSourceFingerprint_==actualSourceFingerprint_;
  if(!sourceFingerprintPassed_)throw std::runtime_error(std::format(
    "Gameplay character build fingerprint mismatch: expected={} actual={}",expectedSourceFingerprint_,actualSourceFingerprint_));
  std::array<char,32768> phasePath{};const DWORD phaseLength=GetEnvironmentVariableA("AEVRYND_FATAL_DIAGNOSTIC_PATH",phasePath.data(),DWORD(phasePath.size()));if(phaseLength&&phaseLength<phasePath.size()){std::ofstream phase(std::filesystem::path(std::string(phasePath.data(),phaseLength)),std::ios::app);phase<<"BuildFingerprint:"<<actualSourceFingerprint_<<'\n';phase.flush();}
  rules_.Reset();PrepareGameplayCaptureStaging3D(rules_,!captureDirectory_.empty());controller_.Reset({-16,0,8});
  boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());
  sphereMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(18,12));
  if(!showcaseEnvironment_.Initialize(0x4a17u))throw std::runtime_error("Gameplay showcase environment initialization failed");
  showcaseCylinderMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitCylinder(14));
  // This existing, otherwise-unsubmitted resource slot owns the one-time merged
  // palette house mesh; its normal destroy/check lifecycle remains unchanged.
  showcaseConeHighMesh_=engine.CreateMeshResource3D(CreateGameplayPaletteBuildingMesh3D());
  showcaseConeLowMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitCone(7));
  // 每個既有 placement 共用一片林下群落網格，不改 descriptor、身份或碰撞。
  // 高細節增加葉片與彎曲段數，遠景也保留六葉輪廓；群落 bounds、葉高與風權重
  // 仍由相同生成器約束，避免只把少量硬片放大成不自然的巨草。
  // 保留原 grass bounds 作上帝視角配置代理；河岸專用網格使用既有、原先
  // 只建立但未提交的兩個低模資源槽，不增加成員、descriptor 或 draw。
  auto grassHigh=CreateGameplayGrassCommunityMesh3D(15,4),grassLow=CreateGameplayGrassCommunityMesh3D(10,2);
  showcaseGrassHighMesh_=engine.CreateMeshResource3D(grassHigh);
  showcaseGrassLowMesh_=engine.CreateMeshResource3D(grassLow);
  showcaseFernHighMesh_=engine.CreateMeshResource3D(CreateGameplayForestGroundCoverMesh3D(GameplayForestGroundCoverSpecies3D::Fern,GameplayForestGroundCoverDetail3D::Near,0x4a17u));
  showcaseFernLowMesh_=engine.CreateMeshResource3D(CreateGameplayForestGroundCoverMesh3D(GameplayForestGroundCoverSpecies3D::Fern,GameplayForestGroundCoverDetail3D::Far,0x4a17u));
  // Upland placements use one shared community mesh: shrub volume, fern fronds
  // and low leaf/moss cards.  This adds a real understory layer without adding
  // descriptors, draw calls or collision objects.
  showcaseShrubHighMesh_=engine.CreateMeshResource3D(CreateGameplayForestGroundCoverCommunityMesh3D(GameplayForestGroundCoverDetail3D::Near,0x7c31u));
  showcaseShrubLowMesh_=engine.CreateMeshResource3D(CreateGameplayForestGroundCoverCommunityMesh3D(GameplayForestGroundCoverDetail3D::Far,0x7c31u));
  showcaseTreeLowMesh_=engine.CreateMeshResource3D(CreateGameplayForestRiverbankGroundCoverMesh3D(GameplayForestGroundCoverDetail3D::Near,0x31b7u));
  showcaseRockLowMesh_=engine.CreateMeshResource3D(CreateGameplayForestRiverbankGroundCoverMesh3D(GameplayForestGroundCoverDetail3D::Far,0x31b7u));
  showcaseCottageFrontMesh_=engine.CreateMeshResource3D(CreateCottageFacadeMesh3D(true,true));
  showcaseCottageBackMesh_=engine.CreateMeshResource3D(CreateCottageFacadeMesh3D(false,true));
  showcaseCottageSideMesh_=engine.CreateMeshResource3D(CreateCottageFacadeMesh3D(false,false));
  showcaseCottageRoofMesh_=engine.CreateMeshResource3D(CreateCottageRoofMesh3D());
  showcaseTerrain_.Configure({{-256,0,-256},129,129,4});
  if(!showcaseTerrain_.GenerateUltra(0x4a17u,{10.5f,.0105f,.56f,.22f,24.f,.34f,7,3,.68f,.16f}))throw std::runtime_error("Gameplay showcase terrain generation failed");
  showcaseTerrain_.FlattenCircle({-16,0,8},8,0,8);
  showcaseTerrain_.FlattenCircle({-12,0,24},7.2f,0,4.5f);
  CarveShowcaseRiverShore3D(showcaseTerrain_);
  // 只在既有 near-band 倒木附近留下壓痕與鬆土；先縮成十個實際 descriptor，
  // 再限制高度場取樣的 bounding boxes，避免把地表初始化變成全場景乘積掃描。
  std::vector<ShowcaseSceneDescriptor3D> terrainDisturbanceAnchors;
  for(const auto& descriptor:showcaseEnvironment_.Descriptors())
    if(descriptor.kind==ShowcaseSceneKind3D::Tree&&descriptor.lodGroup==0&&
       descriptor.meshAsset.find("dead_tree_trunk_02")!=std::string::npos)
      terrainDisturbanceAnchors.push_back(descriptor);
  const auto terrainSettings=showcaseTerrain_.Settings();
  std::vector<std::uint8_t> disturbedSamples(
      std::size_t(terrainSettings.width)*terrainSettings.depth);
  for(const auto& anchor:terrainDisturbanceAnchors){
    constexpr float maximumFootprintRadius=6.f;
    const auto sampleIndex=[&](float coordinate,float origin,unsigned limit){
      return unsigned(Clamp(std::floor((coordinate-origin)/terrainSettings.cellSize),
                            0.f,float(limit-1)));
    };
    const unsigned minimumX=sampleIndex(anchor.position.x-maximumFootprintRadius,
                                         terrainSettings.origin.x,terrainSettings.width);
    const unsigned maximumX=sampleIndex(anchor.position.x+maximumFootprintRadius+
                                         terrainSettings.cellSize,
                                         terrainSettings.origin.x,terrainSettings.width);
    const unsigned minimumZ=sampleIndex(anchor.position.z-maximumFootprintRadius,
                                         terrainSettings.origin.z,terrainSettings.depth);
    const unsigned maximumZ=sampleIndex(anchor.position.z+maximumFootprintRadius+
                                         terrainSettings.cellSize,
                                         terrainSettings.origin.z,terrainSettings.depth);
    for(unsigned z=minimumZ;z<=maximumZ;++z)
      for(unsigned x=minimumX;x<=maximumX;++x)
        disturbedSamples[std::size_t(z)*terrainSettings.width+x]=1;
  }
  for(unsigned z=0;z<terrainSettings.depth;++z)for(unsigned x=0;x<terrainSettings.width;++x){
    if(!disturbedSamples[std::size_t(z)*terrainSettings.width+x])continue;
    const auto position=showcaseTerrain_.SamplePosition(x,z);
    const auto disturbance=SampleShowcaseTerrainDisturbance3D(
        position,terrainDisturbanceAnchors);
    if(disturbance.weight>0.f)
      showcaseTerrain_.SetHeight(x,z,showcaseTerrain_.Height(x,z)+disturbance.heightDelta);
  }
  showcaseTerrain_.RecalculateRange();
  ShowcaseTerrainGroundingReceipt3D terrainGrounding{};
  if(!showcaseEnvironment_.GroundLandDescriptors(showcaseTerrain_,&terrainGrounding)||
     terrainGrounding.groundedDescriptors!=showcaseEnvironment_.LandReceipt().placements)
    throw std::runtime_error("Gameplay showcase land descriptors could not be grounded to the final terrain");
  showcaseRiverMesh_=engine.CreateMeshResource3D(CreateRiverHeroCellMesh3D(showcaseTerrain_));
  // 真實三層 PBR 貼圖提供細節；RGB 只保留倒木造成的微地表擾動，
  // path/leaf-litter/wet-soil 則透過 Terrain 專用 signed UV 直接重配材質層。
  // alpha 仍原封不動保存高度供原有坡度／高度 biome 使用。
  auto terrainMesh=CreateShowcaseRiverRefinedTerrainMesh3D(showcaseTerrain_,{{1,1,1,1},{1,1,1,1},{1,1,1,1},.7f,{1,1,1,1},-.65f,2.2f,0x4a17u,.0042f,0.f,.32f,{1,1,1,1},{1,1,1,1},{1,1,1,1},.34f,.7f,.11f,.7f,.1f});
  const std::size_t authoredTerrainVertexCount=terrainMesh.vertices.size();
  if(!PrepareShowcaseSurfaceTerrainMesh3D(terrainMesh))
    throw std::runtime_error("Gameplay showcase terrain apron construction failed");
  // 外環建立後一次寫滿所有頂點，禁止新頂點意外沿用已廢棄的 world UV。
  ApplyShowcaseTerrainBiomeUv3D(terrainMesh,authoredTerrainVertexCount,showcaseEnvironment_.Descriptors());
  ShowcaseTerrainBiomeUvValidation3D biomeValidation{};
  if(!ValidateShowcaseTerrainBiomeUv3D(terrainMesh,authoredTerrainVertexCount,&biomeValidation))
    throw std::runtime_error("Gameplay showcase terrain biome UV contract failed: reason="+std::to_string(unsigned(biomeValidation.failure))+" element="+std::to_string(biomeValidation.elementIndex)+" u="+std::to_string(biomeValidation.minimumU)+"/"+std::to_string(biomeValidation.maximumU));
  for(auto& vertex:terrainMesh.vertices){const auto source=UnpackColorRgba8(vertex.color);const auto disturbance=SampleShowcaseTerrainDisturbance3D({vertex.x,vertex.y,vertex.z},terrainDisturbanceAnchors);vertex.color=PackColorRgba8({source.r*disturbance.tint.r,source.g*disturbance.tint.g,source.b*disturbance.tint.b,source.a});}
  showcaseTerrainMesh_=engine.CreateMeshResource3D(terrainMesh);
  if(!showcaseCylinderMesh_||!showcaseConeHighMesh_||!showcaseConeLowMesh_||!showcaseGrassHighMesh_||!showcaseGrassLowMesh_||!showcaseFernHighMesh_||!showcaseFernLowMesh_||!showcaseShrubHighMesh_||!showcaseShrubLowMesh_||!showcaseTreeLowMesh_||!showcaseRockLowMesh_||!showcaseRiverMesh_||!showcaseCottageFrontMesh_||!showcaseCottageBackMesh_||!showcaseCottageSideMesh_||!showcaseCottageRoofMesh_||!showcaseTerrainMesh_)throw std::runtime_error("Gameplay showcase procedural GPU upload failed");
  const auto assetRoot=engine.ExecutableDirectory()/"assets";
  engine.SetMaterialTextureMemoryBudget(128*1024*1024);
  showcaseMaterialLibrary_.Initialize(engine,assetRoot/"environment-material-packs");
  std::array<MaterialAssetDefinition3D,4> showcaseMaterials={
    MaterialAssetDefinition3D{"ground",{},{{1,1,1,1},{.78f,0,0,.9f},false,{},.085f}},
    MaterialAssetDefinition3D{"wood",{"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",1024,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.48f,0,0,1},true,{},.48f}},
    MaterialAssetDefinition3D{"bark",{"poly-haven/bark_brown_02/1k/base-color.jpg","poly-haven/bark_brown_02/1k/roughness.jpg",1024,"poly-haven/bark_brown_02/1k/normal-dx.jpg"},{{1,1,1,1},{.82f,0,0,.92f},true,{},1.f}},
    MaterialAssetDefinition3D{"rock",{"poly-haven/aerial_ground_rock/1k/base-color.jpg","poly-haven/aerial_ground_rock/1k/roughness.jpg",1024,"poly-haven/aerial_ground_rock/1k/normal-dx.jpg"},{{1,1,1,1},{.76f,0,0,1},true,{},.5f}}};
  // WoodSiding006 原本只載入但沒有送入繪製；木屋牆面現在會在封存擷取中使用它，
  // 因此明確綁定不可變來源快照，禁止確定性驗證接受無法核實的貼圖集合。
  showcaseMaterials[1].textures.integrity={
    MaterialTextureIntegrity3D{724730,"c626ec1b31055a85e8e09d42ce2d0c6f0b968250ad6672ea3282b136dd568d46"},
    MaterialTextureIntegrity3D{423700,"adc51758ddaf97bcd795eef154f705a4f020594efc5a5b515d0f92b39240c844"},
    MaterialTextureIntegrity3D{1849769,"55895492f8b92a1d847201e76d339679097d0901f7b3fbf9bbcd1c740b05f3fb"}};
  auto& terrain=showcaseMaterials[0];
  // 三層都由封存 manifest 建構；共同使用 2K 上限與 packed ORM 語意。
  CookedRenderManifest3D terrainManifest{};
  CookedRenderManifestError3D terrainManifestError{};
  const auto terrainAssetRoot=assetRoot/"photoreal-cooked";
  if(!terrainManifest.LoadFile(terrainAssetRoot/"runtime-render-manifest.json",&terrainManifestError))
    throw std::runtime_error("Gameplay terrain production manifest failed: "+terrainManifestError.message);
  ShowcaseTerrainMaterialReceipt3D terrainMaterialReceipt{};
  terrain.material.materialClass=MaterialClass3D::Terrain;
  // 正式載入前鎖定三個不同的 production semantic identities。
  if(!BuildShowcaseTerrainMaterialTextures3D(terrainManifest,terrainAssetRoot,terrain.textures,&terrainMaterialReceipt))
    throw std::runtime_error("Gameplay terrain material final binding failed");
  if(!showcaseMaterialLibrary_.LoadAll(showcaseMaterials))throw std::runtime_error("Gameplay showcase CC0 PBR material loading failed");
  if(const auto* value=showcaseMaterialLibrary_.Find("ground"))showcaseGroundMaterial_=*value;
  if(const auto* value=showcaseMaterialLibrary_.Find("wood"))showcaseWoodMaterial_=*value;
  if(const auto* value=showcaseMaterialLibrary_.Find("bark"))showcaseBarkMaterial_=*value;
  if(const auto* value=showcaseMaterialLibrary_.Find("rock"))showcaseRockMaterial_=*value;
  showcaseGroundMaterial_.normalScale=.65f;showcaseWoodMaterial_.normalScale=.8f;showcaseBarkMaterial_.normalScale=1.1f;showcaseRockMaterial_.normalScale=1.2f;
  const auto showcaseModelRoot=assetRoot/"environment-model-packs";
  showcaseModelMaterialLibrary_.Initialize(engine,showcaseModelRoot);
  const std::array<MaterialAssetDefinition3D,2> showcaseModelMaterials={
    MaterialAssetDefinition3D{"quiver-tree-02",{"poly-haven/quiver_tree_02/1k/base-color.jpg","poly-haven/quiver_tree_02/1k/roughness.jpg",1024,"poly-haven/quiver_tree_02/1k/normal-dx.jpg"},{{1,1,1,1},{.82f,0,0,.92f},true,{},1.f,true}},
    MaterialAssetDefinition3D{"rock-09",{"poly-haven/rock_09/1k/base-color.jpg","poly-haven/rock_09/1k/roughness.jpg",1024,"poly-haven/rock_09/1k/normal-dx.jpg"},{{1,1,1,1},{.76f,0,0,1},true,{},1.f,true}}};
  if(!showcaseModelMaterialLibrary_.LoadAll(showcaseModelMaterials))throw std::runtime_error("Gameplay showcase CC0 model material loading failed");
  if(const auto* value=showcaseModelMaterialLibrary_.Find("quiver-tree-02"))showcaseQuiverTreeMaterial_=*value;
  if(const auto* value=showcaseModelMaterialLibrary_.Find("rock-09"))showcaseRock09Material_=*value;
  showcaseQuiverTreeMaterial_.materialClass=MaterialClass3D::Foliage;showcaseQuiverTreeMaterial_.normalScale=.9f;showcaseRock09Material_.normalScale=1.15f;
  const auto quiverTree=LoadAevryndMeshFile(showcaseModelRoot/"poly-haven"/"quiver_tree_02"/"quiver_tree_02.aemesh");
  const auto rock09=LoadAevryndMeshFile(showcaseModelRoot/"poly-haven"/"rock_09"/"rock_09.aemesh");
  if(!quiverTree||!rock09)throw std::runtime_error("Gameplay showcase CC0 model mesh loading failed");
  showcaseQuiverTreeMesh_=engine.CreateMeshResource3D(quiverTree.mesh);showcaseRock09Mesh_=engine.CreateMeshResource3D(rock09.mesh);
  if(!showcaseQuiverTreeMesh_||!showcaseRock09Mesh_)throw std::runtime_error("Gameplay showcase CC0 model GPU upload failed");
  const auto cookedManifest=assetRoot/"photoreal-cooked"/"runtime-render-manifest.json";
  if(!showcaseRenderAssets_.LoadProduction(engine,sceneEcs_,cookedManifest,
      showcaseEnvironment_.Descriptors(),showcaseProductionReceipt_)||
     !showcaseProductionReceipt_.photorealActive||
     showcaseProductionReceipt_.payloadFiles!=72||
     showcaseProductionReceipt_.profileAssets!=11||
     showcaseProductionReceipt_.materialSets!=14||
     showcaseProductionReceipt_.meshParts!=9||
     showcaseProductionReceipt_.meshLodLevels!=27||
     showcaseProductionReceipt_.iblFiles!=3)
     throw std::runtime_error("Gameplay photoreal cooked exact-set activation failed");
  if((captureLightingIsolation_&4u)&&!engine.ClearEnvironmentLighting3D())
    throw std::runtime_error("Gameplay capture IBL isolation failed");
  const auto modelRoot=assetRoot/"model-packs";
  const auto materialRoot=assetRoot/"material-packs"/"original";
  auto female=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-female"/"realistic-female.aeskin");
  auto male=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-male"/"realistic-male.aeskin");
  if(!female||!male)throw std::runtime_error("Gameplay Lab requires the existing F2/M2 character packs");
  std::string error;
  if(!LoadCharacterMaterialBindings3D(engine,femaleMaterialLibrary_,female.asset,materialRoot,materialRoot/"female.materials.json",femaleMaterials_,&error)||
     !LoadCharacterMaterialBindings3D(engine,maleMaterialLibrary_,male.asset,materialRoot,materialRoot/"male.materials.json",maleMaterials_,&error))
    throw std::runtime_error(error);
  characterMaterialEvidence_=BuildGameplayCharacterMaterialEvidence3D(femaleMaterials_.size(),maleMaterials_.size());
  if(!characterMaterialEvidence_.Complete()||femaleMaterials_.size()!=9||maleMaterials_.size()!=9)throw std::runtime_error("Gameplay character material count mismatch");
  // 明確 opt-in 專用資產路徑；上方 F2/M2 九材質 gate 及武器材質仍保持原契約。
  std::array<wchar_t,32768> candidateRoot{};
  const DWORD candidateRootLength=GetEnvironmentVariableW(L"AEVRYND_CANDIDATE7_ROOT",candidateRoot.data(),DWORD(candidateRoot.size()));
  if(candidateRootLength){
    if(candidateRootLength>=candidateRoot.size())throw std::runtime_error("Candidate7 root path too long");
    auto package=LoadCandidate7Package3D(std::filesystem::path(candidateRoot.data()),[](const auto& file,std::string_view expected){std::array<std::byte,32> hash{};return CaptureFileSha256(file,hash)&&CaptureShaHex(hash)==expected;});
    std::array<CharacterBindBounds3D,4> candidateBounds{};
    for(std::size_t i=0;i<package.components.size();++i)
      if(!MeasureCharacterBindBounds3D(package.components[i].asset.model.bindMesh.vertices,candidateBounds[i]))
        throw std::runtime_error("Candidate7 component bind bounds contract failed");
    if(!BuildCandidate7PresentationScale3D(candidateBounds,1.8f,candidate7PresentationScale_))
      throw std::runtime_error("Candidate7 shared presentation scale contract failed");
    for(std::size_t i=0;i<package.components.size();++i){
      auto& part=package.components[i];auto& library=candidate7MaterialLibraries_[i];
      if(!library.Initialize(engine,part.materialRoot)||!library.LoadAll(part.sidecar.materials))throw std::runtime_error("Candidate7 material GPU load failed");
      for(const auto& name:part.asset.materialSlotNames){const auto* material=library.Find(name);if(!material)throw std::runtime_error("Candidate7 material slot missing");candidate7Materials_[i].push_back(*material);}
      if(i){candidate7AttachmentModels_[i-1]=std::move(part.asset.model);candidate7AttachmentSubmeshes_[i-1]=std::move(part.asset.submeshes);candidate7AttachmentMeshes_[i-1]=engine.CreateSkinnedMeshResource3D(candidate7AttachmentModels_[i-1]);if(!candidate7AttachmentMeshes_[i-1])throw std::runtime_error("Candidate7 attachment upload failed");}
    }
    female.asset=std::move(package.components[0].asset);candidate7Active_=true;
    if(!captureDirectory_.empty()){std::filesystem::create_directories(captureDirectory_);std::ofstream evidence(captureDirectory_/"candidate7-package-receipt.txt");evidence<<"CANDIDATE7=1\nSOURCE=body59-candidate7+upper94+lower17+hair92\nCOMPONENTS=4\nMATERIALS=6,6,2,4\nJOINTS=53\nLEGACY_NINE_MATERIAL_GATE=UNCHANGED\nPOSE_AND_VISUAL_APPROVAL=NOT_CLAIMED\n";evidence.flush();if(!evidence)throw std::runtime_error("Candidate7 receipt write failed");}
  }
  if(!femaleAnimation_.Initialize(female.asset,&error)||!maleAnimation_.Initialize(male.asset,&error)||!monsterAnimation_.Initialize(male.asset,&error))
    throw std::runtime_error(error);
  auto femaleFirstPersonArms=candidate7Active_?BuildCandidate7Arms3D(female.asset):BuildFirstPersonArmsSkinnedMesh3D(female.asset);
  auto maleFirstPersonArms=BuildFirstPersonArmsSkinnedMesh3D(male.asset);
  if(!femaleFirstPersonArms||!maleFirstPersonArms)
    throw std::runtime_error("Gameplay first-person arms derivation failed");
  femaleFirstPersonArmsEvidence_=BuildGameplayFirstPersonArmsEvidence3D(femaleFirstPersonArms);
  maleFirstPersonArmsEvidence_=BuildGameplayFirstPersonArmsEvidence3D(maleFirstPersonArms);
  if(!femaleFirstPersonArmsEvidence_.Complete()||!maleFirstPersonArmsEvidence_.Complete())
    throw std::runtime_error("Gameplay first-person arms evidence is incomplete");
  CharacterEquipmentAsset3D femaleWeapon{},maleWeapon{};
  if(!CreateCharacterEquipmentAsset3D(female.asset,femaleAnimation_.Rig(),CharacterEquipmentShape3D::Sword,femaleWeapon,&error)||
     !CreateCharacterEquipmentAsset3D(male.asset,maleAnimation_.Rig(),CharacterEquipmentShape3D::Sword,maleWeapon,&error))
    throw std::runtime_error(error);
  if(!ValidateCharacterSwordGeometry3D(femaleWeapon)||!ValidateCharacterSwordGeometry3D(maleWeapon))
    throw std::runtime_error("Gameplay sword geometry contract is unavailable");
  femaleSwordGeometry_=femaleWeapon.swordGeometry;maleSwordGeometry_=maleWeapon.swordGeometry;
  femaleWeaponProjection_=BuildGameplayFirstPersonWeaponProjectionGeometry3D(femaleWeapon);
  maleWeaponProjection_=BuildGameplayFirstPersonWeaponProjectionGeometry3D(maleWeapon);
  if(!femaleWeaponProjection_.Valid()||!maleWeaponProjection_.Valid())
    throw std::runtime_error("Gameplay actual weapon projection geometry is invalid");
  femaleWeaponSubmeshes_=femaleWeapon.submeshes;maleWeaponSubmeshes_=maleWeapon.submeshes;
  femaleWeaponMaterials_={femaleMaterials_[8],femaleMaterials_[7]};
  maleWeaponMaterials_={maleMaterials_[8],maleMaterials_[7]};
  const auto configureSwordSteel=[](Material3D& material){
    // 武器刀身使用中性高金屬表面；角色飾品的金色貼圖不可污染鋼刃。
    material.tint={.72f,.79f,.86f,1.f};
    material.surface.roughness=.22f;material.surface.metallic=.94f;
    material.surface.emissive=0.f;material.surface.occlusion=1.f;
    material.textures={};material.useUv=false;material.shadingModel=CharacterShadingModel3D::Default;
  };
  configureSwordSteel(femaleWeaponMaterials_[0]);
  configureSwordSteel(maleWeaponMaterials_[0]);
  femaleSubmeshes_=female.asset.submeshes;maleSubmeshes_=male.asset.submeshes;
  femaleFirstPersonArmsSubmeshes_=std::move(femaleFirstPersonArms.submeshes);
  maleFirstPersonArmsSubmeshes_=std::move(maleFirstPersonArms.submeshes);
  femaleFirstPersonArmsModel_=std::move(femaleFirstPersonArms.model);
  maleFirstPersonArmsModel_=std::move(maleFirstPersonArms.model);
  femaleModel_=std::move(female.asset.model);maleModel_=std::move(male.asset.model);
  femaleMesh_=engine.CreateSkinnedMeshResource3D(femaleModel_);
  maleMesh_=engine.CreateSkinnedMeshResource3D(maleModel_);
  femaleFirstPersonArmsMesh_=engine.CreateSkinnedMeshResource3D(femaleFirstPersonArmsModel_);
  maleFirstPersonArmsMesh_=engine.CreateSkinnedMeshResource3D(maleFirstPersonArmsModel_);
  femaleWeaponMesh_=engine.CreateSkinnedMeshResource3D(femaleWeapon.model);
  maleWeaponMesh_=engine.CreateSkinnedMeshResource3D(maleWeapon.model);
  if(!femaleMesh_||!maleMesh_||!femaleFirstPersonArmsMesh_||!maleFirstPersonArmsMesh_||!femaleWeaponMesh_||!maleWeaponMesh_)throw std::runtime_error("Gameplay character GPU resource creation failed");
  if(candidate7Active_){
    auto candidateFootProfile=BuildCandidate7FootProfile3D(femaleModel_,candidate7AttachmentModels_[1],femaleAnimation_.Rig());
    for(auto& foot:candidateFootProfile.feet)foot.soleOffset*=candidate7PresentationScale_.uniformScale;
    if(!femaleFootPlacement_.Configure(candidateFootProfile))
      throw std::runtime_error("Candidate7 foot placement configuration failed");
  }
  if(!maleFootPlacement_.Configure(BuildSkinnedAssetFootProfile3D(maleModel_,maleModel_,maleAnimation_.Rig())))
    throw std::runtime_error("Male asset-derived foot placement configuration failed");
  if(!femaleFootPlacement_.Prepare(femaleModel_.skeleton,femaleAnimation_.Rig(),femaleModel_.skeletonCompatibilityId)||
     !maleFootPlacement_.Prepare(maleModel_.skeleton,maleAnimation_.Rig(),maleModel_.skeletonCompatibilityId))
    throw std::runtime_error("Gameplay foot placement preparation failed");
  for(unsigned state=0;state<11;++state){
    femaleAnimation_.SetState(AnimationState3D(state),true);femaleAnimation_.Tick(.173f);
    maleAnimation_.SetState(AnimationState3D(state),true);maleAnimation_.Tick(.173f);
    if(!femaleAnimation_.EvaluateInto(femalePose_)||!maleAnimation_.EvaluateInto(malePose_))throw std::runtime_error("Fallback animation evaluation failed");
    if(candidate7Active_)ApplyCandidate7ArmRest3D(femaleModel_,femaleAnimation_.Rig(),femalePose_);
    femalePoseHashes_[state]=HashPose(femalePose_);malePoseHashes_[state]=HashPose(malePose_);
  }
  femaleAnimation_.SetState(AnimationState3D::Idle,true);maleAnimation_.SetState(AnimationState3D::Idle,true);monsterAnimation_.SetState(AnimationState3D::Idle,true);
  // 權威 pose 必須在 fixed simulation 建立；不可把上一輪語意驗證留下的 Death pose
  // 帶入首個 God camera search，也不可等待首個 OnRender 才修正。
  if(!femaleAnimation_.EvaluateInto(femalePose_)||!maleAnimation_.EvaluateInto(malePose_)||
     !monsterAnimation_.EvaluateInto(monsterPose_))throw std::runtime_error("Gameplay authoritative idle pose initialization failed");
  if(candidate7Active_)ApplyCandidate7ArmRest3D(femaleModel_,femaleAnimation_.Rig(),femalePose_);
  scene_.SetBoxMesh(boxMesh_);
  scene_.Reserve({0,64,256,512,256});
  paletteUi_->SetLayout({18,502,344,150},4);
  showcaseEnvironment_.ApplyHighProfile(engine.Draw());if(engine.RequestedRenderQuality()==RenderQuality3D::Ultra)engine.Draw().SetDirectionalShadowCascades({4,4096,.1f,220.f,.70f,.10f});
  showcaseRendererBaseline_=engine.GetRendererStats();
  if(!InitializeEnvironmentCollision())throw std::runtime_error("Gameplay environment collision streaming initialization failed");
  collisions_.CreateBox({{{0,1.5f,-12},{4,1.5f,.6f}},1,0xffffffffu});
  for(int i=0;i<18;++i){const Vec3 p{-8.f+float(i%6)*1.6f,.65f+float(i/6)*1.25f,-3.f};if(!palette_->PlaceAt(p,PaletteCategory::Physics))throw std::runtime_error("Initial gameplay palette transaction failed");}
  initialSeedEntityIds_.clear();initialSeedEntityIds_.reserve(rules_.Entities().size());for(const auto& entity:rules_.Entities())initialSeedEntityIds_.push_back(entity.id);
  if(!captureDirectory_.empty()){
    captureScenarioClock_={};
    if(temporalFixtureWarmupFrames_){
      if(temporalFixtureWarmupFrames_<3||temporalFixtureWarmupFrames_>5)throw std::runtime_error("Temporal fixture warmup contract was invalid");
      captureIndex_=1;captureScenarioClock_.scenario=1;
    }
    std::filesystem::create_directories(captureDirectory_);
    constexpr std::uintmax_t MinimumCaptureFreeBytes=4ull*1024ull*1024ull*1024ull;
    std::error_code captureSpaceError;
    const auto captureSpace=std::filesystem::space(captureDirectory_,captureSpaceError);
    if(captureSpaceError||captureSpace.available<MinimumCaptureFreeBytes)
      throw std::runtime_error("Gameplay capture requires at least 4 GiB of free disk space");
    for(std::size_t index=0;index<Names.size();++index)CleanupGameplayHdrCaptureArtifacts(index,true);
    {std::error_code ignored;std::filesystem::remove(captureDirectory_/"gameplay-hdr-capture-receipt.json",ignored);std::filesystem::remove(captureDirectory_/"gameplay-hdr-capture-receipt.json.tmp",ignored);}
    std::unordered_set<std::uint64_t> femaleUnique(femalePoseHashes_.begin(),femalePoseHashes_.end()),maleUnique(malePoseHashes_.begin(),malePoseHashes_.end());
    if(femaleUnique.size()!=11||maleUnique.size()!=11||std::ranges::any_of(femalePoseHashes_,[](auto value){return value==0;})||std::ranges::any_of(malePoseHashes_,[](auto value){return value==0;}))throw std::runtime_error("Semantic gameplay pose hashes are not unique");
    std::ofstream poseOut(captureDirectory_/"gameplay-pose-receipt.json",std::ios::trunc);
    poseOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    poseOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabSemanticSkinning\",\"female\":[";
    for(std::size_t i=0;i<11;++i){if(i)poseOut<<",";poseOut<<femalePoseHashes_[i];}
    poseOut<<"],\"male\":[";
    for(std::size_t i=0;i<11;++i){if(i)poseOut<<",";poseOut<<malePoseHashes_[i];}
    poseOut<<"]}";
    std::ofstream jointOut(captureDirectory_/"weapon-joint.txt",std::ios::trunc);
    jointOut<<maleAnimation_.Rig().Joint(HumanoidJoint3D::RightHand)<<" RightHand";
    for(auto name:Names){std::error_code ec;std::filesystem::remove(captureDirectory_/name,ec);}
  }
  UpdateCamera(engine);
  PublishFirstPersonPose(0.f);
}
void GameplayLabGame::OnStop(Engine &engine) {
  // Engine 的例外清理可能再次呼叫 OnStop；失敗清理完成後不可重複拋出。
  if(!projectionFailureReason_.empty()&&projectionFailureStopFinished_)return;
  // 失敗保全不輸出任何看似成功的停止階段 receipt；資源釋放流程仍照常執行。
  if(!projectionFailureReason_.empty())captureDirectory_.clear();
  firstPersonPose_={};captureFrozenFirstPersonPose_={};firstPersonPoseWorkspace_={};
  activeCaptureRoute_.Clear();
  if(cursorCaptured_){ReleaseCapture();ClipCursor(nullptr);ShowCursor(TRUE);cursorCaptured_=false;}
  palette_->EndGrab();
  if (!captureDirectory_.empty()) {
    const auto grab = palette_->Stats();
    std::ofstream out(captureDirectory_ / "gameplay-grab-receipt.json",
                      std::ios::trunc);
    out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    out << std::format(
        "{{\"runId\":\"{}\",\"source\":\"GameplayLabGrab\",\"pickAttempts\":{},"
        "\"pickRejected\":{},\"grabBegins\":{},\"grabHoldSteps\":{},"
        "\"releaseDrops\":{},\"activeAtStop\":{}}}",
        runId_, grab.pickAttempts, grab.pickRejected, grab.grabs,
        grab.dragSteps, grab.drops, palette_->Grabbed() ? 1 : 0);
  }
  if(!captureDirectory_.empty()){
    const auto ui=paletteUi_->Receipt();
    std::ofstream uiOut(captureDirectory_/"gameplay-palette-ui-receipt.json",std::ios::trunc);
    uiOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    uiOut<<std::format("{{\"runId\":\"{}\",\"source\":\"GameplayObjectPaletteUi3D\",\"categories\":6,\"drawnFrames\":{},\"cleanHudPresentationFrames\":{},\"pointerDowns\":{},\"drags\":{},\"previews\":{},\"commits\":{},\"invalidPlacements\":{},\"cancels\":{},\"evidenceColliderReleased\":{},\"evidenceColliderHandleCleared\":{},\"deterministicHash\":{}}}",runId_,paletteUiDrawnFrames_,captureCleanHudFrames_,ui.pointerDowns,ui.drags,ui.previews,ui.commits,ui.invalidPlacements,ui.cancels,captureEvidenceColliderReleased_?1:0,captureEvidenceCollider_?0:1,ui.deterministicHash);
  }
  palette_->Clear();
  const auto environmentCollisionBeforeRelease=environmentCollisionActiveReceipt_;
  ReleaseEnvironmentGameplayColliders();
  EnvironmentCollisionReleaseReceipt3D environmentCollisionRelease{};
  EnvironmentCollisionStreamingReceipt3D environmentCollisionAfterRelease{};
  if(environmentCollisions_){environmentCollisionRelease=environmentCollisions_->Release();environmentCollisionAfterRelease=environmentCollisions_->Receipt();environmentCollisions_.reset();}
  std::vector<EnvironmentColliderDescriptor3D>{}.swap(environmentCollisionDescriptors_);
  std::vector<ColliderHandle3D>{}.swap(environmentGameplayColliders_);
  if(!captureDirectory_.empty()){
    std::ofstream collisionOut(captureDirectory_/"gameplay-environment-collision-receipt.json",std::ios::trunc);
    collisionOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    collisionOut<<std::format("{{\"runId\":\"{}\",\"source\":\"EnvironmentCollisionStreaming3D\",\"partitionId\":{},\"stateHash\":{},\"descriptors\":{},\"solidDescriptors\":{},\"nonSolidDescriptors\":{},\"activePartitions\":{},\"activeColliders\":{},\"activePhysicsBodies\":{},\"activations\":{},\"deactivations\":{},\"failedActivations\":{},\"rollbacks\":{},\"stableIdEvidence\":{},\"rayEvidence\":{},\"meleeQueryEvidence\":{},\"reloadStable\":{},\"releasedDescriptors\":{},\"releasedColliders\":{},\"releasedPhysicsBodies\":{},\"retainedBytesAfterRelease\":{},\"activeCollidersAfterRelease\":{},\"activePhysicsBodiesAfterRelease\":{}}}",runId_,EnvironmentPartitionId,environmentCollisionBeforeRelease.stateHash,environmentCollisionBeforeRelease.registeredDescriptors,environmentCollisionBeforeRelease.solidDescriptors,environmentCollisionBeforeRelease.nonSolidDescriptors,environmentCollisionBeforeRelease.activePartitions,environmentCollisionBeforeRelease.activeColliders,environmentCollisionBeforeRelease.activePhysicsBodies,environmentCollisionBeforeRelease.activations,environmentCollisionBeforeRelease.deactivations,environmentCollisionBeforeRelease.failedActivations,environmentCollisionBeforeRelease.rollbacks,environmentStableIdEvidence_?1:0,environmentRayEvidence_?1:0,environmentMeleeQueryEvidence_?1:0,environmentCollisionReloadStable_?1:0,environmentCollisionRelease.descriptorsReleased,environmentCollisionRelease.collidersReleased,environmentCollisionRelease.physicsBodiesReleased,environmentCollisionRelease.retainedBytesAfter,environmentCollisionAfterRelease.activeColliders,environmentCollisionAfterRelease.activePhysicsBodies);
  }
  const auto showcaseRendererAfter=engine.GetRendererStats();
  const auto showcaseExecution=showcaseEnvironment_.ExecutionReceipt(showcaseRendererBaseline_,showcaseRendererAfter);
  showcaseExecutionObserved_=showcaseExecution.executed;
  std::uint32_t heroGroundCover{},heroGroundCoverQuadrantMask{};
  for(const auto& descriptor:showcaseEnvironment_.Descriptors()){
    if(descriptor.kind!=ShowcaseSceneKind3D::Grass)continue;
    const Vec3 delta=descriptor.position-Vec3{-16,0,8};
    if(delta.x*delta.x+delta.z*delta.z>22.f*22.f)continue;
    ++heroGroundCover;
    heroGroundCoverQuadrantMask|=1u<<((delta.z>=0?2u:0u)+(delta.x>=0?1u:0u));
  }
  const auto showcaseReceipt=showcaseEnvironment_.Receipt();
  const auto showcaseRelease=showcaseEnvironment_.Release();
  if(!captureDirectory_.empty()){
    const auto productionSubmission=showcaseRenderAssets_.ProductionSubmissionStats();
    std::ofstream showcaseOut(captureDirectory_/"gameplay-environment-receipt.json",std::ios::trunc);
    showcaseOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    showcaseOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayShowcaseEnvironment3D\",\"worldHash\":"<<showcaseReceipt.worldHash<<",\"descriptors\":"<<(showcaseReceipt.terrain+showcaseReceipt.water+showcaseReceipt.vegetation+showcaseReceipt.rocks+showcaseReceipt.buildings)<<",\"submittedDescriptors\":"<<showcaseSubmittedDescriptors_<<",\"terrainPatches\":"<<showcaseReceipt.terrain<<",\"terrainSpanMeters\":"<<showcaseReceipt.terrainSpanMeters<<",\"waterSegments\":"<<showcaseReceipt.water<<",\"riverLengthMeters\":"<<showcaseReceipt.riverLengthMeters<<",\"riverSurfaceAreaSquareMeters\":"<<showcaseReceipt.riverSurfaceAreaSquareMeters<<",\"treeInstances\":"<<showcaseReceipt.trees<<",\"grassInstances\":"<<showcaseReceipt.grass<<",\"nearVegetation\":"<<showcaseReceipt.nearVegetation<<",\"midVegetation\":"<<showcaseReceipt.midVegetation<<",\"farVegetation\":"<<showcaseReceipt.farVegetation<<",\"heroGroundCover\":"<<heroGroundCover<<",\"heroGroundCoverQuadrantMask\":"<<heroGroundCoverQuadrantMask<<",\"rockInstances\":"<<showcaseReceipt.rocks<<",\"buildingComponents\":"<<showcaseReceipt.buildings<<",\"buildingComponentMask\":"<<showcaseReceipt.buildingComponentMask<<",\"materialIdentityHash\":"<<showcaseReceipt.materialIdentityHash<<",\"licenseInventoryHash\":"<<showcaseReceipt.licenseInventoryHash<<",\"executionObserved\":"<<(showcaseExecution.executed?1:0)<<",\"counterDeltas\":[";
    for(std::size_t index=0;index<showcaseExecution.deltas.size();++index){if(index)showcaseOut<<',';showcaseOut<<showcaseExecution.deltas[index];}
    showcaseOut<<"],\"cameraTargets\":[";
    for(std::size_t index=0;index<Names.size();++index){
      if(index)showcaseOut<<',';
      const auto& visibility=captureEnvironmentVisibility_[index];
      const bool requireHero=GameplayCaptureExpectations3D[index].camera!=GameplayCaptureCamera3D::First;
      showcaseOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"visibleRiverSegments\":"<<visibility.visibleRiverSegments<<",\"riverCoverage\":"<<visibility.riverCoverage<<",\"visibleBuildingComponents\":"<<visibility.visibleBuildingComponents<<",\"buildingCoverage\":"<<visibility.buildingCoverage<<",\"buildingTouchesEdge\":"<<(visibility.buildingTouchesEdge?1:0)<<",\"buildingEdgeMask\":"<<visibility.buildingEdgeMask<<",\"required\":"<<(requireHero?1:0)<<",\"pass\":"<<(visibility.Meets(requireHero,requireHero)?1:0)<<",\"lodTransitions\":[";
      bool firstTransition=true;
      for(const auto& sample:captureEnvironmentLodSamples_[index])if(sample.transitioning){if(!firstTransition)showcaseOut<<',';firstTransition=false;showcaseOut<<"{\"stableLow\":"<<sample.stableLow<<",\"kind\":"<<unsigned(sample.kind)<<",\"centerDistance\":"<<sample.centerDistance<<",\"boundsRadius\":"<<sample.boundsRadius<<",\"surfaceDistance\":"<<sample.surfaceDistance<<",\"switchDistance\":"<<sample.switchDistance<<",\"transitionWidth\":"<<sample.transitionWidth<<",\"nearLevel\":"<<sample.nearLevel<<",\"farLevel\":"<<sample.farLevel<<",\"nearCoverage\":"<<sample.nearCoverage<<",\"farCoverage\":"<<sample.farCoverage<<'}';}
      showcaseOut<<"]}";
    }
    showcaseOut<<"],\"terrainGroundingEligible\":"<<showcaseReceipt.terrainGroundingEligible<<",\"terrainGroundedDescriptors\":"<<showcaseReceipt.terrainGroundedDescriptors<<",\"terrainGroundingMaximumError\":"<<showcaseReceipt.terrainGroundingMaximumError<<",\"terrainGroundingApplied\":"<<(showcaseReceipt.terrainGroundingApplied?1:0)<<",\"normalizedAssets\":"<<productionSubmission.normalizedAssets<<",\"normalizationRejected\":"<<productionSubmission.normalizationRejected<<",\"meshChainsSubmitted\":"<<productionSubmission.meshChainsSubmitted<<",\"meshChainsRejected\":"<<productionSubmission.meshChainsRejected<<",\"releasedDescriptors\":"<<showcaseRelease.descriptorsReleased<<",\"retainedBytesAfterRelease\":"<<showcaseRelease.retainedBytesAfter<<"}";
  }
  // typed LOD證據寫出後立即釋放每景向量容量，避免長時間工具執行保留記憶體。
  for(auto& samples:captureEnvironmentLodSamples_)std::vector<::GameplayLab::ShowcaseLodSample3D>{}.swap(samples);
  physics_.Clear();
  collisions_.Clear();
  scene_.SetBoxMesh({});
  playerHistory_.DiscardSubmitted();
  monsterHistory_.DiscardSubmitted();
  playerHistory_.Invalidate();
  monsterHistory_.Invalidate();
  femaleMaterialLibrary_.Shutdown();
  for(auto& library:candidate7MaterialLibraries_)library.Shutdown();
  for(auto& mesh:candidate7AttachmentMeshes_)engine.DestroySkinnedMesh3D(mesh.handle);
  maleMaterialLibrary_.Shutdown();
  engine.DestroySkinnedMesh3D(femaleMesh_.handle);
  engine.DestroySkinnedMesh3D(maleMesh_.handle);
  engine.DestroySkinnedMesh3D(femaleFirstPersonArmsMesh_.handle);
  engine.DestroySkinnedMesh3D(maleFirstPersonArmsMesh_.handle);
  engine.DestroySkinnedMesh3D(femaleWeaponMesh_.handle);
  engine.DestroySkinnedMesh3D(maleWeaponMesh_.handle);
  const auto productionRelease=showcaseRenderAssets_.ReleaseProduction(engine,scene_);
  if(!captureDirectory_.empty()){
    const auto evidenceScope=temporalFixtureWarmupFrames_?GameplayCaptureEvidenceScope3D::ThirdDayTargeted:GameplayCaptureEvidenceScope3D::Full;
    if(!ValidateGameplayCaptureEvidenceScope3D(evidenceScope,captureSequenceReceipts_))throw std::runtime_error("Gameplay capture evidence scope exact-set failed at receipt publication");
    if(evidenceScope==GameplayCaptureEvidenceScope3D::ThirdDayTargeted){const auto& receipt=captureSequenceReceipts_[1];for(const auto& phase:receipt.phases){if(phase.phaseIndex>=captureIntegrityDiagnostics_[1].size())throw std::runtime_error("Gameplay targeted integrity phase was out of range");const auto& diagnostic=captureIntegrityDiagnostics_[1][phase.phaseIndex];if(!diagnostic.recorded||diagnostic.objectId!=phase.determinism||diagnostic.depth!=phase.determinism)throw std::runtime_error("Gameplay targeted sequence/integrity cross-receipt drifted");}return;}
    std::ofstream integrityOut(captureDirectory_/"gameplay-determinism-integrity-receipt.json",std::ios::trunc);integrityOut<<std::setprecision(std::numeric_limits<float>::max_digits10);integrityOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabDeterminismIntegrity\",\"captures\":[";for(std::size_t integrityIndex=0;integrityIndex<Names.size();++integrityIndex){if(integrityIndex)integrityOut<<',';integrityOut<<"{\"name\":\""<<std::filesystem::path(Names[integrityIndex]).string()<<"\"";const auto& frameRender=captureSequenceReceipts_[integrityIndex].phases[1].determinism.render;WriteIntegrityJson(integrityOut,captureDeterminismFrameSerial_[integrityIndex],captureDeterminismIntegrity_[integrityIndex],captureCanonicalDrawFingerprint_[integrityIndex],&frameRender);integrityOut<<'}';}integrityOut<<"]}";integrityOut.close();
    const bool objectIdRequested=std::ranges::all_of(captureObjectIdGeneration_,[](auto value){return value!=0;});
    const bool objectIdOpaque=std::ranges::all_of(captureObjectIdOpaqueComplete_,[](auto value){return value!=0;});
    const bool objectIdComplete=objectIdOpaque&&std::ranges::all_of(captureObjectIdMaskedComplete_,[](auto value){return value!=0;})&&std::ranges::all_of(captureObjectIdSkinnedComplete_,[](auto value){return value!=0;});
    const bool depthCaptureSupported=std::ranges::all_of(captureDepthSupported_,[](auto value){return value!=0;});
    const bool depthPass=depthCaptureSupported&&captureGpuFeetContactPassed_[1]&&captureGpuFeetContactPassed_[4]&&captureGpuFeetContactPassed_[7]&&captureGpuFeetContactPassed_[9]&&captureGpuFeetContactPassed_[10];
    const std::size_t actualCaptureIndex=captureIndex_,completedCaptures=std::ranges::count_if(captureSequenceReceipts_,[](const auto& value){return value.Complete();});
    std::unordered_set<std::wstring> uniqueCaptureNames;
    bool semanticPass=actualCaptureIndex==Names.size()&&completedCaptures==Names.size()&&objectIdComplete&&showcaseRendererAfter.rejectedFrameCaptures==showcaseRendererBaseline_.rejectedFrameCaptures,shadowPass=semanticPass,lodPass=semanticPass;
    for(std::size_t index=0;index<Names.size();++index){
      const auto expectedPath=(captureDirectory_/GameplayCaptureExpectations3D[index].name).lexically_normal();const bool actualNameMatches=captureRequestedEffectiveNames_[index]==GameplayCaptureExpectations3D[index].name&&captureCommittedEffectiveNames_[index]==captureRequestedEffectiveNames_[index]&&captureRequestedEffectivePaths_[index]==expectedPath&&captureCommittedEffectivePaths_[index]==captureRequestedEffectivePaths_[index];if(index<actualCaptureIndex&&!captureCommittedEffectiveNames_[index].empty())uniqueCaptureNames.insert(captureCommittedEffectiveNames_[index]);
      const auto& sequence=captureSequenceReceipts_[index];const bool effectivePhase=sequence.Complete()&&sequence.phases.size()>1&&sequence.phases[1].phaseIndex==1;const std::uint64_t effectiveFrame=effectivePhase?sequence.phases[1].raster.frameSerial:0;
      semanticPass=semanticPass&&actualNameMatches&&captureSemanticExpectationPassed_[index]&&!captureSemanticGenerationConflicts_[index]&&captureSemanticFrameSerial_[index]==effectiveFrame&&captureEffectiveRasterProvenance_[index].frameSerial==effectiveFrame&&captureSceneSubmissionSerial_[index]&&captureSceneDrawRevision_[index]==(effectivePhase?sequence.phases[1].metadata.drawListRevision:0)&&captureSceneIdentityOrderFingerprint_[index];
      shadowPass=shadowPass&&captureShadowEvidencePassed_[index]&&captureShadowFrameSerial_[index]==effectiveFrame;
      lodPass=lodPass&&captureLodEvidencePassed_[index]&&captureLodFrameSerial_[index]==effectiveFrame;
    }
    semanticPass=semanticPass&&uniqueCaptureNames.size()==Names.size();
    const std::uint64_t lodInstances=std::accumulate(captureLodInstances_.begin(),captureLodInstances_.end(),std::uint64_t{}),lodTransitions=std::accumulate(captureLodTransitions_.begin(),captureLodTransitions_.end(),std::uint64_t{}),lodDual=std::accumulate(captureLodDualSubmissions_.begin(),captureLodDualSubmissions_.end(),std::uint64_t{}),lodFallbacks=std::accumulate(captureLodBudgetFallbacks_.begin(),captureLodBudgetFallbacks_.end(),std::uint64_t{});
    std::ofstream typedProductionOut(captureDirectory_/"gameplay-photoreal-receipt.json",std::ios::trunc);
    typedProductionOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    typedProductionOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayPhotorealTyped\",\"expectedCaptures\":"<<Names.size()<<",\"actualCaptureIndex\":"<<actualCaptureIndex<<",\"completedCaptures\":"<<completedCaptures<<",\"uniqueCaptureNames\":"<<uniqueCaptureNames.size()<<",\"manifestSha256\":\""<<showcaseProductionReceipt_.manifestSha256<<"\",\"payloadFiles\":"<<showcaseProductionReceipt_.payloadFiles<<",\"validatedPayloadFiles\":"<<showcaseProductionReceipt_.validatedPayloadFiles<<",\"profileAssets\":"<<showcaseProductionReceipt_.profileAssets<<",\"materials\":"<<showcaseProductionReceipt_.materialSets<<",\"validatedMaterials\":"<<showcaseProductionReceipt_.validatedMaterialSets<<",\"meshParts\":"<<showcaseProductionReceipt_.meshParts<<",\"validatedMeshParts\":"<<showcaseProductionReceipt_.validatedMeshParts<<",\"validatedReferencedFiles\":"<<showcaseProductionReceipt_.validatedReferencedFiles<<",\"objectIdSupported\":"<<unsigned(objectIdRequested)<<",\"objectIdRequested\":"<<unsigned(objectIdRequested)<<",\"objectIdPass\":"<<unsigned(objectIdComplete)<<",\"semanticPass\":"<<unsigned(semanticPass)<<",\"depthCaptureSupported\":"<<unsigned(depthCaptureSupported)<<",\"depthPass\":"<<unsigned(depthPass)<<",\"shadowPass\":"<<unsigned(shadowPass)<<",\"lodPass\":"<<unsigned(lodPass)<<",\"shadowCasters\":"<<std::accumulate(captureShadowCasters_.begin(),captureShadowCasters_.end(),std::uint64_t{})<<",\"shadowDrawCalls\":"<<std::accumulate(captureShadowDrawCalls_.begin(),captureShadowDrawCalls_.end(),std::uint64_t{})<<",\"shadowCascadeDrawCalls\":"<<std::accumulate(captureShadowCascadeDrawCalls_.begin(),captureShadowCascadeDrawCalls_.end(),std::uint64_t{})<<",\"lodInstances\":"<<lodInstances<<",\"lodTransitions\":"<<lodTransitions<<",\"lodDualSubmissions\":"<<lodDual<<",\"lodBudgetFallbacks\":"<<lodFallbacks<<",\"captures\":[";
    for(std::size_t index=0;index<Names.size();++index){
      if(index)typedProductionOut<<',';const auto& expectation=GameplayCaptureExpectations3D[index];
      typedProductionOut<<"{\"name\":\""<<std::filesystem::path(expectation.name).string()<<"\",\"requestedName\":\""<<std::filesystem::path(captureRequestedEffectiveNames_[index]).string()<<"\",\"committedName\":\""<<std::filesystem::path(captureCommittedEffectiveNames_[index]).string()<<"\",\"requestedPath\":\""<<captureRequestedEffectivePaths_[index].generic_string()<<"\",\"committedPath\":\""<<captureCommittedEffectivePaths_[index].generic_string()<<"\",\"camera\":\""<<CaptureCameraName(expectation.camera)<<"\",\"lighting\":\""<<CaptureLightingName(expectation.lighting)<<"\",\"requiredIdentityCount\":"<<expectation.requiredIdentityCount<<",\"requiredMatched\":"<<captureSemanticRequiredMatched_[index]<<",\"forbiddenIdentityCount\":"<<expectation.forbiddenIdentityCount<<",\"forbiddenMatched\":"<<captureSemanticForbiddenMatched_[index]<<",\"generationConflicts\":"<<captureSemanticGenerationConflicts_[index]<<",\"semanticPass\":"<<unsigned(captureSemanticExpectationPassed_[index])<<",\"effectiveFrameSerial\":"<<captureEffectiveRasterProvenance_[index].frameSerial<<",\"semanticFrameSerial\":"<<captureSemanticFrameSerial_[index]<<",\"sceneSubmissionSerial\":"<<captureSceneSubmissionSerial_[index]<<",\"sceneDrawRevision\":"<<captureSceneDrawRevision_[index]<<",\"sceneIdentityOrderFingerprint\":"<<captureSceneIdentityOrderFingerprint_[index]<<",\"shadowFrameSerial\":"<<captureShadowFrameSerial_[index]<<",\"shadowResourceGeneration\":"<<captureShadowProvenance_[index].resourceGeneration<<",\"shadowCasters\":"<<captureShadowCasters_[index]<<",\"shadowDrawCalls\":"<<captureShadowDrawCalls_[index]<<",\"shadowCascadeDrawCalls\":"<<captureShadowCascadeDrawCalls_[index]<<",\"shadowCascadeRendered\":"<<captureShadowCascadeRendered_[index]<<",\"shadowCascadeReused\":"<<captureShadowCascadeReused_[index]<<",\"shadowActiveCascades\":"<<captureShadowActiveCascades_[index]<<",\"shadowCascadeFingerprints\":[";for(std::size_t cascade=0;cascade<captureShadowProvenance_[index].cascadeFingerprints.size();++cascade){if(cascade)typedProductionOut<<',';typedProductionOut<<captureShadowProvenance_[index].cascadeFingerprints[cascade];}typedProductionOut<<"],\"shadowPass\":"<<unsigned(captureShadowEvidencePassed_[index])<<",\"lodFrameSerial\":"<<captureLodFrameSerial_[index]<<",\"lodInstances\":"<<captureLodInstances_[index]<<",\"lodTransitions\":"<<captureLodTransitions_[index]<<",\"lodDualSubmissions\":"<<captureLodDualSubmissions_[index]<<",\"lodBudgetFallbacks\":"<<captureLodBudgetFallbacks_[index]<<",\"lodPass\":"<<unsigned(captureLodEvidencePassed_[index])<<",\"requiredIdentities\":[";
      for(std::size_t required=0;required<expectation.requiredIdentityCount;++required){if(required)typedProductionOut<<',';const auto identity=expectation.requiredIdentities[required];typedProductionOut<<"{\"high\":"<<identity.id.high<<",\"low\":"<<identity.id.low<<",\"generation\":"<<identity.generation<<",\"nonzeroPixels\":"<<captureSemanticRequiredPixels_[index][required]<<",\"coverage\":"<<captureSemanticRequiredCoverage_[index][required]<<'}';}
      typedProductionOut<<"],\"forbiddenIdentities\":[";for(std::size_t forbidden=0;forbidden<expectation.forbiddenIdentityCount;++forbidden){if(forbidden)typedProductionOut<<',';const auto identity=expectation.forbiddenIdentities[forbidden];typedProductionOut<<"{\"high\":"<<identity.id.high<<",\"low\":"<<identity.id.low<<",\"generation\":"<<identity.generation<<'}';}typedProductionOut<<"],\"projectionMode\":\""<<CaptureProjectionName(ProjectionMode(captureProjectionMode_[index]))<<"\",\"projectionModeValue\":"<<unsigned(captureProjectionMode_[index])<<",\"verticalFovRadians\":"<<captureVerticalFovRadians_[index]<<",\"orthographicHeight\":"<<captureOrthographicHeight_[index]<<",\"godActualTriangleCoverage\":"<<captureGodActualTriangleCoverage_[index]<<",\"godVisibleWorldDescriptors\":"<<captureGodVisibleWorldDescriptors_[index]<<",\"godBuildingUnion\":"<<captureGodBuildingUnion_[index]<<",\"godMaximumBuildingPart\":"<<captureGodMaximumBuildingPart_[index]<<",\"godRoofCoverage\":"<<captureGodRoofCoverage_[index]<<",\"godRiverCoverage\":"<<captureGodRiverCoverage_[index]<<",\"godVisibleBuildingComponents\":"<<captureGodVisibleBuildingComponents_[index]<<",\"godBuildingEdgeMask\":"<<captureGodBuildingEdgeMask_[index]<<",\"godPredictedLodTransitions\":"<<captureGodLodTransitions_[index]<<",\"godActualLodTransitions\":"<<captureLodTransitions_[index]<<",\"godActualLodDualSubmissions\":"<<captureLodDualSubmissions_[index]<<",\"godLodPredictionMatched\":"<<unsigned(captureLodPredictionMatched_[index])<<",\"godLodContractMissing\":"<<captureLodContracts_[index].missing<<",\"godLodContractExtra\":"<<captureLodContracts_[index].extra<<",\"godLodContractWrongLevel\":"<<captureLodContracts_[index].wrongLevel<<",\"godLodContractWrongBlend\":"<<captureLodContracts_[index].wrongBlend<<",\"godTransitionRows\":[";bool firstLodRow=true;for(const auto& row:captureSubmittedLodRows_[index])if(row.transitioning){if(!firstLodRow)typedProductionOut<<',';firstLodRow=false;const auto actual=std::ranges::find_if(captureActualLodRows_[index],[&](const auto& value){return value.identity.stableLow==row.identity.stableLow&&value.identity.generation==row.identity.generation&&value.identity.submesh==row.identity.submesh;});typedProductionOut<<"{\"stableLow\":"<<row.identity.stableLow<<",\"generation\":"<<row.identity.generation<<",\"submesh\":"<<row.identity.submesh<<",\"kind\":"<<unsigned(row.identity.kind)<<",\"nearLevel\":"<<row.nearLevel<<",\"farLevel\":"<<row.farLevel<<",\"centerDistance\":"<<row.centerDistance<<",\"boundsRadius\":"<<row.boundsRadius<<",\"surfaceDistance\":"<<row.surfaceDistance<<",\"switchDistance\":"<<row.switchDistance<<",\"transitionWidth\":"<<row.transitionWidth<<",\"lodSeed\":"<<row.lodSeed<<",\"nearCoverage\":"<<row.nearCoverage<<",\"farCoverage\":"<<row.farCoverage<<",\"actualNearCoverage\":"<<(actual==captureActualLodRows_[index].end()?-1.f:actual->nearCoverage)<<",\"actualFarCoverage\":"<<(actual==captureActualLodRows_[index].end()?-1.f:actual->farCoverage)<<'}';}typedProductionOut<<"]}";
    }
    typedProductionOut<<"]}";
    std::ofstream productionOut(captureDirectory_/"gameplay-photoreal-legacy-diagnostic.json",std::ios::trunc);
    productionOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    // masked/skinned仍有任何缺口時，整體GPU object-ID gate必須維持fail-closed。
    productionOut<<std::format("{{\"source\":\"GameplayPhotorealCooked\",\"profileSource\":\"runtime-render-manifest\",\"manifestSha256\":\"{}\",\"manifestFingerprint\":{},\"packageBytes\":{},\"payloadFiles\":{},\"profileAssets\":{},\"materials\":{},\"meshParts\":{},\"meshLodLevels\":{},\"iblFiles\":{},\"photorealActive\":{},\"objectIdSupported\":1,\"objectIdRequested\":{},\"objectIdPass\":{},\"semanticPass\":{},\"depthCaptureSupported\":{},\"depthPass\":{},\"shadowPass\":0,\"lodPass\":0,\"leasesReleased\":{},\"meshesReleased\":{},\"materialsReleased\":{},\"rendererBaselineRestored\":{},\"baselineMeshes\":{},\"releasedMeshes\":{},\"baselineMeshReferences\":{},\"releasedMeshReferences\":{},\"baselineMaterials\":{},\"releasedMaterials\":{},\"baselineMaterialReferences\":{},\"releasedMaterialReferences\":{},\"baselineIbl\":{},\"releasedIbl\":{},\"pendingGpuBytes\":{},\"pendingIblBytes\":{}}}",showcaseProductionReceipt_.manifestSha256,showcaseProductionReceipt_.manifestFingerprint,showcaseProductionReceipt_.packageBytes,showcaseProductionReceipt_.payloadFiles,showcaseProductionReceipt_.profileAssets,showcaseProductionReceipt_.materialSets,showcaseProductionReceipt_.meshParts,showcaseProductionReceipt_.meshLodLevels,showcaseProductionReceipt_.iblFiles,showcaseProductionReceipt_.photorealActive?1:0,objectIdRequested?1:0,objectIdComplete?1:0,objectIdComplete?1:0,depthCaptureSupported?1:0,depthPass?1:0,productionRelease.leasesReleased,productionRelease.meshesReleased,productionRelease.materialsReleased,productionRelease.rendererBaselineRestored?1:0,productionRelease.baselineMeshes,productionRelease.releasedMeshes,productionRelease.baselineMeshReferences,productionRelease.releasedMeshReferences,productionRelease.baselineMaterials,productionRelease.releasedMaterials,productionRelease.baselineMaterialReferences,productionRelease.releasedMaterialReferences,productionRelease.baselineIbl,productionRelease.releasedIbl,productionRelease.pendingGpuBytes,productionRelease.pendingIblBytes);
  }
  engine.DestroyMesh3D(boxMesh_.handle);
  engine.DestroyMesh3D(sphereMesh_.handle);
  for(auto* mesh:{&showcaseTerrainMesh_,&showcaseRiverMesh_,&showcaseCottageFrontMesh_,&showcaseCottageBackMesh_,&showcaseCottageSideMesh_,&showcaseCottageRoofMesh_,&showcaseCylinderMesh_,&showcaseConeHighMesh_,&showcaseConeLowMesh_,&showcaseGrassHighMesh_,&showcaseGrassLowMesh_,&showcaseFernHighMesh_,&showcaseFernLowMesh_,&showcaseShrubHighMesh_,&showcaseShrubLowMesh_,&showcaseTreeLowMesh_,&showcaseRockLowMesh_,&showcaseQuiverTreeMesh_,&showcaseRock09Mesh_}){engine.DestroyMesh3D(mesh->handle);*mesh={};}
  showcaseModelMaterialLibrary_.Shutdown();showcaseMaterialLibrary_.Shutdown();
  femaleMesh_ = {};
  maleMesh_ = {};
  femaleFirstPersonArmsMesh_ = {};
  maleFirstPersonArmsMesh_ = {};
  femaleWeaponMesh_ = {};
  maleWeaponMesh_ = {};
  femaleSwordGeometry_={};maleSwordGeometry_={};
  femaleWeaponProjection_={};maleWeaponProjection_={};
  femaleModel_ = {};
  maleModel_ = {};
  femaleFirstPersonArmsModel_={};maleFirstPersonArmsModel_={};
  femaleFirstPersonArmsEvidence_={};maleFirstPersonArmsEvidence_={};characterMaterialEvidence_={};
  femaleSubmeshes_.clear();maleSubmeshes_.clear();femaleFirstPersonArmsSubmeshes_.clear();maleFirstPersonArmsSubmeshes_.clear();femaleWeaponSubmeshes_.clear();maleWeaponSubmeshes_.clear();femaleMaterials_.clear();maleMaterials_.clear();
  boxMesh_ = {};
  sphereMesh_ = {};
  skinning_.TrimUnusedMemory();
  TrimReusableMemory();
  if(!captureDirectory_.empty()){
    engine.TrimUnusedMemory();
    const auto renderer=engine.GetRendererStats();
    const auto memory=engine.GetMemoryStats();
    std::ofstream releaseOut(captureDirectory_/"gameplay-character-release-receipt.json",std::ios::trunc);
    releaseOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    releaseOut<<std::format("{{\"runId\":\"{}\",\"source\":\"GameplayLabCharacterRelease\",\"sourceFingerprint\":\"{}\",\"residentSkinnedMeshes\":{},\"residentSkinnedMeshBytes\":{},\"residentMaterialTextureSets\":{},\"residentMaterialTextureReferences\":{},\"skinnedMeshReleases\":{},\"engineTrimCount\":{},\"gameTrimCount\":{},\"gameRetainedBytes\":{},\"gameReclaimableBytes\":{}}}",runId_,actualSourceFingerprint_,renderer.residentSkinnedMeshes,renderer.residentSkinnedMeshBytes,renderer.residentMaterialTextureSets,renderer.residentMaterialTextureReferences,renderer.skinnedMeshReleases,memory.trimCount,memory.game.trimCount,memory.game.retainedCapacityBytes,memory.game.reclaimableCapacityBytes);
  }
  if(!projectionFailureReason_.empty()){
    // 即使 WM_QUIT 越過下一幀 callback，已鎖存的失敗仍必須以非零結束。
    projectionFailureStopFinished_=true;
    FinishProjectionFailure(engine);
    throw std::runtime_error(projectionFailureReason_);
  }
}
void GameplayLabGame::QueueTemporalHistoryReseed(GameplayTemporalReseedReason3D reason)noexcept{
  const auto mask=GameplayTemporalReasonMask3D(reason);if(!mask)return;
  temporalHistoryCutPending_=true;temporalHistoryPendingReasonMask_|=mask;
  if(captureTemporalWarmupActive_){captureTemporalReseedReasonCurrent_|=mask;captureTemporalWarmupRemaining_=temporalFixtureWarmupFrames_?temporalFixtureWarmupFrames_:GameplayTemporalCaptureWarmupFrames3D;}
}
void GameplayLabGame::ResetProductionCameraCut(GameplayTemporalReseedReason3D reason)noexcept{cameraOcclusionPolicy_.Reset();cameraBuildCacheValid_=false;cameraFadeSettled_=false;QueueTemporalHistoryReseed(reason);}
void GameplayLabGame::UpdateCamera(Engine& engine) {
  const Camera3D previousCamera=camera_;const bool hadTemporalCamera=temporalCameraInitialized_;
  const Vec3 feet = controller_.Position(), focus = feet + Vec3{0, 1.1f, 0};
  cameraOccluderCoverage_=0;cameraRawOccluderCoverage_=0;cameraOccluderStableLow_=0;cameraPolicyFadeIdentity_={};cameraFadeSettled_=false;cameraOcclusionAdjusted_=false;
  cameraBuilderReceipt_={};cameraPolicyReceipt_={};cameraSelectedCandidate_={};cameraBuilderExpected_={};cameraBuilderMatrixExact_=false;
  switch (rules_.Camera()) {
  case CameraMode::FirstPerson:
    cameraOcclusionPolicy_.Reset();
    camera_ = MakeFirstPersonCamera(
        feet + Vec3{0, rules_.CharacterHeight() - .12f, 0}, yaw_, pitch_);
    break;
  case CameraMode::ThirdPerson:
    {
      constexpr float desiredArm=4.5f,sweepRadius=.35f,wallPadding=.12f;
      const Vec2 cameraViewport{float((std::max)(1u,engine.RenderWidth())),float((std::max)(1u,engine.RenderHeight()))};
      // 每個互動視角使用自己的正式投影；不得繼承前一個God capture的窄FOV。
      CameraProjection cameraProjection{};
      std::vector<::GameplayLab::GameplayOccluderBox3D> buildingOccluders;buildingOccluders.reserve(6+rules_.Entities().size());std::uint64_t descriptorLow{};
      for(const auto& descriptor:showcaseEnvironment_.Descriptors()){++descriptorLow;if(descriptor.kind==ShowcaseSceneKind3D::Building)buildingOccluders.push_back({descriptorLow,descriptor.position,descriptor.scale,descriptor.yaw});}
      // Runtime palette buildings are authoritative world occluders too.  If
      // they are omitted here a valid camera candidate can be chosen directly
      // behind a newly placed building even though collision shortening keeps
      // the camera itself outside the collider.
      std::uint64_t paletteOccluderHash=14695981039346656037ull;
      const auto mixPaletteOccluder=[&](std::uint64_t value){for(unsigned byte=0;byte<8;++byte){paletteOccluderHash^=std::uint8_t(value>>(byte*8));paletteOccluderHash*=1099511628211ull;}};
      for(const auto& placed:rules_.Entities()){
        // Initial physics-stack fixtures (1..18) are deliberate test clutter,
        // not user-authored palette content.  Only replay/live additions can
        // invalidate the presentation camera after its candidate cache exists.
        if(placed.id<=18||(!captureDirectory_.empty()&&captureIndex_==9))continue;
        const auto* body=static_cast<const PhysicsWorld3D&>(physics_).GetBody(palette_->Body(placed.id));
        if(!body)continue;
        const Vec3 size=body->settings.colliderExtents*2.f;
        const auto& transform=body->settings.transform;
        buildingOccluders.push_back({0x100000000ull+placed.id,transform.position,size,transform.rotation.y});
        mixPaletteOccluder(placed.id);
        for(float value:{transform.position.x,transform.position.y,transform.position.z,size.x,size.y,size.z,transform.rotation.y})
          mixPaletteOccluder(std::bit_cast<std::uint32_t>(value==0.f?0.f:value));
      }
      const SkinnedModel3D& cameraSubjectModel=rules_.Female()?femaleModel_:maleModel_;
      const AnimationPose3D& cameraSubjectPose=rules_.Female()?femalePose_:malePose_;
      if(!SkinModelInto(cameraSubjectModel,cameraSubjectPose,skinning_,cameraPoseVisualMesh_)){
        cameraOcclusionPolicy_.Reset();cameraBuildCacheValid_=false;camera_=MakeFirstPersonCamera(feet+Vec3{0,rules_.CharacterHeight()-.12f,0},yaw_,pitch_);break;
      }
      MeshBounds3D currentVisualBounds=cameraPoseVisualMesh_.Bounds();
      if(candidate7Active_&&rules_.Female())currentVisualBounds=TransformBounds(currentVisualBounds,
        {{0,candidate7PresentationScale_.modelOriginWorldYOffset,0},{},candidate7PresentationScale_.ScaleVector()});
      const auto mergeBounds=[](MeshBounds3D left,MeshBounds3D right){const Vec3 minimum{(std::min)(left.center.x-left.extents.x,right.center.x-right.extents.x),(std::min)(left.center.y-left.extents.y,right.center.y-right.extents.y),(std::min)(left.center.z-left.extents.z,right.center.z-right.extents.z)},maximum{(std::max)(left.center.x+left.extents.x,right.center.x+right.extents.x),(std::max)(left.center.y+left.extents.y,right.center.y+right.extents.y),(std::max)(left.center.z+left.extents.z,right.center.z+right.extents.z)},center=(minimum+maximum)*.5f,extents=(maximum-minimum)*.5f;return MeshBounds3D{center,Length(extents),extents};};
      const auto boundsChanged=[](MeshBounds3D left,MeshBounds3D right){return Length(left.center-right.center)>.0001f||Length(left.extents-right.extents)>.0001f;};
      const auto playerCameraAnimation=(rules_.Female()?femaleAnimation_:maleAnimation_).State(),companionCameraAnimation=monsterAnimation_.State();
      MeshBounds3D playerEnvelope=currentVisualBounds;if(cameraBuildCacheValid_&&playerCameraAnimation==cameraBuildCachePlayerAnimation_)playerEnvelope=mergeBounds(cameraBuildCacheVisualBounds_,currentVisualBounds);
      std::array<Vertex3D,8> playerProjectionHull{};for(unsigned corner=0;corner<8;++corner){auto& vertex=playerProjectionHull[corner];vertex.x=playerEnvelope.center.x+(corner&1?playerEnvelope.extents.x:-playerEnvelope.extents.x);vertex.y=playerEnvelope.center.y+(corner&2?playerEnvelope.extents.y:-playerEnvelope.extents.y);vertex.z=playerEnvelope.center.z+(corner&4?playerEnvelope.extents.z:-playerEnvelope.extents.z);}
      std::array<::GameplayLab::GameplayCameraCompositionActor3D,1> compositionActors{};std::array<Vertex3D,8> companionProjectionHull{};std::size_t compositionActorCount{};MeshBounds3D companionBounds{},companionEnvelope{};Vec3 companionPresentation{};
      if(combatSpawned_){
        if(!SkinModelInto(maleModel_,monsterPose_,skinning_,cameraCompanionVisualMesh_)){
          cameraOcclusionPolicy_.Reset();cameraBuildCacheValid_=false;camera_=MakeFirstPersonCamera(feet+Vec3{0,rules_.CharacterHeight()-.12f,0},yaw_,pitch_);break;
        }
        companionPresentation=GameplayShowcaseMonsterPresentationPosition3D(combat_->Monster().position,yaw_,combat_->Monster().alive);
        const Transform3D companionTransform=GameplayShowcaseMonsterTransform3D(companionPresentation,feet);companionEnvelope=cameraCompanionVisualMesh_.Bounds();if(cameraBuildCacheValid_&&cameraBuildCacheHasCompanion_&&companionCameraAnimation==cameraBuildCacheCompanionAnimation_)companionEnvelope=mergeBounds(cameraBuildCacheCompanionBounds_,companionEnvelope);
        for(unsigned corner=0;corner<8;++corner){auto& vertex=companionProjectionHull[corner];vertex.x=companionEnvelope.center.x+(corner&1?companionEnvelope.extents.x:-companionEnvelope.extents.x);vertex.y=companionEnvelope.center.y+(corner&2?companionEnvelope.extents.y:-companionEnvelope.extents.y);vertex.z=companionEnvelope.center.z+(corner&4?companionEnvelope.extents.z:-companionEnvelope.extents.z);}
        companionBounds=TransformBounds(companionEnvelope,companionTransform);compositionActors[0]={companionBounds.center,companionBounds.extents,companionProjectionHull,CreateTransformMatrix(companionTransform.position,companionTransform.rotation,companionTransform.scale)};compositionActorCount=1;
      }
      ::GameplayLab::GameplayCameraProductionQueryAdapters3D cameraQueries{};
      cameraQueries.environmentSphereSweep=[&](Vec3 center,float radius,Vec3 displacement){::GameplayLab::CameraOcclusionSweep3D result{};if(environmentCollisions_){const auto hit=environmentCollisions_->SweepSphere(center,radius,displacement);if(hit)result={std::max(0.f,hit.distance-wallPadding),hit.object.id,hit.object.generation,true};}return result;};
      cameraQueries.collisionWorldMaxArm=[&](const Camera3D& desired,Vec3 target){return Length(ConstrainThirdPersonCamera(desired,target,collisions_,.25f).position-target);};
      cameraQueries.heightFieldMaxArm=[&](const Camera3D& desired,Vec3 target){return Length(ConstrainThirdPersonCamera(desired,target,showcaseTerrain_,.25f).position-target);};
      cameraQueries.roofOrTopBlocked=[&](const Camera3D& desired){::GameplayLab::GameplayOccluderMask3D mask{};(void)AnalyzeGameplayOccluderMask3D(desired,buildingOccluders,160,90,mask);return mask.nearPlaneClipped;};
      ::GameplayLab::GameplayCameraProductionBuildInput3D cameraBuildInput{};cameraBuildInput.bindVertices=playerProjectionHull;cameraBuildInput.feet=feet;cameraBuildInput.characterYaw=yaw_;cameraBuildInput.projection=cameraProjection;cameraBuildInput.viewport=cameraViewport;cameraBuildInput.desiredArm=desiredArm;cameraBuildInput.sweepRadius=sweepRadius;cameraBuildInput.candidateIdHigh=GameplayCameraCandidateHigh;
      const float minimumSubjectScreenArea=rules_.PaletteOpen()?.08f:.05f;
      cameraBuildInput.minimumSubjectScreenHeight=.52f;cameraBuildInput.minimumSubjectScreenArea=minimumSubjectScreenArea;
      // 物件面板展示必須保留完整角色輪廓與乾淨背景；一般遊玩仍容許少量建築入鏡並交由淡出策略處理。
      cameraBuildInput.maximumBuildingUnionCoverage=rules_.PaletteOpen()?.08f:.22f;
      cameraBuildInput.maximumBuildingPartCoverage=rules_.PaletteOpen()?.06f:.08f;
      cameraBuildInput.maximumRoofCoverage=rules_.PaletteOpen()?.04f:.05f;
      cameraBuildInput.maximumSubjectOcclusion=.005f;
      cameraBuildInput.useCleanShowcaseYawOffsets=rules_.PaletteOpen();
      cameraBuildInput.compositionOccluders=buildingOccluders;
      cameraBuildInput.compositionActors=std::span<const ::GameplayLab::GameplayCameraCompositionActor3D>{compositionActors.data(),compositionActorCount};
      const MeshBounds3D playerWorldBounds=TransformBounds(playerEnvelope,{feet,{0,yaw_,0},{1,1,1}});
      const float derivedMultiActorArm=::GameplayLab::DeriveGameplayMultiActorCameraArm3D(desiredArm,playerWorldBounds,cameraBuildInput.compositionActors);
      if(!std::isfinite(derivedMultiActorArm)||derivedMultiActorArm<desiredArm)throw std::runtime_error("Gameplay multi-actor camera arm derivation failed closed");
      cameraBuildInput.desiredArm=desiredArm;
      const bool cameraPresentationChanged=cameraBuildCacheValid_&&(rules_.Female()!=cameraBuildCacheFemale_||rules_.Crouched()!=cameraBuildCacheCrouched_);
      const bool companionBoundsChanged=compositionActorCount!=(cameraBuildCacheHasCompanion_?1u:0u)||(compositionActorCount&&Length(companionPresentation-cameraBuildCacheCompanionPosition_)>.02f);
      const bool animationChanged=playerCameraAnimation!=cameraBuildCachePlayerAnimation_||(compositionActorCount&&companionCameraAnimation!=cameraBuildCacheCompanionAnimation_);
      const bool envelopeExpanded=!cameraBuildCacheValid_||animationChanged||boundsChanged(playerEnvelope,cameraBuildCacheVisualBounds_)||(compositionActorCount&&boundsChanged(companionEnvelope,cameraBuildCacheCompanionBounds_));
      const bool rebuildCameraCandidates=envelopeExpanded||companionBoundsChanged||Length(feet-cameraBuildCacheFeet_)>.02f||std::abs(yaw_-cameraBuildCacheYaw_)>.002f||cameraViewport.x!=cameraBuildCacheViewport_.x||cameraViewport.y!=cameraBuildCacheViewport_.y||rules_.Female()!=cameraBuildCacheFemale_||rules_.Crouched()!=cameraBuildCacheCrouched_||rules_.PaletteOpen()!=cameraBuildCachePaletteOpen_||minimumSubjectScreenArea!=cameraBuildCacheMinimumSubjectScreenArea_||paletteOccluderHash!=cameraBuildCachePaletteOccluderHash_;
      if(rebuildCameraCandidates){cameraBuildCache_=::GameplayLab::BuildGameplayCameraProductionCandidates3D(cameraBuildInput,cameraQueries);cameraBuildCacheDesiredArm_=desiredArm;if(cameraBuildCache_.receipt.valid&&!cameraBuildCache_.receipt.admittedCandidates&&compositionActorCount&&derivedMultiActorArm>desiredArm+.0001f){cameraBuildInput.desiredArm=derivedMultiActorArm;cameraBuildCache_=::GameplayLab::BuildGameplayCameraProductionCandidates3D(cameraBuildInput,cameraQueries);cameraBuildCacheDesiredArm_=derivedMultiActorArm;}cameraBuildCacheFeet_=feet;cameraBuildCacheYaw_=yaw_;cameraBuildCacheViewport_=cameraViewport;cameraBuildCacheFemale_=rules_.Female();cameraBuildCacheCrouched_=rules_.Crouched();cameraBuildCachePaletteOpen_=rules_.PaletteOpen();cameraBuildCacheMinimumSubjectScreenArea_=minimumSubjectScreenArea;cameraBuildCachePaletteOccluderHash_=paletteOccluderHash;cameraBuildCacheVisualBounds_=playerEnvelope;cameraBuildCacheCompanionBounds_=companionEnvelope;cameraBuildCacheCompanionPosition_=companionPresentation;cameraBuildCachePlayerAnimation_=playerCameraAnimation;cameraBuildCacheCompanionAnimation_=companionCameraAnimation;cameraBuildCacheHasCompanion_=compositionActorCount!=0;cameraBuildCacheValid_=cameraBuildCache_.receipt.valid;}
      if(cameraPresentationChanged)cameraOcclusionPolicy_.Reset();
      const auto& cameraBuild=cameraBuildCache_;
      const float cameraSearchArm=cameraBuildCacheDesiredArm_;
      cameraBuilderReceipt_=cameraBuild.receipt;
      if(!cameraBuild.receipt.valid){cameraOcclusionPolicy_.Reset();cameraFadeAlpha_=1.f;cameraPolicyFadeIdentity_={};camera_=MakeFirstPersonCamera(feet+Vec3{0,rules_.CharacterHeight()-.12f,0},yaw_,pitch_);break;}
      ::GameplayLab::ProductionCameraOcclusionInput3D policyInput{};policyInput.subject=cameraBuild.subject;policyInput.lens=cameraBuild.lens;policyInput.candidates=cameraBuild.candidates;policyInput.desiredArm=cameraSearchArm;policyInput.deltaTime=simulationDelta_;
      const auto policy=cameraOcclusionPolicy_.StepProduction(policyInput);
      cameraPolicyReceipt_=policy;
      if(!policy.valid){if(!captureDirectory_.empty()){
        std::ofstream rejection(captureDirectory_/"gameplay-camera-policy-rejection.txt",std::ios::trunc);const auto&r=cameraBuild.receipt;
        rejection<<"capture="<<(captureIndex_<Names.size()?std::filesystem::path(Names[captureIndex_]).string():"out-of-range")
                 <<" sex="<<(rules_.Female()?"female":"male")<<" playerState="<<unsigned(playerCameraAnimation)
                 <<" companionState="<<unsigned(companionCameraAnimation)<<" playerPoseHash="<<HashPose(cameraSubjectPose)
                 <<" companionPoseHash="<<HashPose(monsterPose_)<<" feet=["<<feet.x<<','<<feet.y<<','<<feet.z<<"] yaw="<<yaw_
                 <<" companion=["<<companionPresentation.x<<','<<companionPresentation.y<<','<<companionPresentation.z<<']'
                 <<" playerBoundsCenter=["<<playerEnvelope.center.x<<','<<playerEnvelope.center.y<<','<<playerEnvelope.center.z<<']'
                 <<" playerBoundsExtents=["<<playerEnvelope.extents.x<<','<<playerEnvelope.extents.y<<','<<playerEnvelope.extents.z<<']'
                 <<" companionBoundsCenter=["<<companionBounds.center.x<<','<<companionBounds.center.y<<','<<companionBounds.center.z<<']'
                 <<" companionBoundsExtents=["<<companionBounds.extents.x<<','<<companionBounds.extents.y<<','<<companionBounds.extents.z<<"]\n"
                 <<"candidates="<<r.candidates<<" admitted="<<r.admittedCandidates<<" safe="<<r.safeCandidates<<" framed="<<r.framedCandidates<<" environment="<<r.rejectedEnvironment<<" world="<<r.rejectedCollisionWorld<<" terrain="<<r.rejectedHeightField<<" roof="<<r.rejectedRoof<<" height="<<r.rejectedHeight<<" width="<<r.rejectedWidth<<" area="<<r.rejectedArea<<" headroom="<<r.rejectedHeadroom<<" feet="<<r.rejectedFeet<<" actorBounds="<<r.rejectedActorBounds<<" actorGap="<<r.rejectedActorGap<<" buildingUnion="<<r.rejectedBuildingUnion<<" buildingPart="<<r.rejectedBuildingPart<<" roofCoverage="<<r.rejectedRoofCoverage<<" subjectOcclusion="<<r.rejectedSubjectOcclusion<<" minimumUnion="<<r.minimumBuildingUnion<<" minimumWall="<<r.minimumWallCoverage<<" minimumRoof="<<r.minimumRoofCoverage<<" minimumSubjectOcclusion="<<r.minimumSubjectOcclusion<<"\n";
        std::vector<const ::GameplayLab::ProductionCameraCandidate3D*> pareto;
        for(const auto& candidate:cameraBuild.candidates){
          const bool dominated=std::ranges::any_of(cameraBuild.candidates,[&](const auto& other){return &other!=&candidate&&other.actorMinimumMargin>=candidate.actorMinimumMargin&&other.actorGapPixels>=candidate.actorGapPixels&&other.subjectScreenHeight>=candidate.subjectScreenHeight&&other.subjectScreenArea>=candidate.subjectScreenArea&&(other.actorMinimumMargin>candidate.actorMinimumMargin||other.actorGapPixels>candidate.actorGapPixels||other.subjectScreenHeight>candidate.subjectScreenHeight||other.subjectScreenArea>candidate.subjectScreenArea);});
          if(!dominated)pareto.push_back(&candidate);
        }
        std::ranges::sort(pareto,{},[](const auto* candidate){return candidate->actorMinimumMargin*1000.f+candidate->actorGapPixels+candidate->subjectScreenHeight*100.f;});
        const std::size_t first=pareto.size()>16?pareto.size()-16:0;
        for(std::size_t index=first;index<pareto.size();++index){const auto& candidate=*pareto[index];rejection<<"pareto candidate="<<candidate.candidateId.low<<" yaw="<<candidate.yawOffsetRadians<<" pitch="<<candidate.pitchRadians<<" focusY="<<candidate.cameraFocus.y<<" arm="<<candidate.springArmDistance<<" margin="<<candidate.actorMinimumMargin<<" gap="<<candidate.actorGapPixels<<" height="<<candidate.subjectScreenHeight<<" width="<<candidate.subjectScreenWidth<<" area="<<candidate.subjectScreenArea<<" headroom="<<candidate.headroom<<" feet="<<candidate.feetOnScreen<<" actors="<<candidate.allActorsOnScreen<<" composition="<<candidate.compositionAdmitted<<"\n";}
        throw std::runtime_error("Gameplay third-person camera has no admitted production candidate");
      }cameraOcclusionPolicy_.Reset();cameraFadeAlpha_=1.f;cameraFadeSettled_=true;cameraPolicyFadeIdentity_={};cameraOcclusionAdjusted_=true;camera_=MakeFirstPersonCamera(feet+Vec3{0,rules_.CharacterHeight()-.12f,0},yaw_,pitch_);break;}
      thirdPersonOrbitOffset_=policy.yawOffsetRadians;thirdPersonDistance_=policy.arm;cameraFadeAlpha_=policy.fadeAlpha;cameraFadeSettled_=!policy.fadeActive||policy.fadeSettled;cameraPolicyFadeIdentity_=policy.fadeActive?RenderObjectIdentity3D{policy.fadeObject,policy.fadeGeneration}:RenderObjectIdentity3D{};
      cameraOcclusionAdjusted_=std::abs(policy.yawOffsetRadians)>.001f||std::abs(policy.verticalElevation)>.001f||std::abs(policy.pitchRadians-.24f)>.001f||policy.arm<cameraSearchArm-.001f||policy.fadeActive;
      cameraSelectedCandidate_=cameraBuild.candidates[policy.candidateIndex];
      const Vec3 chosenFocus=cameraSelectedCandidate_.cameraFocus;auto value=ConstrainThirdPersonCamera(MakeThirdPersonCamera(chosenFocus,yaw_+policy.yawOffsetRadians,policy.pitchRadians,policy.arm,cameraProjection),chosenFocus,collisions_,.25f);cameraBuilderExpected_=ConstrainThirdPersonCamera(value,chosenFocus,showcaseTerrain_,.25f);camera_=cameraBuilderExpected_;cameraBuilderMatrixExact_=CaptureCameraHash(camera_)==CaptureCameraHash(cameraBuilderExpected_);
      // GPU object-ID readback會在capture frame回填真像素coverage；此處不得以CPU投影冒充。
      cameraRawOccluderCoverage_=cameraOccluderCoverage_=0.f;
      cameraOccluderStableLow_=cameraPolicyFadeIdentity_.id.low;
    }
    break;
  default:
    cameraOcclusionPolicy_.Reset();
    {
      const SkinnedModel3D& godSubjectModel=rules_.Female()?femaleModel_:maleModel_;
      // 與 OnRender 共用凍結條件及姿勢選擇，暖機的即時動畫不得漂移相機權威證據。
      const bool useFrozenPose=GameplayUseFrozenPose3D(capturePoseFrozen_,temporalFixtureWarmupFrames_!=0,capturePairFrozen_,captureTemporalWarmupActive_,captureScenarioClock_.ReadyToStage());
      const auto presentationPoses=SelectGameplayPresentationPoses3D(useFrozenPose,femalePose_,malePose_,monsterPose_,captureFrozenFemalePose_,captureFrozenMalePose_,captureFrozenMonsterPose_);
      const AnimationPose3D& godSubjectPose=rules_.Female()?*presentationPoses.female:*presentationPoses.male;
      if(!SkinModelInto(godSubjectModel,godSubjectPose,skinning_,godCameraPoseVisualMesh_)||!godCameraPoseVisualMesh_.Valid())throw std::runtime_error("Gameplay god camera posed mesh unavailable");
      const Matrix4 godPlayerWorld=GameplayPlayerWorld3D(candidate7Active_,rules_.Female(),candidate7PresentationScale_,feet,yaw_);
      const GameplayGodProductionGeometry3D godGeometry{showcaseGrassHighMesh_.bounds,showcaseRiverMesh_.bounds,boxMesh_.bounds,showcaseCottageFrontMesh_.bounds,showcaseCottageBackMesh_.bounds,showcaseCottageSideMesh_.bounds,showcaseCottageRoofMesh_.bounds};
      // translated layout 仍須重跑完整世界／碰撞／地形閉包；通過後即可重新定錨，
      // 距離或角色性別本身不得迫使相同內容再次執行昂貴全域搜尋。
      const auto resolveGodCamera=[&](const GameplayGodCameraLayout3D& candidate){
        return ResolveGameplayGodCameraLayout3D(focus,candidate,showcaseEnvironment_.Descriptors(),godGeometry,
          godCameraPoseVisualMesh_.vertices,godCameraPoseVisualMesh_.indices,godPlayerWorld,
          [&](Camera3D camera){return ConstrainThirdPersonCamera(camera,focus,collisions_,.25f);},
          [&](Camera3D camera){return ConstrainThirdPersonCamera(camera,focus,showcaseTerrain_,.25f);});
      };
      const Vec3 translation=focus-godCameraBuildCacheFocus_;
      GameplayGodCameraLayout3D translated=godCameraBuildCache_;
      translated.camera.position=translated.camera.position+translation;
      translated.camera.target=translated.camera.target+translation;
      auto validated=godCameraBuildCacheValid_?resolveGodCamera(translated):GameplayGodCameraLayout3D{};
      if(!godCameraBuildCacheValid_||!validated.valid){
        godCameraAuthorityReceipt_=BuildGameplayAuthoritativeGodCamera3D({feet,yaw_,pitch_},showcaseEnvironment_.Descriptors(),godGeometry,godCameraPoseVisualMesh_.vertices,godCameraPoseVisualMesh_.indices,
          [&](Camera3D camera){return ConstrainThirdPersonCamera(camera,focus,collisions_,.25f);},
          [&](Camera3D camera){return ConstrainThirdPersonCamera(camera,focus,showcaseTerrain_,.25f);},&godPlayerWorld);
        const auto& selectedGodCamera=godCameraAuthorityReceipt_.layout;
        godCameraBuildCache_=selectedGodCamera;
        if(!godCameraBuildCache_.valid){
          const auto expected=captureIndex_<GameplayCaptureExpectations3D.size()?GameplayCaptureExpectations3D[captureIndex_].camera:GameplayCaptureCamera3D::First;
          if(GameplayGodCameraRequiresCaptureLayout3D(captureScenarioClock_.ReadyToStage(),expected)||!selectedGodCamera.rasterCandidates||selectedGodCamera.playerCoverage<=0.f)throw std::runtime_error(std::format("Gameplay god camera found no collision-safe visible production LOD transition layout: capture={} sex={} animation={} poseHash={} focus=[{},{},{}] feet=[{},{},{}] yaw={} evaluated={} coarse={} raster={} maxActualCoverage={} candidateCoverage={} candidateDistance={} candidateAzimuth={} candidateElevation={} candidateOrthoHeight={} candidateRiver={} candidateBuilding={} candidateMaximumBuildingPart={} candidateRoofCoverage={} candidateBuildingComponents={} candidateWorldDescriptors={} candidateEdge={} candidateLod={} rejectActual={} rejectLod={} rejectWorld={} rejectBuilding={}",captureIndex_<Names.size()?std::filesystem::path(Names[captureIndex_]).string():"out-of-range",rules_.Female()?"female":"male",unsigned((rules_.Female()?femaleAnimation_:maleAnimation_).State()),HashPose(godSubjectPose),focus.x,focus.y,focus.z,feet.x,feet.y,feet.z,yaw_,selectedGodCamera.evaluatedCandidates,selectedGodCamera.coarseCandidates,selectedGodCamera.rasterCandidates,selectedGodCamera.maximumActualTriangleCoverage,selectedGodCamera.playerCoverage,selectedGodCamera.distance,selectedGodCamera.azimuth,selectedGodCamera.elevation,selectedGodCamera.camera.projection.orthographicHeight,selectedGodCamera.riverCoverage,selectedGodCamera.buildingUnion,selectedGodCamera.maximumBuildingPart,selectedGodCamera.roofCoverage,selectedGodCamera.visibleBuildingComponents,selectedGodCamera.visibleWorldDescriptors,selectedGodCamera.buildingEdgeMask,selectedGodCamera.lodTransitions,selectedGodCamera.rejectedActualCoverage,selectedGodCamera.rejectedLod,selectedGodCamera.rejectedWorld,selectedGodCamera.rejectedBuilding));
          // 預 capture 操作回放可使用已經過碰撞/地形約束的最佳視角；不得冒充正式 God 構圖證據。
          camera_=selectedGodCamera.camera;godCameraBuildCacheValid_=false;break;
        }
        godCameraBuildCacheFocus_=focus;
        godCameraBuildCacheFemale_=rules_.Female();
        godCameraBuildCacheValid_=true;
        ++godCameraSearchCount_;
        validated=resolveGodCamera(godCameraBuildCache_);
        if(!validated.valid)throw std::runtime_error("Gameplay god camera failed selected-candidate deterministic closure");
      }else{
        godCameraBuildCache_=validated;
        godCameraBuildCacheFocus_=focus;
        godCameraBuildCacheFemale_=rules_.Female();
        ++godCameraCacheHitCount_;
      }
      camera_=validated.camera;
      godCameraAuthorityReceipt_.layout=validated;
      godCameraAuthorityReceipt_.inputHash=HashGameplayAuthoritativeGodCameraInput3D({feet,yaw_,pitch_});
      godCameraAuthorityReceipt_.cameraHash=CaptureCameraHash(camera_);
      godCameraAuthorityReceipt_.subjectPoseHash=HashPose(godSubjectPose);
      godCameraAuthorityReceipt_.valid=validated.valid&&godCameraAuthorityReceipt_.inputHash&&godCameraAuthorityReceipt_.cameraHash&&godCameraAuthorityReceipt_.subjectPoseHash;
    }
    break;
  }
  if(activeCaptureRoute_.Active()){
    if(captureDirectory_.empty()||!ValidateGameplayActiveCaptureRoute3D(activeCaptureRoute_,captureIndex_,capturePreparedIndex_,rules_.Camera()))throw std::runtime_error("Gameplay active capture projection route/index was missing or inconsistent");
    if(rules_.Camera()==CameraMode::GodView&&activeCaptureRoute_.route.projection!=ProjectionMode::Orthographic)throw std::runtime_error("Gameplay mixed God receipt and perspective capture route was rejected");
    if(!ApplyGameplayCaptureProductionProjection3D(camera_,activeCaptureRoute_.route))throw std::runtime_error("Gameplay active capture production projection route invalid");
  }
  if(camera_.projection.mode==ProjectionMode::Perspective){camera_.projection.orthographicHeight=0.f;if(rules_.Camera()==CameraMode::ThirdPerson){cameraBuilderExpected_.projection.orthographicHeight=0.f;cameraBuilderMatrixExact_=CaptureCameraHash(camera_)==CaptureCameraHash(cameraBuilderExpected_);}}
  if(hadTemporalCamera&&GameplayTemporalViewDiscontinuity3D(previousCamera,camera_))QueueTemporalHistoryReseed(GameplayTemporalReseedReason3D::ViewProjection);
  temporalCameraInitialized_=true;
}
void GameplayLabGame::PreserveProjectionFailure(Engine& engine,std::string reason) {
  projectionFailureReason_=std::move(reason);
  const auto fail=[&](){throw std::runtime_error(projectionFailureReason_);};
  if(projectionFailurePending_||captureDirectory_.empty()||!captureSequenceToken_||
     !capturePairFrozen_||runId_.empty()||!std::ranges::all_of(runId_,[](unsigned char c){return std::isalnum(c)||c=='-';}))fail();
  const auto directory=std::filesystem::weakly_canonical(captureDirectory_);
  const auto attributes=GetFileAttributesW(directory.c_str());
  if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)||!(attributes&FILE_ATTRIBUTE_DIRECTORY))fail();
  projectionFailureImage_=directory/("diagnostic-projection-failed-"+runId_+".png");
  if(projectionFailureImage_.parent_path()!=directory||std::filesystem::exists(projectionFailureImage_))fail();
  std::ofstream fatal(directory/"gameplay-fatal-error.txt",std::ios::trunc);
  fatal<<projectionFailureReason_<<"\ndiagnosticOnly=1 accepted=0\n";fatal.flush();if(!fatal)fail();
  // 原正式影像請求必須取消成功，才能改為唯一的失敗診斷影像。
  if(!engine.CancelReadbackCaptureSequence3D(captureSequenceToken_))fail();
  captureSequenceToken_={};capturePhaseRequestPending_=false;capturePresentationRequestPending_=false;
  const auto stats=engine.GetRendererStats();projectionFailureCaptureCount_=stats.successfulFrameCaptures;
  projectionFailureRejectedCount_=stats.rejectedFrameCaptures;
  if(!engine.CaptureNextFrame3D(projectionFailureImage_))fail();
  projectionFailurePending_=true;
}
void GameplayLabGame::FinishProjectionFailure(Engine& engine) {
  if(!projectionFailurePending_)return;
  projectionFailurePending_=false;
  // 下一次 callback 一律結束，不等待更多幀，也不執行正常擷取狀態機。
  std::string evidence="diagnostic image unavailable";
  try{
    const auto stats=engine.GetRendererStats();
    const auto attributes=GetFileAttributesW(projectionFailureImage_.c_str());
    std::array<std::byte,32> sha{};
    if(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))){
      const auto bytes=std::filesystem::file_size(projectionFailureImage_);
      std::array<unsigned char,8> signature{};std::ifstream image(projectionFailureImage_,std::ios::binary);
      image.read(reinterpret_cast<char*>(signature.data()),static_cast<std::streamsize>(signature.size()));
      const bool png=image&&signature==std::array<unsigned char,8>{137,80,78,71,13,10,26,10};
      if(bytes>=8&&bytes<=32ull*1024ull*1024ull&&png&&CaptureFileSha256(projectionFailureImage_,sha))
        evidence=std::format("diagnosticOnly=1 accepted=0 image={} bytes={} sha256={} successfulCaptureAdvanced={} rejectedCaptureAdvanced={}",
          projectionFailureImage_.filename().string(),bytes,CaptureShaHex(sha),stats.successfulFrameCaptures>projectionFailureCaptureCount_,stats.rejectedFrameCaptures>projectionFailureRejectedCount_);
    }
    std::ofstream marker(projectionFailureImage_.parent_path()/"gameplay-projection-failure-diagnostic.txt",std::ios::trunc);
    marker<<projectionFailureReason_<<'\n'<<evidence<<'\n';
  }catch(...){evidence="diagnostic verification failed; accepted=0";}
  throw std::runtime_error(projectionFailureReason_+"; "+evidence);
}
void GameplayLabGame::PublishFirstPersonPose(float simulationDelta) {
  if(!std::isfinite(simulationDelta)||simulationDelta<0.f)throw std::runtime_error("First-person publication delta was invalid");
  if(rules_.Camera()!=CameraMode::FirstPerson){firstPersonPose_.active=false;return;}
  const bool female=rules_.Female();
  const auto& animation=female?femaleAnimation_:maleAnimation_;
  const auto& source=female?femalePose_:malePose_;
  const bool entering=!firstPersonPose_.active||firstPersonPose_.female!=female;
  const float blend=entering?0.f:(std::min)(1.f,firstPersonPose_.enterBlend+simulationDelta/.2f);
  GameplayFirstPersonPoseInput3D input{};
  input.character=female?&femaleModel_:&maleModel_;input.rig=&animation.Rig();input.sourcePose=&source;
  input.sword=female?&femaleSwordGeometry_:&maleSwordGeometry_;input.camera=camera_;
  input.state=animation.State();input.phase=animation.NormalizedPhase();input.enterBlend=blend;
  if(candidate7Active_&&female){input.shoulderCenter=Candidate7ShoulderCenter3D;input.readyGrip=Candidate7ReadyGrip3D;}
  if(EvaluateGameplayFirstPersonPose3D(input,firstPersonPoseWorkspace_,firstPersonPose_.output)!=GameplayFirstPersonPoseStatus3D::Ready)
    throw std::runtime_error("First-person fixed-step natural pose publication failed");
  firstPersonPose_.state=input.state;firstPersonPose_.phase=input.phase;firstPersonPose_.enterBlend=blend;
  firstPersonPose_.tick=rules_.GetStats().ticks;firstPersonPose_.sourcePoseHash=HashPose(source);
  firstPersonPose_.cameraHash=CaptureCameraHash(camera_);firstPersonPose_.female=female;firstPersonPose_.active=true;
  auto& hash=firstPersonPose_.hash;hash=CaptureStateSeed;
  MixCaptureHash(hash,HashPose(firstPersonPose_.output.pose));MixCaptureHash(hash,firstPersonPose_.sourcePoseHash);
  MixCaptureHash(hash,firstPersonPose_.cameraHash);MixCaptureHash(hash,firstPersonPose_.tick);MixCaptureHash(hash,unsigned(input.state));
  MixCaptureHash(hash,female?1u:0u);MixCaptureFloat(hash,input.phase);MixCaptureFloat(hash,blend);
  for(const auto& row:firstPersonPose_.output.modelToWorld.values)for(float value:row)MixCaptureFloat(hash,value);
}
void GameplayLabGame::OnUpdate(Engine &engine, float dt) {
  FinishProjectionFailure(engine);
  const float gameplayDelta=captureDirectory_.empty()?dt:1.f/60.f;
  simulationDelta_=GameplayPresentationDelta3D(gameplayDelta);
  if(!captureDirectory_.empty()&&!captureScenarioClock_.AdvancesSimulation()){
    // capture staging 只允許相機策略以固定步長收斂；readback 等待期間連相機也凍結。
    // gameplay/animation/AI/combat/physics 一律不因 GPU 或磁碟等待時間前進。
    // GPU/readback 等待不屬於 fixed simulation；相機、pose 與足部放置全部凍結。
    UpdateCapture(engine,false);return;
  }
  Input &liveInput = engine.GetInput();
  // 自動capture使用獨立、持久的fixed-tick輸入狀態，Win32鍵鼠與視窗游標不得進入simulation。
  Input &in = captureDirectory_.empty()?liveInput:captureReplayInput_;
  const bool focused=GetForegroundWindow()==engine.Window();
  if(captureDirectory_.empty()&&!focused&&inputFocused_){
    for(const unsigned key:{unsigned('W'),unsigned('A'),unsigned('S'),unsigned('D'),unsigned(VK_SHIFT),unsigned(VK_SPACE),unsigned('G')})in.SetKey(key,false);
    in.SetMouseButton(false);in.SetRightMouseButton(false);ReleaseCapture();ClipCursor(nullptr);
    if(cursorCaptured_)ShowCursor(TRUE);cursorCaptured_=false;
  }
  inputFocused_=focused;
  const bool inputActive=focused||!captureDirectory_.empty();
  if(inputActive&&in.Pressed(VK_F1))diagnosticsVisible_=!diagnosticsVisible_;
  if (in.Pressed(VK_ESCAPE)){
    if(rules_.PaletteOpen()||paletteUi_->Active()||palette_->Preview().active){
      paletteUi_->Cancel();palette_->Cancel();palettePointerInteraction_=false;
      if(rules_.PaletteOpen()){TickInput close{};close.togglePalette=true;rules_.Tick(close,gameplayDelta);}
    }
    else engine.Quit();
  }
  if (!combatSpawned_) {
    // 怪物跟隨玩家出生區配置，避免更換合法展示站位後超出AI追蹤與近戰範圍。
    combat_->SpawnMonster(GameplayShowcaseMonsterSpawn3D(controller_.Position(),yaw_));
    combatSpawned_ = true;
  }
  if(!captureDirectory_.empty()){
    const auto replayTick=rules_.GetStats().ticks;
    ApplyReplayInput(in,replayTick);
    ApplyCombatFinishInput(in,replayTick);
    if(captureCrouchTapPending_){in.SetKey('C',true);in.SetKey('C',false);captureCrouchTapPending_=false;}
  }
  ProductionInput3D mapped = ReadProductionInput3D(in, yaw_);
  mapped.tick.attack=mapped.tick.attack||in.Pressed('F');
  const bool uiOwnedInput=rules_.PaletteOpen()||paletteUi_->Active()||palette_->Preview().active;
  if(!inputActive||uiOwnedInput){mapped.tick.movement={};mapped.tick.jump=false;mapped.tick.run=false;mapped.mouseDelta={};}
  if(mapped.tick.togglePalette&&(paletteUi_->Active()||palette_->Preview().active)){
    paletteUi_->Cancel();palette_->Cancel();palettePointerInteraction_=false;
  }
  if (mapped.camera){if(*mapped.camera!=rules_.Camera())ResetProductionCameraCut();rules_.SetCamera(*mapped.camera);}
  if (mapped.cycleCamera){ResetProductionCameraCut();rules_.CycleCamera();}
  if (mapped.lighting)
    rules_.SetLighting(*mapped.lighting);
  if (mapped.cycleLighting)
    rules_.CycleLighting();
  if (mapped.category)
    rules_.SetPaletteCategory(*mapped.category);
  if (mapped.female){if(!rules_.Female())ResetProductionCameraCut(GameplayTemporalReseedReason3D::Character);rules_.SetCharacterFemale(true);}
  if (mapped.male){if(rules_.Female())ResetProductionCameraCut(GameplayTemporalReseedReason3D::Character);rules_.SetCharacterFemale(false);}
  yaw_ -= mapped.mouseDelta.x * .0025f;
  pitch_ = Clamp(pitch_ - mapped.mouseDelta.y * .0025f, -.75f, .9f);
  if(!SyncEnvironmentCollision(controller_.Position()))throw std::runtime_error("Gameplay environment collision streaming update failed");
  if(environmentCollisions_&&environmentCollisions_->IsPartitionActive(EnvironmentPartitionId))environmentCollisionActiveReceipt_=environmentCollisions_->Receipt();
  const auto beforeCrouchPosition = controller_.Position();
  if (!captureDirectory_.empty() && rules_.GetStats().ticks == 16 &&
      !captureCeiling_)
    captureCeiling_ = collisions_.CreateBox(
        {{{beforeCrouchPosition.x, beforeCrouchPosition.y + 1.45f,
           beforeCrouchPosition.z},
          {1, .15f, 1}},
         8,
         0xffffffffu,
         0xc001,
         false});
  const Vec3 standingCenter = controller_.Position() + Vec3{0, .9f, 0};
  rules_.SetHeadroom(
      !collisions_.OverlapsBox(standingCenter, {.44f, .89f, .44f}));
  TickInput rulesInput = mapped.tick;
  rulesInput.attack = false;
  rulesInput.movement = {};
  const bool paletteWasOpen=rules_.PaletteOpen();
  rules_.Tick(rulesInput, gameplayDelta);
  if(paletteWasOpen&&!rules_.PaletteOpen()){
    for(const unsigned key:{unsigned('W'),unsigned('A'),unsigned('S'),unsigned('D'),unsigned(VK_SHIFT),unsigned(VK_SPACE),unsigned('F'),unsigned('G')})in.SetKey(key,false);
    in.SetMouseButton(false);in.SetRightMouseButton(false);mapped.mouseDelta={};
  }
  controller_.SetHeight(rules_.Crouched() ? 1.15f : 1.8f);
  minimumControllerHeight_ =
      (std::min)(minimumControllerHeight_, controller_.Extents().y * 2.f);
  crouchFeetInvariant_ =
      crouchFeetInvariant_ &&
      Length(controller_.Position() - beforeCrouchPosition) < 1e-6f;
  if (captureCeiling_ && rules_.GetStats().ticks >= 20) {
    collisions_.Destroy(captureCeiling_);
    captureCeiling_ = {};
  }
  const float movementSpeedMultiplier=rules_.Crouched()?2.f/6.f:mapped.tick.run?7.f/6.f:4.f/6.f;
  CharacterControl3D control{mapped.tick.movement, mapped.tick.jump,movementSpeedMultiplier};
  controller_.Step(control, collisions_, showcaseTerrain_, gameplayDelta);
  const auto gameplayTick = rules_.GetStats().ticks;
  // gameplayTick在Rules::Tick之後，26精確對應ApplyReplayInput的tick 25拖曳階段。
  if(!captureDirectory_.empty()&&gameplayTick==26){
    if(!captureReplayInteractionCamera_)throw std::runtime_error("Capture palette replay interaction camera was not frozen at tick 24");
    // The placed building must remain part of the authoritative replay, but it
    // may not be dropped beside the player and turn the later third-person
    // evidence shot into a close-up of one wall.  The same God-view ray,
    // collision and grid-snap path is retained; only a production-safe world
    // separation from the controlled actor is required.
    constexpr float minimumCaptureBuildingPlayerSeparation=7.f;
    const auto pointer=FindGameplayClearPlacementPointer3D(
        *captureReplayInteractionCamera_,collisions_,0.f,
        controller_.Position(),minimumCaptureBuildingPlayerSeparation);
    if(!pointer)throw std::runtime_error("Capture palette replay found no collision-free production pointer ray");
    mapped.mouse=*pointer;
  }
  const PaletteUiPoint3D pointer{mapped.mouse.x,mapped.mouse.y};
  const bool pointerInsidePalettePanel=pointer.x>=18.f&&pointer.x<362.f&&pointer.y>=502.f&&pointer.y<652.f;
  const bool pointerOverPalette=rules_.PaletteOpen()&&pointerInsidePalettePanel;
  const Vec3 forward{std::sin(yaw_), 0, std::cos(yaw_)};
  combat_->Tick({controller_.Position(), forward,
                 mapped.tick.attack && !rules_.PaletteOpen()&&!paletteUi_->Active()},
                gameplayDelta);
  if (combat_->CurrentAttack() == Attack3D::Attack3 && combat_->InHitWindow())
    weaponHitWindowEvidence_ = true;
  AnimationState3D animation = AnimationState3D::Idle;
  if (combat_->CurrentAttack() != Attack3D::None)
    animation = AnimationState3D(unsigned(AnimationState3D::Attack1) +
                                 unsigned(combat_->CurrentAttack()) - 1);
  else if (!controller_.Grounded())
    animation = controller_.Velocity().y >= 0 ? AnimationState3D::Jump
                                              : AnimationState3D::Fall;
  else if (rules_.Crouched())
    animation = AnimationState3D::Crouch;
  else if (Length(mapped.tick.movement) > .1f)
    animation =
        mapped.tick.run ? AnimationState3D::Run : AnimationState3D::Walk;
  if (!captureDirectory_.empty() && captureIndex_ == 1)
    animation = AnimationState3D::Walk;
  if (!captureDirectory_.empty() && captureIndex_ == 4)
    animation = AnimationState3D::Crouch;
  if(femaleAnimation_.State()!=animation||maleAnimation_.State()!=animation)ResetProductionCameraCut(GameplayTemporalReseedReason3D::PoseState);
  femaleAnimation_.SetState(animation);
  maleAnimation_.SetState(animation);
  const float animationDelta=gameplayDelta;
  femaleAnimation_.Tick(animationDelta);
  maleAnimation_.Tick(animationDelta);
  if(combat_->CurrentAttack()!=Attack3D::None){
    femaleAnimation_.SynchronizeNormalizedPhase(combat_->NormalizedAttackPhase());
    maleAnimation_.SynchronizeNormalizedPhase(combat_->NormalizedAttackPhase());
    // 軌跡證據只在固定遊戲模擬 tick 取樣，不依賴 OnRender 次數或呈現幀率。
    auto& trajectoryAnimation=rules_.Female()?femaleAnimation_:maleAnimation_;
    const auto& trajectoryModel=rules_.Female()?femaleModel_:maleModel_;
    AnimationPose3D trajectoryPose{};
    Matrix4 handSocket{};
    Vec3 trajectoryTip{};
    const auto& trajectorySword=rules_.Female()?femaleSwordGeometry_:maleSwordGeometry_;
    const Matrix4 trajectoryWorld=GameplayPlayerWorld3D(candidate7Active_,rules_.Female(),candidate7PresentationScale_,controller_.Position(),yaw_);
    if(!trajectoryAnimation.EvaluateInto(trajectoryPose)||
       !CharacterSocketTransform3D(trajectoryModel.skeleton,trajectoryAnimation.Rig(),HumanoidJoint3D::RightHand,trajectoryPose,trajectoryWorld,handSocket)||
       !ResolveGameplaySwordTipWorld3D(trajectorySword,handSocket,trajectoryTip)||
       !authoritativeTrajectory_.Record(rules_.GetStats().ticks,combat_->CurrentAttack(),combat_->InHitWindow(),combat_->NormalizedAttackPhase(),controller_.Position(),TransformPoint(handSocket,{}),trajectoryTip))
      throw std::runtime_error("Gameplay authoritative attack trajectory checkpoint failed");
  }
  monsterAnimation_.SetState(combat_->Monster().alive
                                 ? AnimationState3D::Walk
                                 : AnimationState3D::Death);
  monsterAnimation_.Tick(animationDelta);
  // Camera search 與 renderer 共用同一份 fixed-tick pose。呈現幀不得成為 pose 的作者。
  if(!femaleAnimation_.EvaluateInto(femalePose_)||!maleAnimation_.EvaluateInto(malePose_)||
     !monsterAnimation_.EvaluateInto(monsterPose_))throw std::runtime_error("Gameplay authoritative fixed-tick pose evaluation failed");
  if(candidate7Active_)ApplyCandidate7ArmRest3D(femaleModel_,femaleAnimation_.Rig(),femalePose_);
  if(combat_->CurrentAttack()!=Attack3D::None&&
     (captureDirectory_.empty()||captureIndex_!=4)){
    const float error=std::abs(femaleAnimation_.NormalizedPhase()-combat_->NormalizedAttackPhase());
    maximumWeaponPhaseError_=(std::max)(maximumWeaponPhaseError_,error);
    weaponAnimationSyncPassed_=weaponAnimationSyncPassed_&&error<=.035f;
  }
  physics_.Step(gameplayDelta, 2);
  if (!captureDirectory_.empty() && rules_.GetStats().ticks == 240)
    captureAuthoritativePhysicsSteps_ = physics_.Stats().steps;
  if (!palette_->SyncDynamicTransforms())
    engine.Quit();
  // 固定模擬順序：動畫狀態/Tick -> pose Evaluate -> physics/sync -> 足部貼地。
  // Render 與 GPU readback 等待路徑只能讀取這份權威姿勢，不得重新求值。
  {
    const Matrix4 playerWorld=GameplayPlayerWorld3D(candidate7Active_,rules_.Female(),candidate7PresentationScale_,controller_.Position(),yaw_);
    FootPlacementFrame3D footFrame{};
    footFrame.collisionWorld=&collisions_;
    footFrame.modelToWorld=playerWorld;
    footFrame.worldToModel=GameplayPlayerWorldInverse3D(candidate7Active_,rules_.Female(),candidate7PresentationScale_,controller_.Position(),yaw_);
    footFrame.deltaTime=gameplayDelta;
    authorityFootPlacementCameraDistanceInput_=Length(camera_.position-controller_.Position());
    footFrame.cameraDistance=authorityFootPlacementCameraDistanceInput_;
    footFrame.frameIndex=gameplayTick;
    AnimationPose3D& activePose=rules_.Female()?femalePose_:malePose_;
    FootPlacement3D& activePlacement=rules_.Female()?femaleFootPlacement_:maleFootPlacement_;
    const SkinnedModel3D& activeModel=rules_.Female()?femaleModel_:maleModel_;
    const HumanoidSkeletonMap3D& activeRig=rules_.Female()?femaleAnimation_.Rig():maleAnimation_.Rig();
    FootPlacementResult3D footResult{};
    if(!activePlacement.Evaluate(activeModel.skeleton,activeRig,footFrame,activePose,&footResult))
      throw std::runtime_error("Gameplay authoritative fixed-tick foot placement failed");
    authorityFootPlacementHash_=HashFootPlacement(footResult);
    if(!authorityFootPlacementHash_)throw std::runtime_error("Gameplay authoritative foot placement hash was invalid");
  }
  // Final camera必須消費本tick完成足部貼地後的pose；foot cameraDistance明確來自前一tick camera。
  UpdateCamera(engine);
  PublishFirstPersonPose(gameplayDelta);
  if(!captureDirectory_.empty()&&gameplayTick==24){
    const auto cameraHash=CaptureCameraHash(camera_);
    if(activeCaptureRoute_.Active()||rules_.Camera()!=CameraMode::GodView||camera_.projection.mode!=ProjectionMode::Orthographic||!godCameraAuthorityReceipt_.valid||cameraHash!=godCameraAuthorityReceipt_.cameraHash)
      throw std::runtime_error(std::format("Gameplay prelude God camera/projection authority closure failed: activeRoute={} camera={} projection={} receiptValid={} layoutValid={} cameraHash={} receiptCameraHash={} subjectPoseHash={} playerCoverage={} rasterCandidates={}",
        activeCaptureRoute_.Active(),unsigned(rules_.Camera()),unsigned(camera_.projection.mode),godCameraAuthorityReceipt_.valid,godCameraAuthorityReceipt_.layout.valid,cameraHash,godCameraAuthorityReceipt_.cameraHash,godCameraAuthorityReceipt_.subjectPoseHash,godCameraAuthorityReceipt_.layout.playerCoverage,godCameraAuthorityReceipt_.layout.rasterCandidates));
    captureReplayInteractionCamera_=camera_;captureReplayInteractionCameraInputHash_=godCameraAuthorityReceipt_.inputHash;captureReplayInteractionCameraSubjectPoseHash_=godCameraAuthorityReceipt_.subjectPoseHash;
  }
  // Interaction camera/ray只可在final rendered camera完成後建立。優先權：frozen God replay、
  // first-person grab fixture、其餘live/fixed gameplay的final rendered camera。
  const Camera3D firstPersonInteractionOverride=MakeFirstPersonCamera(controller_.Position()+Vec3{0,rules_.CharacterHeight()-.12f,0},yaw_,pitch_);
  const auto interactionSelection=SelectGameplayInteractionCamera3D(camera_,captureReplayInteractionCamera_?&*captureReplayInteractionCamera_:nullptr,firstPersonInteractionOverride,!captureDirectory_.empty(),gameplayTick);
  const Camera3D interactionCamera=interactionSelection.camera;
  const Ray3D cursorRay=ScreenPointToRay(interactionCamera,{1280,720},mapped.mouse);
  const Ray3D interactionRayRecheck=ScreenPointToRay(interactionSelection.camera,{1280,720},mapped.mouse);
  if(!interactionSelection.interactionCameraHash||HashGameplayInteractionRay3D(cursorRay)!=HashGameplayInteractionRay3D(interactionRayRecheck))throw std::runtime_error("Interaction camera/ray did not derive from this tick selected final camera");
  if(captureDirectory_.empty()&&(interactionSelection.source!=GameplayInteractionCameraSource3D::FinalRendered||interactionSelection.interactionCameraHash!=interactionSelection.finalRenderedCameraHash))throw std::runtime_error("Live interaction camera did not use this tick final rendered camera");
  if (!captureDirectory_.empty() && gameplayTick == 258 && !captureGrabSeeded_) {
    const float directionLength = Length(cursorRay.direction);
    if (!std::isfinite(directionLength) || directionLength < .0001f)throw std::runtime_error("Capture grab ray was invalid");
    const Vec3 seedPosition=cursorRay.origin+cursorRay.direction*(3.f/directionLength);
    if (!palette_->PlaceAt(seedPosition, PaletteCategory::Physics))throw std::runtime_error("Capture grab seed transaction failed");
    rules_.SetPaletteCategory(PaletteCategory::Lights);captureGrabSeeded_=true;
  }
  if(in.Pressed('G')&&!rules_.PaletteOpen())palette_->BeginGrab(cursorRay);
  if(palette_->Grabbed()){if(in.Down('G'))palette_->UpdateGrab(cursorRay,gameplayDelta);else palette_->EndGrab();}
  // 指標命中UI時優先攔截；鍵盤與指標共用同一個transaction包裝器。
  if(rules_.PaletteOpen()&&in.MousePressed()&&pointerOverPalette){
    if(const auto category=paletteUi_->HitTest(pointer))rules_.SetPaletteCategory(*category);
    palettePointerInteraction_=paletteUi_->PointerDown(pointer,cursorRay);
  }else if(rules_.PaletteOpen()&&mapped.tick.attack&&!pointerOverPalette&&!paletteUi_->Active()){
    palettePointerInteraction_=false;paletteUi_->KeyboardBegin(rules_.Category(),cursorRay);
  }
  if(paletteUi_->Active()){
    if(mapped.tick.cancel){paletteUi_->Cancel();palettePointerInteraction_=false;}
    else if(in.MouseDown()){
      if(palettePointerInteraction_)paletteUi_->PointerDrag(cursorRay,mapped.rotate);
      else paletteUi_->KeyboardPreview(cursorRay,mapped.rotate);
    }else{paletteUi_->PointerUp(true);palettePointerInteraction_=false;}
  }
  if (mapped.tick.erase && !rules_.Entities().empty())
    palette_->Delete(rules_.Entities().back().id);
  if (mapped.tick.undo)
    palette_->Undo();
  if (mapped.tick.redo)
    palette_->Redo();
  if(!captureDirectory_.empty()&&gameplayTick>=24&&gameplayTick<=26){auto& diagnostic=capturePaletteReplayDiagnostics_[gameplayTick-24];const auto preview=palette_->Preview();const auto stats=rules_.GetStats();diagnostic.observed=true;diagnostic.liveSuppressed=true;diagnostic.tick=gameplayTick;diagnostic.gameplayDelta=gameplayDelta;diagnostic.inputBits=(mapped.tick.attack?1ull:0ull)|(mapped.tick.rotate?2ull:0ull)|(mapped.tick.cancel?4ull:0ull)|(mapped.tick.undo?8ull:0ull)|(mapped.tick.redo?16ull:0ull);diagnostic.cameraHash=CaptureCameraHash(interactionCamera);diagnostic.cameraAuthorityInputHash=captureReplayInteractionCameraInputHash_;diagnostic.cameraSubjectPoseHash=captureReplayInteractionCameraSubjectPoseHash_;diagnostic.placementPlaneHeight=0.f;diagnostic.gridStep=2.f;diagnostic.footPlacementCameraDistance=authorityFootPlacementCameraDistanceInput_;diagnostic.normalizedPointer={mapped.mouse.x/1280.f,mapped.mouse.y/720.f};diagnostic.cameraPosition=interactionCamera.position;diagnostic.cameraTarget=interactionCamera.target;diagnostic.cameraUp=interactionCamera.up;diagnostic.projectionMode=interactionCamera.projection.mode;diagnostic.cameraRawBits={std::bit_cast<std::uint32_t>(interactionCamera.position.x),std::bit_cast<std::uint32_t>(interactionCamera.position.y),std::bit_cast<std::uint32_t>(interactionCamera.position.z),std::bit_cast<std::uint32_t>(interactionCamera.target.x),std::bit_cast<std::uint32_t>(interactionCamera.target.y),std::bit_cast<std::uint32_t>(interactionCamera.target.z),std::bit_cast<std::uint32_t>(interactionCamera.up.x),std::bit_cast<std::uint32_t>(interactionCamera.up.y),std::bit_cast<std::uint32_t>(interactionCamera.up.z),std::bit_cast<std::uint32_t>(interactionCamera.projection.verticalFieldOfView),std::bit_cast<std::uint32_t>(interactionCamera.projection.orthographicHeight),std::bit_cast<std::uint32_t>(interactionCamera.projection.nearPlane),std::bit_cast<std::uint32_t>(interactionCamera.projection.farPlane)};diagnostic.rayOrigin=cursorRay.origin;diagnostic.rayDirection=cursorRay.direction;if(std::abs(cursorRay.direction.y)>=.0001f){const float hitDistance=-cursorRay.origin.y/cursorRay.direction.y;if(std::isfinite(hitDistance)&&hitDistance>=0.f&&hitDistance<=200.f){diagnostic.hitValid=true;diagnostic.projectedHit=cursorRay.origin+cursorRay.direction*hitDistance;}}diagnostic.previewActive=preview.active;diagnostic.previewValid=preview.valid;diagnostic.snappedTarget=preview.position;diagnostic.undoDepth=stats.undoDepth;diagnostic.redoDepth=stats.redoDepth;if(gameplayTick==26&&(!diagnostic.hitValid||!diagnostic.previewValid))throw std::runtime_error("Gameplay authoritative God palette placement lacked a valid ray hit or preview");if(diagnostic.previewActive||gameplayTick==26){Vec3 expected=diagnostic.projectedHit;expected.x=std::round(expected.x/diagnostic.gridStep)*diagnostic.gridStep;expected.z=std::round(expected.z/diagnostic.gridStep)*diagnostic.gridStep;expected.y=diagnostic.placementPlaneHeight+.5f;if(diagnostic.snappedTarget.x!=expected.x||diagnostic.snappedTarget.y!=expected.y||diagnostic.snappedTarget.z!=expected.z)throw std::runtime_error("Gameplay authoritative God palette placement disagreed with the same-frame camera ray and grid snap");}}
  const bool captureCursor=captureDirectory_.empty()&&focused&&!rules_.PaletteOpen()&&!paletteUi_->Active();
  if(captureCursor&&!cursorCaptured_){
    SetCapture(engine.Window());RECT client{};GetClientRect(engine.Window(),&client);POINT topLeft{client.left,client.top},bottomRight{client.right,client.bottom};ClientToScreen(engine.Window(),&topLeft);ClientToScreen(engine.Window(),&bottomRight);RECT clip{topLeft.x,topLeft.y,bottomRight.x,bottomRight.y};ClipCursor(&clip);ShowCursor(FALSE);cursorCaptured_=true;
  }else if(!captureCursor&&cursorCaptured_){ReleaseCapture();ClipCursor(nullptr);ShowCursor(TRUE);cursorCaptured_=false;}
  if(captureCursor){RECT client{};GetClientRect(engine.Window(),&client);POINT center{(client.right-client.left)/2,(client.bottom-client.top)/2};ClientToScreen(engine.Window(),&center);SetCursorPos(center.x,center.y);}
  if(!captureDirectory_.empty()){
    bool scenarioReady=true;
    if(captureIndex_==10){const auto grab=palette_->Stats();scenarioReady=!combat_->Monster().alive&&combat_->CurrentAttack()==Attack3D::Attack3&&combat_->InHitWindow()&&grab.grabs&&grab.dragSteps&&grab.drops;}
    if(!captureScenarioClock_.RecordSimulationStep(scenarioReady))throw std::runtime_error("Gameplay capture scenario clock rejected a fixed simulation step");
  }
  if(!captureDirectory_.empty())captureReplayInput_.ConsumePresses();
  UpdateCapture(engine,true);
}
void GameplayLabGame::BuildWorld(Engine &engine) {
  scene_.Clear();
  scene_.SetBoxMesh(boxMesh_);
  AddShowcaseEnvironment();
  // 物件面板的牆、堆疊剛體與預覽方塊是互動驗證道具，不是森林內容。
  // 正式環境／戰鬥擷取只保留其權威物理狀態，避免測試幾何污染展示畫面。
  const bool showPaletteFixtures = captureDirectory_.empty() || captureIndex_ == 9;
  if (showPaletteFixtures)
    scene_.MaterialBox({0, 1.5f, -12}, {8, 3, 1.2f},
                       {{.24f, .13f, .08f, 1}, {.48f, 0, 0, 1}, false},
                       {{GameplayWorldIdentityHigh, 1}, 1});
  // R34_PALETTE_BUILDING_BEGIN
  // One shared gabled mesh replaces one cube: no per-entity draw growth at the
  // 256-entity cap. The original body/collider and identity remain authoritative.
  const auto submitPaletteBuilding=[&](Vec3 center,float yaw,Color accent,
      RenderObjectIdentity3D identity,bool feedback){
    if(!showcaseWoodMaterial_.textures)return false;
    auto wood=showcaseWoodMaterial_;
    wood.tint=feedback?accent:Color{.64f,.50f,.38f,1.f};
    wood.surface.roughness=.68f;wood.surface.metallic=0;
    Transform3D transform{};
    if(!GameplayPaletteBuildingTransform3D(center,yaw,transform))return false;
    const auto before=scene_.MeshCount();
    scene_.Mesh(showcaseConeHighMesh_,transform,wood,{},identity);
    return scene_.MeshCount()==before+GameplayPaletteBuildingDrawCap3D;
  };
  std::uint64_t worldBodyIndex{};
  for (auto handle : bodies_) {
    if (const auto *b = physics_.GetBody(handle))
      scene_.MaterialBox(b->settings.transform.position,
                         b->settings.colliderExtents * 2,
                         {{.18f, .28f, .38f, 1}, {.38f, .12f, 0, 1}, false},
                         {{GameplayWorldIdentityHigh, 2 + worldBodyIndex}, 1});
    ++worldBodyIndex;
  }
  if (showPaletteFixtures) {
    for (const auto &placed : rules_.Entities()) {
      // Capture 9 owns the palette UI plus its explicit valid/invalid building
      // ghosts.  Entities added after the initial stack belong to later replay
      // scenarios (notably the capture-10 grab fixture) and remain physically
      // authoritative without leaking into this shot's draw list.
      if(!captureDirectory_.empty()&&captureIndex_==9&&placed.id>18)continue;
      if (const auto *body = physics_.GetBody(palette_->Body(placed.id))) {
        if(placed.category==PaletteCategory::Building){
          if(!submitPaletteBuilding(body->settings.transform.position,
              body->settings.transform.rotation.y,
              Color{.58f,.36f,.19f,1.f},
              {{GameplayPlacedIdentityHigh,placed.id},1},false))
            throw std::runtime_error("Palette building visual assembly unavailable");
        }else scene_.MaterialBox(
              body->settings.transform.position, body->settings.colliderExtents * 2,
              {{placed.id <= 18 ? .18f : .8f, placed.id <= 18 ? .28f : .42f,
                placed.id <= 18 ? .38f : .12f, 1},
               {.32f, .08f, 0, 1},
               false},
              {{GameplayPlacedIdentityHigh, placed.id}, 1});
      }
    }
  }
  const auto preview = palette_->Preview();
  if (preview.active){
    if(preview.category==PaletteCategory::Building){
      const Color accent=preview.valid ? Color{.72f,.92f,.72f,1.f} :
                                         Color{1.f,.60f,.55f,1.f};
      if(!submitPaletteBuilding(preview.position,preview.rotation,accent,
          {{GameplayPreviewIdentityHigh,1},1},true))
        throw std::runtime_error("Palette building preview assembly unavailable");
    }else scene_.MaterialBox(
          preview.position, {1.04f, 1.04f, 1.04f},
          {{preview.valid ? .15f : .9f, preview.valid ? .85f : .12f, .08f, .55f},
           {.7f, 0, 0, .5f},
           false},
          {{GameplayPreviewIdentityHigh, 1}, 1});
  }
  if (captureIndex_ == 9) {
    if (const auto valid = captureValidGhost_)
      if(!submitPaletteBuilding(*valid,0.f,Color{.72f,.92f,.72f,1.f},
          {{GameplayPreviewIdentityHigh,2},1},true))
        throw std::runtime_error("Valid palette capture building unavailable");
    if (const auto invalid = captureInvalidGhost_)
      if(!submitPaletteBuilding(*invalid,0.f,Color{1.f,.60f,.55f,1.f},
          {{GameplayPreviewIdentityHigh,3},1},true))
        throw std::runtime_error("Invalid palette capture building unavailable");
  }
  // R34_PALETTE_BUILDING_END
  appliedOcclusionIdentity_={};appliedOcclusionUpdate_={};appliedOcclusionOpacity_=1.f;
  if(cameraPolicyFadeIdentity_){
    const float opacity=GameplayCaptureQueuedPhaseActive3D(capturePairFrozen_,captureRawPhase_)?1.f:cameraFadeAlpha_;
    const auto update=scene_.SetObjectOcclusionOpacity(cameraPolicyFadeIdentity_,opacity);
    if(!update||update.generationConflicts||update.missing)throw std::runtime_error("Gameplay exact occlusion opacity identity was not present in the submitted scene");
    appliedOcclusionIdentity_=cameraPolicyFadeIdentity_;appliedOcclusionUpdate_=update;appliedOcclusionOpacity_=opacity;
  }
  if(GameplayCaptureQueuedPhaseActive3D(capturePairFrozen_,captureReferencePhase_)){
    std::uint32_t matched{};
    // 低於門檻時不提交任何透明度覆寫，完整保留 effective phase 的實際狀態；
    // 超過門檻才逐一移除六個建築 ID，避免「寫回 1」製造假的 fingerprint 差異。
    const auto referencePlan=GameplayReferenceBuildingOverridePlan(captureEffectiveBuildingUnion_[captureIndex_]);
    const float referenceBuildingOpacity=referencePlan.opacity;
    const auto buildingIdentities=BuildGameplayBuildingIdentities3D(showcaseEnvironment_.Descriptors(),EnvironmentPartitionId);
    if(referencePlan.Removes())for(std::size_t part=0;part<buildingIdentities.size();++part){captureReferenceRequestedById_[captureIndex_][part]=1;captureReferenceAppliedOpacityById_[captureIndex_][part]=referenceBuildingOpacity;
      const auto update=scene_.SetObjectOcclusionOpacity(buildingIdentities[part],referenceBuildingOpacity);
      captureReferenceMatchedById_[captureIndex_][part]=std::uint8_t(update.matchedStatic+update.matchedMultipart+update.matchedSkinned);captureReferenceMissingById_[captureIndex_][part]=update.missing?1u:0u;captureReferenceConflictById_[captureIndex_][part]=update.generationConflicts?1u:0u;
      if(!update||update.generationConflicts||update.missing)throw std::runtime_error("Gameplay character reference visibility override failed closed");
      matched+=std::uint32_t(update.matchedStatic+update.matchedMultipart+update.matchedSkinned);
    }
    captureReferenceRequestedCount_[captureIndex_]=referencePlan.RequestedCount();captureReferenceOpacityMatches_[captureIndex_]=matched;captureReferenceRemovalThreshold_[captureIndex_]=referencePlan.removalThreshold;captureReferenceOverrideMode_[captureIndex_]=referencePlan.Removes()?1u:0u;captureReferenceRemovedCount_[captureIndex_]=referencePlan.Removes()?matched:0u;captureReferenceRemovedCoverage_[captureIndex_]=referencePlan.Removes()?captureEffectiveBuildingUnion_[captureIndex_]:0.f;
  }
  DrawList &draw = engine.Draw();
  const auto showcaseLighting=rules_.Lighting()==LightingPhase::Day?ShowcaseLightingProfile3D::Day:rules_.Lighting()==LightingPhase::Dusk?ShowcaseLightingProfile3D::Dusk:ShowcaseLightingProfile3D::Night;
  const auto lightingState=showcaseEnvironment_.EvaluateLighting(showcaseLighting);
  engine.SetClearColor(lightingState.sky);showcaseEnvironment_.ApplyHighProfile(draw);if(temporalFixtureCloudOff_){auto clouds=draw.VolumetricClouds();clouds.enabled=false;draw.SetVolumetricClouds(clouds);}if(captureLightingIsolation_&1u)draw.SetDirectionalShadowMapEnabled(false);if(captureLightingIsolation_&2u){auto ambientOcclusion=draw.AmbientOcclusion();ambientOcclusion.enabled=false;draw.SetAmbientOcclusion(ambientOcclusion);}
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
  if(captureLightingIsolation_&8u){auto contactShadows=draw.ContactShadows();contactShadows.enabled=false;draw.SetContactShadows(contactShadows);}
#endif
  if(engine.RequestedRenderQuality()==RenderQuality3D::Ultra)draw.SetDirectionalShadowCascades({4,4096,.1f,220.f,.70f,.10f});showcaseEnvironment_.ApplyLighting(draw,showcaseLighting,camera_.position);
  // 曝光直接使用場景型別狀態，避免兩份時段策略發生漂移。
  draw.SetExposureCompensation(lightingState.exposureEv);
  const auto grabbed = palette_->Grabbed();
  const auto grabbedCategory = palette_->GrabbedCategory();
  const bool cleanCapturePresentation=!captureDirectory_.empty()&&(capturePresentationWarmupActive_||capturePresentationRequestPending_||capturePresentationPending_);
  const auto interactionHudPolicy=BuildGameplayHudPresentationPolicy3D(diagnosticsVisible_,rules_.PaletteOpen(),cleanCapturePresentation);
  if(interactionHudPolicy.interactionDiagnostics){
    draw.Quad({1025, 575}, {490, 122}, {.008f, .018f, .022f, .9f});
    draw.Text({790, 528}, "[G HOLD] GRAB / RELEASE", 1.1f, Cyan);
    draw.Text({790, 554},
              std::format("ACTIVE {}  ID {}  CLASS {}", grabbed ? 1 : 0,
                          grabbed.value_or(0),
                          unsigned(grabbedCategory.value_or(rules_.Category()))),
              1.05f, grabbed ? Gold : Cyan);
    draw.Text(
        {790, 580},
        std::format(
            "HOLD {:.2f}  ERROR {:.3f}",
            grabbed ? Length(palette_->GrabTarget() - camera_.position) : 0.f,
            palette_->GrabError()),
        1.05f, Gold);
  }
  const auto sceneStats=scene_.Submit(draw, camera_, {1280, 720},
                {true, true, {true, .02f, .2f, 80, 256}, {150, .1f}});
  if(capturePreparedIndex_&&*capturePreparedIndex_==captureIndex_)
    capturePreparedSceneSubmitted_=true;
  if(captureIndex_<Names.size()){
    // 收據當前正式相機的英雄場景可視性與LOD混合，不反向修改相機或角色。
    captureEnvironmentVisibility_[captureIndex_]=showcaseEnvironment_.EvaluateCameraVisibility(camera_,{1280,720});
    captureEnvironmentLodSamples_[captureIndex_]=showcaseEnvironment_.EvaluateLodSamples(camera_.position);
    captureSubmittedLodRows_[captureIndex_]=BuildShowcaseLodRenderRows3D(camera_,showcaseSubmittedLodItems_);
    std::vector<ShowcaseLodRenderRow3D> expectedTransitions;for(const auto& row:captureSubmittedLodRows_[captureIndex_])if(row.transitioning)expectedTransitions.push_back(row);
    auto& actualTransitions=captureActualLodRows_[captureIndex_];actualTransitions=BuildShowcaseActualLodRows3D(expectedTransitions,draw.meshInstances,EnvironmentPartitionId);
    captureLodContracts_[captureIndex_]=CompareShowcaseLodRenderRows3D(expectedTransitions,actualTransitions);
  }
  if(!temporalSceneIdentityFingerprint_||temporalSceneIdentityFingerprint_!=sceneStats.acceptedIdentityOrderFingerprint){QueueTemporalHistoryReseed(GameplayTemporalReseedReason3D::DrawIdentity);temporalSceneIdentityFingerprint_=sceneStats.acceptedIdentityOrderFingerprint;}
  if(temporalHistoryCutPending_){draw.SetCameraCut3D(true);temporalHistoryCutPending_=false;temporalHistoryPendingReasonMask_=0;}
  if(captureIndex_<Names.size()){
    if(capturePairFrozen_&&captureSequenceToken_){if(!captureScenePhaseLedger_.Resolve(captureSequenceToken_,captureIndex_,capturePairMetadata_.sequence.index)&&!captureScenePhaseLedger_.Record(captureSequenceToken_,captureIndex_,capturePairMetadata_.sequence.index,sceneStats))throw std::runtime_error("Gameplay Scene phase submission provenance was mismatched");}
  }
  if(captureIndex_<Names.size()){std::uint32_t identities{},terrain{},terrainIdentity{},groundIdentity{},groundOpaque{},groundShadowOnly{};for(const auto& instance:draw.meshInstances){identities+=instance.identity?1u:0u;const bool ground=instance.identity==RenderObjectIdentity3D{{EnvironmentPartitionId,1},1};groundIdentity+=ground?1u:0u;groundOpaque+=ground&&instance.layer==MeshRenderLayer3D::Opaque?1u:0u;groundShadowOnly+=ground&&instance.layer==MeshRenderLayer3D::ShadowOnly?1u:0u;if(ground){captureGroundMeshHandle_[captureIndex_]=instance.mesh.value;captureGroundCastsShadow_[captureIndex_]=instance.castsShadow?1u:0u;captureGroundAlphaMode_[captureIndex_]=std::uint32_t(instance.alphaMode);}if(instance.materialClass==MaterialClass3D::Terrain){++terrain;terrainIdentity+=ground?1u:0u;}}captureDrawIdentityCount_[captureIndex_]=identities;captureDrawTerrainCount_[captureIndex_]=terrain;captureDrawTerrainIdentityCount_[captureIndex_]=terrainIdentity;captureDrawGroundIdentityCount_[captureIndex_]=groundIdentity;captureDrawGroundOpaqueCount_[captureIndex_]=groundOpaque;captureDrawGroundShadowOnlyCount_[captureIndex_]=groundShadowOnly;}
  if(captureIndex_<Names.size()&&!showcaseEnvironment_.Descriptors().empty()){const auto& ground=showcaseEnvironment_.Descriptors().front();const auto bounds=TransformSphereBounds(showcaseTerrainMesh_.bounds,{ground.position,{0,ground.yaw,0},{1,1,1}});captureGroundBoundsRadius_[captureIndex_]=bounds.radius;captureGroundDistanceMinusRadius_[captureIndex_]=Length(bounds.center-camera_.position)-bounds.radius;captureGroundFrustumAccepted_[captureIndex_]=SphereIntersectsFrustum(bounds.center,bounds.radius,camera_,1280.f/720.f)?1u:0u;captureGroundViewDepth_[captureIndex_]=TransformPoint(CreateViewMatrix(camera_),bounds.center).z;captureGroundProjectedRadius_[captureIndex_]=ProjectedSphereRadiusPixels(bounds.center,bounds.radius,camera_,{1280,720});}
  scene_.Clear();
}

std::filesystem::path GameplayLabGame::GameplayHdrCapturePath(std::size_t index)const{
  if(index>=Names.size()||captureDirectory_.empty())return{};
  return captureDirectory_/(std::filesystem::path(Names[index]).stem().wstring()+L".production-hdr.aevhdr");
}
void GameplayLabGame::CleanupGameplayHdrCaptureArtifacts(std::size_t index,bool removeFinal)noexcept{
  const auto destination=GameplayHdrCapturePath(index);if(destination.empty())return;std::error_code error;if(removeFinal)std::filesystem::remove(destination,error);error.clear();const std::wstring stem=std::filesystem::path(Names[index]).stem().wstring();
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
  if(removeFinal){
    for(const auto& name:{stem+L"-raw.production-hdr.aevhdr",stem+L"-atmosphere-rgba16f.bin",stem+L"-atmosphere-output.json",stem+L"-atmosphere-scene-rgba16f.bin",stem+L"-atmosphere-scene.json"}){
      error.clear();std::filesystem::remove(captureDirectory_/name,error);
    }
  }
#endif
  const std::wstring prefix=destination.filename().wstring()+L".tmp.";for(std::filesystem::directory_iterator iterator(captureDirectory_,error),end;!error&&iterator!=end;iterator.increment(error)){const auto name=iterator->path().filename().wstring();if(name.starts_with(prefix)){std::error_code removeError;std::filesystem::remove(iterator->path(),removeError);}}
}
void GameplayLabGame::FailGameplayHdrCapture(Engine& engine,std::size_t index)noexcept{
  if(index<gameplayHdrCaptureStates_.size()){gameplayHdrCaptureStates_[index]=GameplayHdrCaptureState3D::Failed;gameplayHdrCaptureReceipts_[index]={};gameplayHdrCaptureReceipts_[index].state=GameplayHdrCaptureState3D::Failed;CleanupGameplayHdrCaptureArtifacts(index,true);}
  engine.CancelLinearHdrReadback3D();if(captureSequenceToken_)engine.CancelReadbackCaptureSequence3D(captureSequenceToken_);captureSequenceToken_={};capturePresentationLogicalToken_={};captureSequencePhaseCount_=0;captureScenePhaseLedger_.Clear();capturePhaseRequestPending_=capturePresentationWarmupActive_=capturePresentationRequestPending_=capturePresentationPending_=false;capturePresentationWarmupRemaining_=capturePresentationWarmupFramesCurrent_=captureDiagnosticAsyncWaitFrames_=0;capturePending_=false;gameplayHdrInFlight_=0;
}
bool GameplayLabGame::WriteGameplayHdrCaptureReceipt()const{
  constexpr std::uint64_t MaximumEngineHdrBytes=16ull*1024ull*1024ull;if(captureDirectory_.empty()||gameplayHdrEnginePeakBytes_>MaximumEngineHdrBytes||gameplayHdrMaxInFlight_!=1||gameplayHdrInFlight_)return false;
  const auto nonzero=[](const auto& hash){return std::ranges::any_of(hash,[](std::uint8_t value){return value!=0;});};
  for(std::size_t index=0;index<Names.size();++index){const auto& receipt=gameplayHdrCaptureReceipts_[index];if(gameplayHdrCaptureStates_[index]!=GameplayHdrCaptureState3D::Verified||receipt.state!=GameplayHdrCaptureState3D::Verified||!receipt.verified||receipt.path!=GameplayHdrCapturePath(index)||!receipt.byteCount||!receipt.frameSerial||!receipt.sequenceToken||receipt.sequenceIndex!=1||!nonzero(receipt.fileHash)||!nonzero(receipt.stableContentHash)||!nonzero(receipt.payloadHash)||!nonzero(receipt.rootHash))return false;}
  const auto destination=captureDirectory_/"gameplay-hdr-capture-receipt.json",temporary=captureDirectory_/"gameplay-hdr-capture-receipt.json.tmp";std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output)return false;output<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabProductionHdr\",\"expectedCaptures\":"<<Names.size()<<",\"engineRawPeakBytes\":"<<gameplayHdrEnginePeakBytes_<<",\"maximumInFlight\":"<<gameplayHdrMaxInFlight_<<",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)output<<',';const auto& receipt=gameplayHdrCaptureReceipts_[index];output<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"path\":\""<<receipt.path.filename().string()<<"\",\"state\":\""<<GameplayHdrStateName3D(receipt.state)<<"\",\"byteCount\":"<<receipt.byteCount<<",\"fileHash\":\""<<GameplayProductionHashHex3D(receipt.fileHash)<<"\",\"stableContentHash\":\""<<GameplayProductionHashHex3D(receipt.stableContentHash)<<"\",\"payloadHash\":\""<<GameplayProductionHashHex3D(receipt.payloadHash)<<"\",\"rootHash\":\""<<GameplayProductionHashHex3D(receipt.rootHash)<<"\",\"frameSerial\":"<<receipt.frameSerial<<",\"sequenceToken\":"<<receipt.sequenceToken<<",\"sequenceIndex\":"<<receipt.sequenceIndex<<",\"verified\":1}";}output<<"]}";output.flush();if(!output){output.close();std::error_code ignored;std::filesystem::remove(temporary,ignored);return false;}output.close();if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::error_code ignored;std::filesystem::remove(temporary,ignored);return false;}return true;
}
void GameplayLabGame::RequestCapturePhase(Engine& engine){
  if(capturePresentationRequestPending_){
    if(!capturePairFrozen_||captureSequenceToken_||!capturePresentationLogicalToken_||captureIndex_>=Names.size())throw std::runtime_error("Gameplay presentation capture was not paired with a completed diagnostic sequence");
    const auto path=(captureDirectory_/Names[captureIndex_]).lexically_normal();
    captureRequestedEffectiveNames_[captureIndex_]=path.filename().wstring();captureRequestedEffectivePaths_[captureIndex_]=path;captureCommittedEffectiveNames_[captureIndex_]=path.filename().wstring();captureCommittedEffectivePaths_[captureIndex_]=path;
    const auto renderer=engine.GetRendererStats();captureSuccess_=renderer.successfulFrameCaptures;captureReject_=renderer.rejectedFrameCaptures;
    if(!engine.CaptureNextFrame3D(path))throw std::runtime_error("Gameplay production presentation capture request failed");
    capturePresentationRequestPending_=false;capturePresentationPending_=true;
    return;
  }
  if(!capturePhaseRequestPending_)return;
  if(!capturePairFrozen_||!captureSequenceToken_||captureIndex_>=Names.size()||!captureScenePhaseLedger_.BindRequest(captureSequenceToken_,captureIndex_,capturePairMetadata_.sequence.index,capturePairMetadata_)){
    FailGameplayHdrCapture(engine,captureIndex_);
    throw std::runtime_error("Gameplay capture phase could not bind the exact current Scene submission");
  }
  const bool effective=capturePairMetadata_.sequence.index==1;
  if(effective){
    if(gameplayHdrCaptureStates_[captureIndex_]!=GameplayHdrCaptureState3D::Armed){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay HDR effective phase was not armed");}
    gameplayHdrCaptureStates_[captureIndex_]=GameplayHdrCaptureState3D::SceneBound;
  }
  std::filesystem::path path;
  if(capturePairMetadata_.sequence.index==0)path=captureDirectory_/(std::filesystem::path(Names[captureIndex_]).stem().wstring()+L"-raw.png");
  else if(capturePairMetadata_.sequence.index==1)path=captureDirectory_/(std::filesystem::path(Names[captureIndex_]).stem().wstring()+L"-diagnostic.png");
  else if(capturePairMetadata_.sequence.index==2)path=captureDirectory_/(std::filesystem::path(Names[captureIndex_]).stem().wstring()+L"-reference.png");
  else{FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay capture phase index was unsupported");}
  const auto renderer=engine.GetRendererStats();captureSuccess_=renderer.successfulFrameCaptures;captureReject_=renderer.rejectedFrameCaptures;
  const bool depthRequested=engine.RequestDepthReadback3D(capturePairMetadata_),objectIdRequested=engine.RequestObjectIdReadback3D(capturePairMetadata_),frameRequested=engine.CaptureNextFrame3D(path);
  const bool hdrRequired=effective||(captureRawHdrDiagnostic_&&capturePairMetadata_.sequence.index==0);
  const bool hdrRequested=!hdrRequired||engine.RequestLinearHdrReadback3D(capturePairMetadata_);
  if(!GameplayCapturePhaseRequestsAtomic3D(depthRequested,objectIdRequested,frameRequested)||!hdrRequested){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay color/object-ID/depth/HDR phase requests were not atomic");}
  if(effective){
    if(gameplayHdrCaptureStates_[captureIndex_]!=GameplayHdrCaptureState3D::SceneBound||gameplayHdrInFlight_){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay HDR allowed only one effective-phase readback in flight");}
    gameplayHdrCaptureStates_[captureIndex_]=GameplayHdrCaptureState3D::BundleRequested;
    gameplayHdrInFlight_=1;gameplayHdrMaxInFlight_=std::max(gameplayHdrMaxInFlight_,gameplayHdrInFlight_);
    gameplayHdrCaptureStates_[captureIndex_]=GameplayHdrCaptureState3D::AwaitGpu;
  }
  capturePhaseRequestPending_=false;capturePending_=true;
}

void GameplayLabGame::AddShowcaseEnvironment(){
  showcaseSubmittedDescriptors_=0;
  showcaseSubmittedLodItems_.clear();
  showcaseRenderAssets_.ResetProductionSubmissionStats();
  const auto waterMaterial=[](const WaterSurfaceSettings3D& source){const auto settings=NormalizeWaterSurfaceSettings3D(source);Material3D material{{settings.deepColor.x,settings.deepColor.y,settings.deepColor.z,settings.deepAlpha},{settings.maximumRoughness,0,0,1},false};material.materialClass=MaterialClass3D::Water;material.doubleSided=true;material.normalScale=1.f+settings.broadWaveStrength*4.f+settings.detailWaveStrength*2.f;return material;};
  std::size_t descriptorIndex{};
  std::uint32_t lodObjectIndex{};
  for(const auto& descriptor:showcaseEnvironment_.Descriptors()){
    const std::uint64_t stableLow=++descriptorIndex;
    const RenderObjectIdentity3D renderIdentity{{EnvironmentPartitionId,stableLow},1};
    bool submitted=false;
    std::vector<ShowcaseProductionSubmittedLodChain3D> productionChains;
    Transform3D transform{descriptor.position,{0,descriptor.yaw,0},descriptor.scale};
    const MeshResource3D* grassHigh=&showcaseGrassHighMesh_;
    const MeshResource3D* grassLow=&showcaseGrassLowMesh_;
    const Aevrynd::Material3D* groundCoverSource{};
    GameplayForestGroundCoverVariant3D groundCoverVariant=GameplayForestGroundCoverVariant3D::Fern;
    if(descriptor.kind==ShowcaseSceneKind3D::Grass){
      if(descriptor.meshAsset.find("grass-blade")!=std::string::npos){
        grassHigh=&showcaseTreeLowMesh_;grassLow=&showcaseRockLowMesh_;
        groundCoverSource=showcaseRenderAssets_.ProductionMaterialById("island_tree_01:leaves");
        groundCoverVariant=GameplayForestGroundCoverVariant3D::Riverbank;
      }else if(descriptor.meshAsset.find("grass-fan")!=std::string::npos){
        grassHigh=&showcaseFernHighMesh_;grassLow=&showcaseFernLowMesh_;
        groundCoverSource=showcaseRenderAssets_.ProductionMaterialById("island_tree_01:leaves");
        groundCoverVariant=GameplayForestGroundCoverVariant3D::Fern;
      }else if(descriptor.meshAsset.find("grass-tuft")!=std::string::npos){
        grassHigh=&showcaseShrubHighMesh_;grassLow=&showcaseShrubLowMesh_;
        groundCoverSource=showcaseRenderAssets_.ProductionMaterialById("tree_small_02:leaves");
        groundCoverVariant=GameplayForestGroundCoverVariant3D::Community;
      }
      if(groundCoverSource)
        transform.scale=GameplayForestGroundCoverWorldScale3D(transform.scale);
    }
    switch(descriptor.kind){
    case ShowcaseSceneKind3D::Terrain:
      if(showcaseGroundMaterial_.textures){auto material=showcaseGroundMaterial_;material.tint=ShowcaseForestTerrainTint3D();material.materialClass=MaterialClass3D::Terrain;material.normalScale=TerrainAuthoredBiomeNormalScaleMarker3D;material.textureScale=ShowcaseForestTerrainTextureScale3D();material.useUv=true;const auto before=scene_.MeshCount();const Transform3D terrainTransform{descriptor.position,{0,descriptor.yaw,0},{1,1,1}};scene_.Mesh(showcaseTerrainMesh_,terrainTransform,material,{},renderIdentity);submitted=scene_.MeshCount()==before+1;}break;
    case ShowcaseSceneKind3D::Water:
      {const auto before=scene_.MeshCount();scene_.Mesh(showcaseRiverMesh_,transform,waterMaterial(showcaseEnvironment_.WaterSurfaceSettings()),{},renderIdentity);submitted=scene_.MeshCount()==before+1;}break;
    case ShowcaseSceneKind3D::Tree:
      {const std::string_view asset=ResolveShowcasePhotorealAssetId3D(descriptor);if(!asset.empty()){transform.position.y-=ShowcaseTreeBurialOffset3D(asset,transform.scale.y);submitted=showcaseRenderAssets_.SubmitProductionMesh(scene_,asset,transform,renderIdentity,lodObjectIndex,&productionChains);}}break;
    case ShowcaseSceneKind3D::Grass:
      {const auto* source=groundCoverSource?groundCoverSource:showcaseRenderAssets_.ProductionMaterial(ShowcaseForestGrassMaterialAsset3D(descriptor.materialAsset));
       if(source){const auto material=groundCoverSource?PrepareGameplayForestGroundCoverMaterial3D(*source,groundCoverVariant,descriptor.castsShadow):ShowcaseForestGrassMaterial3D(*source,descriptor.materialAsset,descriptor.castsShadow);const auto before=scene_.MeshCount();scene_.MeshLod(*grassHigh,*grassLow,ShowcaseGrassLodPolicy3D(),transform,material,material,{},renderIdentity);submitted=scene_.MeshCount()==before+1;}}break;
    case ShowcaseSceneKind3D::Rock:
      submitted=showcaseRenderAssets_.SubmitProductionMesh(scene_,descriptor.meshAsset.find("boulder_01")!=std::string::npos?"boulder_01":"coastal_cliff_01",transform,renderIdentity,lodObjectIndex,&productionChains);break;
    case ShowcaseSceneKind3D::Building:
      {const auto role=GameplayCottageBaseMaterialRole3D(descriptor.meshAsset);const bool roof=role==GameplayCottageMaterialRole3D::RoofTile;const auto* brickSource=showcaseRenderAssets_.ProductionMaterial("Bricks100");const auto* roofSource=showcaseRenderAssets_.ProductionMaterial("RoofingTiles014B");if(!brickSource||!roofSource||!showcaseWoodMaterial_.textures)break;auto material=role==GameplayCottageMaterialRole3D::Brick?*brickSource:role==GameplayCottageMaterialRole3D::RoofTile?*roofSource:showcaseWoodMaterial_;
       // 分段牆網格以世界空間連續取樣 WoodSiding006；屋瓦只在坡面沿建好的 UV 取樣。
       // 材質回應只覆寫既有 PBR 參數；mesh、transform、identity 與 draw 路徑保持不變。
       if(!ApplyGameplayCottageMaterialResponse3D(material,role))break;const auto before=scene_.MeshCount();
       const MeshResource3D* mesh=&boxMesh_;if(descriptor.meshAsset.find("front")!=std::string::npos)mesh=&showcaseCottageFrontMesh_;else if(descriptor.meshAsset.find("back")!=std::string::npos)mesh=&showcaseCottageBackMesh_;else if(descriptor.meshAsset.find("left")!=std::string::npos||descriptor.meshAsset.find("right")!=std::string::npos)mesh=&showcaseCottageSideMesh_;else if(roof)mesh=&showcaseCottageRoofMesh_;scene_.Mesh(*mesh,transform,material,{},renderIdentity);
       auto glass=material;if(!ApplyGameplayCottageWindowResponse3D(glass,rules_.Lighting()))break;
       // 裝飾件使用已載入的 WoodSiding006，不再從基座或屋頂複製磚／瓦紋。
       auto wood=showcaseWoodMaterial_;wood.tint={.58f,.36f,.19f,1};wood.surface.roughness=.62f;wood.surface.metallic=0;wood.surface.emissive=.006f;wood.textureScale=.16f;wood.useUv=false;
       auto doorMaterial=wood;doorMaterial.tint={.36f,.17f,.07f,1};
       const auto framedWindowZ=[&](float x,float z,float facing){scene_.Mesh(boxMesh_,{{x,1.95f,z},{0,0,0},{1.35f,1.15f,.12f}},glass,{},renderIdentity);const float frameZ=z+facing*.11f;for(float side:{-.76f,.76f})scene_.Mesh(boxMesh_,{{x+side,1.95f,frameZ},{0,0,0},{.12f,1.48f,.16f}},wood,{},renderIdentity);for(float y:{1.27f,2.63f})scene_.Mesh(boxMesh_,{{x,y,frameZ},{0,0,0},{1.64f,.12f,.16f}},wood,{},renderIdentity);scene_.Mesh(boxMesh_,{{x,1.95f,frameZ+facing*.02f},{0,0,0},{.09f,1.22f,.13f}},wood,{},renderIdentity);scene_.Mesh(boxMesh_,{{x,1.95f,frameZ+facing*.02f},{0,0,0},{1.42f,.09f,.13f}},wood,{},renderIdentity);};
       const auto framedWindowX=[&](float x,float facing){scene_.Mesh(boxMesh_,{{x,1.95f,24},{0,0,0},{.12f,1.15f,1.45f}},glass,{},renderIdentity);const float frameX=x+facing*.11f;for(float side:{-.81f,.81f})scene_.Mesh(boxMesh_,{{frameX,1.95f,24+side},{0,0,0},{.16f,1.48f,.12f}},wood,{},renderIdentity);for(float y:{1.27f,2.63f})scene_.Mesh(boxMesh_,{{frameX,y,24},{0,0,0},{.16f,.12f,1.82f}},wood,{},renderIdentity);scene_.Mesh(boxMesh_,{{frameX+facing*.02f,1.95f,24},{0,0,0},{.13f,1.22f,.09f}},wood,{},renderIdentity);scene_.Mesh(boxMesh_,{{frameX+facing*.02f,1.95f,24},{0,0,0},{.13f,.09f,1.52f}},wood,{},renderIdentity);};
       if(descriptor.meshAsset.find("front")!=std::string::npos){
         scene_.Mesh(boxMesh_,{{-12,1.35f,20.17f},{0,0,0},{1.45f,2.1f,.12f}},doorMaterial,{},renderIdentity);
         // 門窗框、窗台與前階共享原建築identity，增加輪廓細節但不改變descriptor或碰撞代理。
         for(float x:{-14.1f,-9.9f})framedWindowZ(x,20.16f,-1.f);
         for(float side:{-.82f,.82f})scene_.Mesh(boxMesh_,{{-12+side,1.42f,20.06f},{0,0,0},{.14f,2.45f,.18f}},wood,{},renderIdentity);
         scene_.Mesh(boxMesh_,{{-12,2.69f,20.06f},{0,0,0},{1.78f,.14f,.18f}},wood,{},renderIdentity);
         for(float y:{.83f,1.38f,1.93f})scene_.Mesh(boxMesh_,{{-12,y,20.04f},{0,0,0},{1.12f,.08f,.16f}},wood,{},renderIdentity);
         scene_.Mesh(boxMesh_,{{-12,.16f,19.55f},{0,0,0},{2.45f,.26f,1.05f}},doorMaterial,{},renderIdentity);
         // R42_COTTAGE_LANTERN_BEGIN
         // The porch light now has a visible physical source at the exact point-light
         // origin. Seven render-only pieces reuse resident meshes and add no textures.
         auto lanternMetal=wood;lanternMetal.textures={};
         lanternMetal.tint={.075f,.065f,.055f,1.f};
         lanternMetal.surface={.28f,.78f,.015f,1.f};
         lanternMetal.textureScale=1.f;lanternMetal.normalScale=0.f;
         lanternMetal.useUv=false;
         auto lanternGlass=glass;lanternGlass.textures={};
         lanternGlass.tint={1.f,.42f,.12f,1.f};
         lanternGlass.surface={.12f,.04f,1.75f,.92f};
         lanternGlass.textureScale=1.f;lanternGlass.normalScale=0.f;
         lanternGlass.useUv=false;lanternGlass.castsShadow=false;
         struct CottageLanternPart3D{Transform3D transform;bool cylindrical;bool glass;};
         const Vec3 lantern=GameplayCottagePorchLanternCenter3D;
         const std::array<CottageLanternPart3D,7> lanternParts{{
           {{{lantern.x,lantern.y,lantern.z},{0,0,0},{.38f,.42f,.30f}},false,true},
           {{{lantern.x,lantern.y+.27f,lantern.z},{0,0,0},{.48f,.10f,.40f}},true,false},
           {{{lantern.x,lantern.y-.27f,lantern.z},{0,0,0},{.48f,.10f,.40f}},true,false},
           {{{lantern.x-.21f,lantern.y,lantern.z-.17f},{0,0,0},{.045f,.50f,.045f}},false,false},
           {{{lantern.x+.21f,lantern.y,lantern.z-.17f},{0,0,0},{.045f,.50f,.045f}},false,false},
           {{{lantern.x,lantern.y+.48f,lantern.z+.28f},{0,0,0},{.08f,.48f,.08f}},false,false},
           {{{lantern.x,lantern.y+.55f,lantern.z+.14f},{1.5708f,0,0},{.08f,.32f,.08f}},true,false}}};
         if(lanternParts.size()!=7)
           throw std::runtime_error("Cottage lantern part count gate failed");
         for(const auto& part:lanternParts){
           const auto& resource=part.cylindrical?showcaseCylinderMesh_:boxMesh_;
           const auto bounds=TransformBounds(resource.bounds,part.transform);
           const Vec3 minimum=bounds.center-bounds.extents,maximum=bounds.center+bounds.extents;
           if(minimum.x<lantern.x-.32f||maximum.x>lantern.x+.32f||
              minimum.y<lantern.y-.36f||maximum.y>lantern.y+.76f||
              minimum.z<lantern.z-.25f||maximum.z>lantern.z+.38f)
             throw std::runtime_error("Cottage lantern part exceeded fixture bounds");
         }
         const auto lanternDrawsBefore=scene_.MeshCount();
         for(const auto& part:lanternParts)
           scene_.Mesh(part.cylindrical?showcaseCylinderMesh_:boxMesh_,part.transform,
               part.glass?lanternGlass:lanternMetal,{},renderIdentity);
         if(scene_.MeshCount()!=lanternDrawsBefore+lanternParts.size())
           throw std::runtime_error("Cottage lantern draw count gate failed");
         // R42_COTTAGE_LANTERN_END
         // R34_COTTAGE_LIFE_BEGIN
         // Twelve opaque, render-only pieces remain inside the cottage footprint:
         // a log stack, bench, crates, and paired threshold stones.
         struct CottageLifeDetail3D{Transform3D transform;bool cylindrical;bool brick;};
          const std::array<CottageLifeDetail3D,12> lifeDetails{{
           {{{-15.2f,.22f,26.55f},{0,0,1.5708f},{.36f,.90f,.36f}},true,false},
           {{{-15.2f,.22f,27.02f},{0,0,1.5708f},{.36f,.90f,.36f}},true,false},
           {{{-15.2f,.50f,26.78f},{0,0,1.5708f},{.36f,.90f,.36f}},true,false},
           {{{-14.55f,.22f,26.55f},{0,0,1.5708f},{.36f,.90f,.36f}},true,false},
           {{{-14.55f,.22f,27.02f},{0,0,1.5708f},{.36f,.90f,.36f}},true,false},
           {{{-8.75f,.58f,24.9f},{0,0,0},{.55f,.16f,2.f}},false,false},
           {{{-8.75f,.28f,24.22f},{0,0,0},{.42f,.55f,.18f}},false,false},
           {{{-8.75f,.28f,25.58f},{0,0,0},{.42f,.55f,.18f}},false,false},
           {{{-15.05f,.42f,21.25f},{0,.12f,0},{.78f,.78f,.78f}},false,false},
           {{{-14.18f,.32f,21.55f},{0,-.16f,0},{.58f,.58f,.58f}},false,false},
            {{{-12.f,.12f,19.72f},{0,0,0},{1.15f,.18f,.32f}},false,true},
            {{{-10.72f,.10f,19.72f},{0,0,0},{.95f,.14f,.32f}},false,true}}};
          const auto finite=[](Vec3 value){return std::isfinite(value.x)&&
              std::isfinite(value.y)&&std::isfinite(value.z);};
          if(lifeDetails.size()!=12)
            throw std::runtime_error("Cottage life detail count gate failed");
          for(const auto& detail:lifeDetails){
            if(!finite(detail.transform.position)||!finite(detail.transform.rotation)||
               !finite(detail.transform.scale)||detail.transform.scale.x<=0||
               detail.transform.scale.y<=0||detail.transform.scale.z<=0)
              throw std::runtime_error("Cottage life detail finite transform gate failed");
            const auto& resource=detail.cylindrical?showcaseCylinderMesh_:boxMesh_;
            const auto bounds=TransformBounds(resource.bounds,detail.transform);
            const Vec3 minimum=bounds.center-bounds.extents,maximum=bounds.center+bounds.extents;
            if(minimum.x< -16.5f||maximum.x> -7.5f||minimum.y<-.001f||
               minimum.z<19.5f||maximum.z>28.5f)
              throw std::runtime_error("Cottage life detail exceeded cottage footprint");
          }
          const auto lifeDrawsBefore=scene_.MeshCount();
          for(const auto& detail:lifeDetails)
            scene_.Mesh(detail.cylindrical?showcaseCylinderMesh_:boxMesh_,
                detail.transform,detail.brick?*brickSource:showcaseWoodMaterial_,
                {},renderIdentity);
          if(scene_.MeshCount()!=lifeDrawsBefore+lifeDetails.size())
            throw std::runtime_error("Cottage life detail draw count gate failed");
         // R34_COTTAGE_LIFE_END
       }
       else if(descriptor.meshAsset.find("back")!=std::string::npos)for(float x:{-14.1f,-9.9f})framedWindowZ(x,27.84f,1.f);
       else if(descriptor.meshAsset.find("left")!=std::string::npos||descriptor.meshAsset.find("right")!=std::string::npos){const bool left=descriptor.meshAsset.find("left")!=std::string::npos;framedWindowX(left?-16.19f:-7.81f,left?-1.f:1.f);}
        else if(roof){
          auto brick=*brickSource;if(!ApplyGameplayCottageMaterialResponse3D(brick,GameplayCottageMaterialRole3D::Brick))break;
         if(!ApplyGameplayCottageChimneyStoneResponse3D(brick))break;
         scene_.Mesh(boxMesh_,{{-14.45f,4.72f,25.3f},{0,0,0},{.72f,2.15f,.72f}},brick,{},renderIdentity);
         scene_.Mesh(boxMesh_,{{-14.45f,5.84f,25.3f},{0,0,0},{.94f,.16f,.94f}},brick,{},renderIdentity);
         for(float z:{19.78f,28.22f})scene_.Mesh(boxMesh_,{{-12,3.48f,z},{0,0,0},{9.25f,.18f,.24f}},wood,{},renderIdentity);
         for(float x:{-16.38f,-7.62f})scene_.Mesh(boxMesh_,{{x,3.48f,24},{0,0,0},{.24f,.18f,8.2f}},wood,{},renderIdentity);
       }
       submitted=scene_.MeshCount()>before;}break;
    }
    if(submitted){
      ++showcaseSubmittedDescriptors_;
      for(const auto& chain:productionChains){
        const auto bounds=TransformSphereBounds(chain.localUnionBounds,chain.normalizedTransform);
        showcaseSubmittedLodItems_.push_back({stableLow,std::uint32_t(renderIdentity.generation),chain.submesh,descriptor.kind,bounds.center,bounds.radius,chain.switchDistances,chain.transitionWidths,chain.levelCount,chain.lodSeedBase});
      }
      lodObjectIndex+=std::uint32_t(productionChains.size());
      if(descriptor.kind==ShowcaseSceneKind3D::Grass){
        const Vec3 minimum{
          (std::min)(grassHigh->bounds.center.x-grassHigh->bounds.extents.x,grassLow->bounds.center.x-grassLow->bounds.extents.x),
          (std::min)(grassHigh->bounds.center.y-grassHigh->bounds.extents.y,grassLow->bounds.center.y-grassLow->bounds.extents.y),
          (std::min)(grassHigh->bounds.center.z-grassHigh->bounds.extents.z,grassLow->bounds.center.z-grassLow->bounds.extents.z)};
        const Vec3 maximum{
          (std::max)(grassHigh->bounds.center.x+grassHigh->bounds.extents.x,grassLow->bounds.center.x+grassLow->bounds.extents.x),
          (std::max)(grassHigh->bounds.center.y+grassHigh->bounds.extents.y,grassLow->bounds.center.y+grassLow->bounds.extents.y),
          (std::max)(grassHigh->bounds.center.z+grassHigh->bounds.extents.z,grassLow->bounds.center.z+grassLow->bounds.extents.z)};
        MeshBounds3D localBounds{(minimum+maximum)*.5f,Length((maximum-minimum)*.5f),(maximum-minimum)*.5f};
        const auto bounds=TransformSphereBounds(localBounds,transform);
        const auto policy=ShowcaseGrassLodPolicy3D();
        showcaseSubmittedLodItems_.push_back({stableLow,std::uint32_t(renderIdentity.generation),0,descriptor.kind,bounds.center,bounds.radius,{policy.switchDistance,0,0},{policy.transitionWidth,0,0},2,lodObjectIndex*5u+3u});
        showcaseSubmittedLodItems_.back().lodSeedBase&=7u;
        ++lodObjectIndex;
      }
    }
  }
}
bool GameplayLabGame::WriteCaptureIntegrityDiagnostic(bool fatal)const{
  if(captureDirectory_.empty())return false;
  const auto destination=captureDirectory_/(fatal?"gameplay-determinism-integrity-fatal.json":"gameplay-determinism-phase-diagnostic.json");
  const auto temporary=destination.wstring()+L".tmp";
  std::ofstream output(std::filesystem::path(temporary),std::ios::binary|std::ios::trunc);if(!output)return false;
  output<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabFrameBoundDeterminismDiagnostic\",\"fatal\":"<<(fatal?1:0)<<",\"phases\":[";
  bool first=true;for(std::size_t captureIndex=0;captureIndex<captureIntegrityDiagnostics_.size();++captureIndex)for(const auto& phase:captureIntegrityDiagnostics_[captureIndex]){if(!phase.recorded)continue;if(!first)output<<',';first=false;output<<"{\"name\":\""<<std::filesystem::path(Names[captureIndex]).string()<<"\",\"captureIndex\":"<<captureIndex<<",\"phaseIndex\":"<<phase.phaseIndex<<",\"objectFrameSerial\":"<<phase.objectId.frameSerial<<",\"depthFrameSerial\":"<<phase.depth.frameSerial<<",\"pairedExact\":"<<(phase.objectId==phase.depth?1:0);WriteIntegrityJson(output,phase.objectId.frameSerial,phase.objectId.integrity,phase.objectId.canonical,&phase.objectId.render);output<<'}';}output<<"]}";output.flush();if(!output){output.close();std::filesystem::remove(temporary);return false;}output.close();
  if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary);return false;}return true;
}

bool GameplayLabGame::ConsumeObjectIdCapture(Engine &engine,std::size_t index,bool raw,bool reference){
  captureReadbackFailure_="unclassified-readback-failure";
  const auto failReadback=[&](std::string reason){captureReadbackFailure_=std::move(reason);std::ofstream output(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc);output<<"index="<<index<<" raw="<<raw<<" reference="<<reference<<" reason="<<captureReadbackFailure_;output.flush();return false;};
  if(index>=Names.size())return failReadback("capture-index-out-of-range");
  const bool effective=!raw&&!reference,rawHdrDiagnostic=raw&&captureRawHdrDiagnostic_,bundled=effective||rawHdrDiagnostic;
  std::optional<ProductionReadbackBundle3D> productionBundle;std::optional<ObjectIdCapture3D> objectIdStorage;std::optional<DepthCapture3D> depthStorage;if(bundled)productionBundle=engine.TakeProductionReadbackBundle3D();else{objectIdStorage=engine.TakeObjectIdReadback3D();depthStorage=engine.TakeDepthReadback3D();}
  const ObjectIdCapture3D* capture=bundled&&productionBundle?&productionBundle->ObjectId():objectIdStorage?&*objectIdStorage:nullptr;const DepthCapture3D* depth=bundled&&productionBundle?&productionBundle->Depth():depthStorage?&*depthStorage:nullptr;const LinearHdrReadback3D* hdr=bundled&&productionBundle?&productionBundle->Hdr():nullptr;
  if(!capture||!depth||(bundled&&!hdr)){return failReadback("no-paired-readback object="+std::to_string(bool(capture))+" depth="+std::to_string(bool(depth))+" hdr="+std::to_string(bool(hdr)));}
  if(effective){
    constexpr std::uint64_t MaximumEngineHdrBytes=16ull*1024ull*1024ull;
    if(gameplayHdrCaptureStates_[index]!=GameplayHdrCaptureState3D::AwaitGpu||gameplayHdrInFlight_!=1||!hdr->Complete()||hdr->receipt.requestMetadata!=capture->requestMetadata||hdr->receipt.rasterProvenance!=capture->rasterProvenance||hdr->receipt.determinism!=capture->determinism||hdr->receipt.requestMetadata!=depth->requestMetadata||hdr->receipt.rasterProvenance!=depth->rasterProvenance||hdr->receipt.determinism!=depth->determinism||hdr->receipt.requestMetadata!=capturePairMetadata_||hdr->receipt.requestMetadata.sequence.index!=1||hdr->receipt.requestMetadata.sequence.token!=captureSequenceToken_||hdr->receipt.rawHalfByteCount>MaximumEngineHdrBytes){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=hdr-frame-bound-pair-mismatch state="<<GameplayHdrStateName3D(gameplayHdrCaptureStates_[index])<<" bytes="<<hdr->receipt.rawHalfByteCount;return false;}
    if(capture->materialBindings.empty()||capture->materialBindings.size()!=capture->tokenMap.size())return failReadback("material-binding-count-mismatch");
    for(std::size_t bindingIndex=0;bindingIndex<capture->materialBindings.size();++bindingIndex){const auto& binding=capture->materialBindings[bindingIndex];if(!binding.Complete()||binding.order!=bindingIndex||binding.identity!=capture->tokenMap[bindingIndex])return failReadback("material-binding-identity-mismatch");}
    gameplayHdrEnginePeakBytes_=std::max(gameplayHdrEnginePeakBytes_,std::uint64_t(hdr->receipt.rawHalfByteCount));gameplayHdrInFlight_=0;gameplayHdrCaptureStates_[index]=GameplayHdrCaptureState3D::Taken;
  }
  const std::uint32_t diagnosticPhase=capture->requestMetadata.sequence.index;
  if(diagnosticPhase>=captureIntegrityDiagnostics_[index].size())return failReadback("diagnostic-phase-out-of-range");
  captureIntegrityDiagnostics_[index][diagnosticPhase]={index,diagnosticPhase,capture->determinism,depth->determinism,true};
  if(!WriteCaptureIntegrityDiagnostic())return failReadback("integrity-diagnostic-publication-failed");
  if(!capture->Complete()||!depth->Complete()||depth->width!=capture->width||depth->height!=capture->height||!ValidateGameplayFrameBoundRenderPair3D(capture->determinism,depth->determinism,capture->rasterProvenance,depth->rasterProvenance,capture->requestMetadata,depth->requestMetadata)){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=typed-readback-pair-mismatch objectComplete="<<capture->Complete()<<" depthComplete="<<depth->Complete()<<" objectFrame="<<capture->rasterProvenance.frameSerial<<" depthFrame="<<depth->rasterProvenance.frameSerial<<" determinismExact="<<(depth->determinism==capture->determinism)<<" renderExact="<<(depth->determinism.render==capture->determinism.render);WriteCaptureIntegrityDiagnostic(true);return false;}
  const auto& render=capture->determinism.render;if(!render.Complete()||render.frameSerial!=capture->rasterProvenance.frameSerial||render.sceneDrawRevision!=capture->requestMetadata.drawListRevision){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=frame-bound-render-provenance frame="<<render.frameSerial<<":"<<capture->rasterProvenance.frameSerial<<" sceneRevision="<<render.sceneDrawRevision<<":"<<capture->requestMetadata.drawListRevision;WriteCaptureIntegrityDiagnostic(true);return false;}captureFrameSceneDrawRevision_[index]=render.sceneDrawRevision;captureFrameWorldRevision_[index]=render.worldRevision;captureFrameSubmittedMeshes_[index]=render.submittedMeshInstances;captureFrameSubmittedSkinned_[index]=render.submittedSkinnedInstances;captureFrameSubmittedTerrain_[index]=render.submittedTerrainInstances;captureFrameSubmittedWorldVertices_[index]=render.submittedWorldVertices;captureFrameSubmittedWorldIndices_[index]=render.submittedWorldIndices;captureRendererSubmittedMeshes_[index]=std::uint32_t(std::min<std::size_t>(render.submittedMeshInstances,(std::numeric_limits<std::uint32_t>::max)()));captureRendererSubmittedTerrain_[index]=std::uint32_t(std::min<std::size_t>(render.submittedTerrainInstances,(std::numeric_limits<std::uint32_t>::max)()));
  const auto* phaseSceneStats=captureScenePhaseLedger_.Resolve(capture->requestMetadata.sequence.token,index,capture->requestMetadata.sequence.index);
  if(!phaseSceneStats||phaseSceneStats->drawListRevision!=capture->requestMetadata.drawListRevision){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=scene-submission-provenance phase="<<capture->requestMetadata.sequence.index<<" serial="<<(phaseSceneStats?phaseSceneStats->submissionSerial:0)<<" sceneRevision="<<(phaseSceneStats?phaseSceneStats->drawListRevision:0)<<" requestRevision="<<capture->requestMetadata.drawListRevision<<" identityOrder="<<(phaseSceneStats?phaseSceneStats->acceptedIdentityOrderFingerprint:0);return false;}
  const std::uint64_t femalePoseHash=HashPose(femalePose_),malePoseHash=HashPose(malePose_),monsterPoseHash=HashPose(monsterPose_);
  if(capture->width!=1280||capture->height!=720||capture->pixels.size()!=std::size_t(capture->width)*capture->height||capture->submittedOpaqueStatic>(std::numeric_limits<std::uint32_t>::max)()){
    std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=shape width="<<capture->width<<" height="<<capture->height<<" pixels="<<capture->pixels.size();return false;
  }
  std::vector<std::uint64_t> counts(capture->tokenMap.size()+1);
  std::uint64_t hash=14695981039346656037ull;
  const auto mix=[&](std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};
  for(const auto& identity:capture->tokenMap){mix(identity.id.high);mix(identity.id.low);mix(identity.generation);}
  for(const auto token:capture->pixels){if(token>capture->tokenMap.size())return failReadback("pixel-token-out-of-range");++counts[token];mix(token);}
  if(!reference){
    // 無損 token 平面與完整身份側車共同綁定本次 readback；未知或水面保守視為動態。
    const auto stem=std::filesystem::path(Names[index]).stem().string()+(raw?"-raw":"");
    const auto planePath=captureDirectory_/(stem+"-tokens.bin");
    std::ofstream plane(planePath,std::ios::binary|std::ios::trunc);
    for(const auto token:capture->pixels)for(unsigned byte=0;byte<4;++byte)plane.put(char((token>>(byte*8))&255));
    plane.flush();if(!plane)return failReadback("token-plane-publication-failed");plane.close();
    std::array<std::byte,32> planeSha{};if(!CaptureFileSha256(planePath,planeSha))return failReadback("token-plane-hash-failed");
    const auto metadataPath=captureDirectory_/(stem+"-tokens.json");
    std::ofstream metadata(metadataPath,std::ios::binary|std::ios::trunc);
    metadata<<"{\"source\":\"GameplayLosslessTokens\",\"version\":1,\"runId\":\""<<runId_<<"\",\"width\":"<<capture->width<<",\"height\":"<<capture->height<<",\"frameSerial\":\""<<capture->rasterProvenance.frameSerial<<"\",\"tokenGeneration\":\""<<capture->rasterProvenance.tokenGeneration<<"\",\"phase\":"<<(raw?0:1)<<",\"plane\":\""<<planePath.filename().string()<<"\",\"planeSha256\":\""<<CaptureShaHex(planeSha)<<"\",\"identities\":[";
    const auto descriptors=showcaseEnvironment_.Descriptors();
    for(std::size_t token=0;token<capture->tokenMap.size();++token){
      const auto identity=capture->tokenMap[token];
      const bool knownStatic=identity.id.high==EnvironmentPartitionId&&identity.generation==1&&identity.id.low>=1&&identity.id.low<=descriptors.size()&&descriptors[std::size_t(identity.id.low-1)].kind!=ShowcaseSceneKind3D::Water;
      if(token)metadata<<',';
      metadata<<"{\"high\":\""<<identity.id.high<<"\",\"low\":\""<<identity.id.low<<"\",\"generation\":\""<<identity.generation<<"\",\"class\":\""<<(knownStatic?"static":"dynamic")<<"\",\"opacityAffected\":"<<(identity==cameraPolicyFadeIdentity_?"true":"false")<<'}';
    }
    metadata<<"]}";metadata.flush();if(!metadata)return failReadback("token-metadata-publication-failed");metadata.close();
    if(!CaptureFileSha256(metadataPath,raw?captureRawTokenMetadataSha256_[index]:captureEffectiveTokenMetadataSha256_[index]))return failReadback("token-metadata-hash-failed");
  }
  const auto buildingIdentities=BuildGameplayBuildingIdentities3D(showcaseEnvironment_.Descriptors(),EnvironmentPartitionId);
  std::array<float,6> buildingCoverage{};
  float buildingUnion{};
  std::uint32_t maximumVisibleToken{};
  for(std::uint32_t token=1;token<=capture->tokenMap.size();++token){
    const auto& identity=capture->tokenMap[token-1];
    if(!maximumVisibleToken||counts[token]>counts[maximumVisibleToken])maximumVisibleToken=token;
    const auto part=GameplayBuildingPart3D(buildingIdentities,identity);
    if(part<buildingIdentities.size()){
      const float coverage=float(counts[token])/float(capture->pixels.size());
      buildingCoverage[part]+=coverage;
      buildingUnion+=coverage;
    }
  }
  const auto maximumVisibleIdentity=maximumVisibleToken?capture->tokenMap[maximumVisibleToken-1]:RenderObjectIdentity3D{};
  const float maximumVisibleCoverage=maximumVisibleToken?float(counts[maximumVisibleToken])/float(capture->pixels.size()):0.f;
  std::uint32_t dominantToken{},playerToken{},terrainToken{},companionToken{},firstPersonArmsToken{},weaponToken{};
  for(std::uint32_t token=1;token<=capture->tokenMap.size();++token){
    const auto& identity=capture->tokenMap[token-1];
    if(identity==RenderObjectIdentity3D{{GameplayCharacterIdentityHigh,1},1})playerToken=token;
    if(identity==RenderObjectIdentity3D{{GameplayCharacterIdentityHigh,2},1})companionToken=token;
    if(identity==GameplayCaptureFirstPersonArmsIdentity3D)firstPersonArmsToken=token;
    if(identity==GameplayCaptureWeaponIdentity3D)weaponToken=token;
    if(identity==RenderObjectIdentity3D{{EnvironmentPartitionId,1},1})terrainToken=token;
    if(reference){if(identity==RenderObjectIdentity3D{{GameplayCharacterIdentityHigh,1},1})dominantToken=token;}
    else if(raw){if(GameplayBuildingPart3D(buildingIdentities,identity)==buildingIdentities.size())continue;if(!dominantToken||counts[token]>counts[dominantToken])dominantToken=token;}
    else if(identity==RenderObjectIdentity3D{{captureOccluderStableHigh_[index],captureOccluderStableLow_[index]},captureOccluderGeneration_[index]})dominantToken=token;
  }
  const auto isPlayerPixel=[&](std::uint32_t token){return token&&token<=capture->tokenMap.size()&&capture->tokenMap[token-1]==RenderObjectIdentity3D{{GameplayCharacterIdentityHigh,1},1};};
  const auto buildGpuContact=[&](std::uint64_t poseHash){
    const auto footBand=SelectGameplayGpuFootBand3D(capture->width,capture->height,capture->pixels,playerToken);
    if(!footBand.Complete())return GameplayGpuContactArtifact3D{};
    auto contactIds=capture->pixels;
    for(unsigned y=0;y<footBand.beginY;++y)for(unsigned x=0;x<capture->width;++x){
      const auto pixel=std::size_t(y)*capture->width+x;
      if(contactIds[pixel]==playerToken)contactIds[pixel]=0;
    }
    return BuildGameplayGpuContactArtifact3D(capture->width,capture->height,contactIds,depth->pixels,playerToken,terrainToken,capture->rasterProvenance,poseHash);
  };
  if(!raw){capturePlayerToken_[index]=playerToken;captureTerrainToken_[index]=terrainToken;}
  std::vector<std::uint8_t> mask(capture->pixels.size()*4);
  for(std::size_t pixel=0;pixel<capture->pixels.size();++pixel){const auto token=capture->pixels[pixel];if(!token)continue;const auto& identity=capture->tokenMap[token-1];std::uint64_t color=StableObjectIdScalarHash3D(identity.id)^identity.generation;mask[pixel*4+0]=std::uint8_t(48+(color&191));mask[pixel*4+1]=std::uint8_t(48+((color>>8)&191));mask[pixel*4+2]=std::uint8_t(48+((color>>16)&191));mask[pixel*4+3]=255;if(token==dominantToken)mask[pixel*4+0]=mask[pixel*4+1]=mask[pixel*4+2]=255;}
  const auto stem=std::filesystem::path(Names[index]).stem().wstring();
  const auto maskName=stem+(reference?L"-reference-object-id.png":raw?L"-raw-object-id.png":L"-object-id.png");
  const auto depthName=stem+(reference?L"-reference-depth.png":raw?L"-raw-depth.png":L"-depth.png");
  if(!EncodeFrameCapturePng3D(captureDirectory_/maskName,capture->width,capture->height,std::size_t(capture->width)*4,mask))return failReadback("object-id-png-publication-failed");
  PublishedFrameCapture3D depthPublication{};const auto depthPath=captureDirectory_/depthName;
  if(!EncodeDepthCapture(depthPath,*depth,depthPublication)||!ValidatePublishedFrameCapture3D(depthPath,depthPublication))return failReadback("depth-png-publication-failed");
  if(!capture->Complete()){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" raw="<<raw<<" reason=incomplete opaque="<<capture->opaqueStaticComplete<<" masked="<<capture->maskedStaticComplete<<" skinned="<<capture->skinnedComplete<<" submittedOpaque="<<capture->submittedOpaqueStatic<<" submittedSkinned="<<capture->submittedSkinned<<" rejectedMissing="<<capture->rejectedMissingIdentity<<" rejectedMasked="<<capture->rejectedMaskedStatic<<" rejectedSkinned="<<capture->rejectedSkinned<<" generationConflicts="<<capture->rejectedGenerationConflict;return false;}
  if(reference){
    const auto& effectiveProvenance=captureEffectiveRasterProvenance_[index];
    if(capture->rasterProvenance.frameSerial<=effectiveProvenance.frameSerial||capture->rasterProvenance.tokenGeneration<=effectiveProvenance.tokenGeneration||capture->rasterProvenance.viewProjectionHash!=effectiveProvenance.viewProjectionHash||capture->rasterProvenance.temporalHistoryReadGeneration!=effectiveProvenance.temporalHistoryReadGeneration||capture->rasterProvenance.temporalHistoryReadIndex!=effectiveProvenance.temporalHistoryReadIndex||capture->rasterProvenance.jitterIndex!=effectiveProvenance.jitterIndex||capture->rasterProvenance.jitterPixels.x!=effectiveProvenance.jitterPixels.x||capture->rasterProvenance.jitterPixels.y!=effectiveProvenance.jitterPixels.y){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=reference-raster-provenance-mismatch effectiveFrame="<<effectiveProvenance.frameSerial<<" referenceFrame="<<capture->rasterProvenance.frameSerial<<" effectiveVp="<<effectiveProvenance.viewProjectionHash<<" referenceVp="<<capture->rasterProvenance.viewProjectionHash<<" effectiveJitter="<<effectiveProvenance.jitterIndex<<" referenceJitter="<<capture->rasterProvenance.jitterIndex;return false;}
    captureReferenceRasterProvenance_[index]=capture->rasterProvenance;
    captureReferenceFemalePoseHash_[index]=femalePoseHash;captureReferenceMalePoseHash_[index]=malePoseHash;captureReferenceMonsterPoseHash_[index]=monsterPoseHash;captureReferenceFootPlacementHash_[index]=captureFrozenFootPlacementHash_;
    if(capture->requestMetadata!=capturePairMetadata_||captureEffectiveAcceptedExcludingOpacity_[index]!=capture->acceptedDrawFingerprintExcludingOpacity||captureEffectiveIdentityOrderHash_[index]!=capture->acceptedIdentityOrderHash||captureReferenceOpacityMatches_[index]!=captureReferenceRemovedCount_[index]||!playerToken||captureEffectiveCharacterMask_.size()!=capture->pixels.size()){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=reference-parity metadata="<<(capture->requestMetadata==capturePairMetadata_)<<" excluding="<<captureEffectiveAcceptedExcludingOpacity_[index]<<":"<<capture->acceptedDrawFingerprintExcludingOpacity<<" order="<<captureEffectiveIdentityOrderHash_[index]<<":"<<capture->acceptedIdentityOrderHash<<" matches="<<captureReferenceOpacityMatches_[index]<<" removed="<<captureReferenceRemovedCount_[index]<<" player="<<playerToken<<" mask="<<captureEffectiveCharacterMask_.size()<<":"<<capture->pixels.size();return false;}
    std::size_t referencePixels{},effectivePixels{},intersectionPixels{},unionPixels{};
    for(std::size_t pixel=0;pixel<capture->pixels.size();++pixel){const bool referenceCharacter=isPlayerPixel(capture->pixels[pixel]);const bool effectiveCharacter=captureEffectiveCharacterMask_[pixel]!=0;referencePixels+=referenceCharacter?1u:0u;effectivePixels+=effectiveCharacter?1u:0u;intersectionPixels+=referenceCharacter&&effectiveCharacter?1u:0u;unionPixels+=referenceCharacter||effectiveCharacter?1u:0u;}
    if(!referencePixels||!unionPixels)return false;
    unsigned minimumX=capture->width,maximumX{},minimumY=capture->height,maximumY{};for(unsigned y=0;y<capture->height;++y)for(unsigned x=0;x<capture->width;++x)if(isPlayerPixel(capture->pixels[std::size_t(y)*capture->width+x])){minimumX=std::min(minimumX,x);maximumX=std::max(maximumX,x);minimumY=std::min(minimumY,y);maximumY=std::max(maximumY,y);}
    if(minimumX>maximumX||minimumY>maximumY)return false;
    capturePlayerToken_[index]=playerToken;captureTerrainToken_[index]=terrainToken;
    const unsigned roiBeginY=minimumY,roiEndY=std::min(capture->height-1,maximumY+16);std::vector<std::uint64_t> roiCounts(capture->tokenMap.size()+1);float roiDepthMin=1.f,roiDepthMax=0.f;
    for(unsigned y=roiBeginY;y<=roiEndY;++y)for(unsigned x=minimumX;x<=maximumX;++x){const auto pixel=std::size_t(y)*capture->width+x;const auto token=capture->pixels[pixel];if(token<=capture->tokenMap.size())++roiCounts[token];const float z=depth->pixels[pixel];if(std::isfinite(z)){roiDepthMin=std::min(roiDepthMin,z);roiDepthMax=std::max(roiDepthMax,z);}}
    std::uint32_t roiDominant{};std::string roiHistogram{"["};bool firstHistogram=true;for(std::uint32_t token=1;token<=capture->tokenMap.size();++token){if(!roiCounts[token])continue;if(!roiDominant||roiCounts[token]>roiCounts[roiDominant])roiDominant=token;const auto& identity=capture->tokenMap[token-1];if(!firstHistogram)roiHistogram+=',';firstHistogram=false;roiHistogram+=std::format("{{\"token\":{},\"high\":{},\"low\":{},\"generation\":{},\"pixels\":{}}}",token,identity.id.high,identity.id.low,identity.generation,roiCounts[token]);}roiHistogram+=']';captureFootRoiHistogramJson_[index]=std::move(roiHistogram);captureFootRoiDepthMin_[index]=roiDepthMin;captureFootRoiDepthMax_[index]=roiDepthMax;captureFootRoiDominantToken_[index]=roiDominant;if(roiDominant){const auto& identity=capture->tokenMap[roiDominant-1];captureFootRoiDominantHigh_[index]=identity.id.high;captureFootRoiDominantLow_[index]=identity.id.low;captureFootRoiDominantGeneration_[index]=identity.generation;}
    const auto terrainPixel=[&](std::uint32_t token){const auto* identity=capture->Resolve(token);return identity&&identity->id==StableObjectId3D{EnvironmentPartitionId,1};};
    std::vector<std::uint8_t> contactMask(mask.size(),0);
    const auto evaluateFoot=[&](unsigned beginX,unsigned endX,std::uint32_t& gap,float& depthDelta){unsigned footX{},footY{};bool footFound{};for(unsigned y=minimumY;y<=maximumY;++y)for(unsigned x=beginX;x<=endX;++x)if(isPlayerPixel(capture->pixels[std::size_t(y)*capture->width+x])&&(!footFound||y>footY)){footX=x;footY=y;footFound=true;}gap=std::numeric_limits<std::uint32_t>::max();depthDelta=1.f;if(!footFound)return false;unsigned groundX{},groundY{};for(unsigned y=footY+1;y<std::min(capture->height,footY+13);++y){const unsigned scanBegin=footX>4?footX-4:0,scanEnd=std::min(capture->width-1,footX+4);for(unsigned x=scanBegin;x<=scanEnd;++x)if(terrainPixel(capture->pixels[std::size_t(y)*capture->width+x])){gap=y-footY-1;groundX=x;groundY=y;break;}if(gap!=std::numeric_limits<std::uint32_t>::max())break;}if(gap==std::numeric_limits<std::uint32_t>::max())return false;depthDelta=depth->pixels[std::size_t(groundY)*capture->width+groundX]-depth->pixels[std::size_t(footY)*capture->width+footX];for(const auto [x,y,color]:{std::tuple{footX,footY,std::array<std::uint8_t,3>{255,32,32}},std::tuple{groundX,groundY,std::array<std::uint8_t,3>{32,255,32}}}){const auto offset=(std::size_t(y)*capture->width+x)*4;contactMask[offset]=color[0];contactMask[offset+1]=color[1];contactMask[offset+2]=color[2];contactMask[offset+3]=255;}return gap<=3&&depthDelta>=-0.0001f&&depthDelta<=.02f;};
    std::uint32_t referenceLeftGap{},referenceRightGap{};float referenceLeftDepth{},referenceRightDepth{};const unsigned middle=(minimumX+maximumX)/2;(void)evaluateFoot(minimumX,middle,referenceLeftGap,referenceLeftDepth);(void)evaluateFoot(std::min(maximumX,middle+1),maximumX,referenceRightGap,referenceRightDepth);
    const auto referenceContact=buildGpuContact(captureReferenceFootPlacementHash_[index]);
    if(!referenceContact.Complete(capture->width,capture->height)||!EncodeFrameCapturePng3D(captureDirectory_/(stem+L"-reference-contact.png"),capture->width,capture->height,std::size_t(capture->width)*4,referenceContact.rgba))return failReadback("reference-gpu-contact-artifact-failed");
    captureCharacterReferenceCoverage_[index]=float(referencePixels)/float(capture->pixels.size());captureCharacterEffectiveCoverage_[index]=float(effectivePixels)/float(capture->pixels.size());captureCharacterVisibility_[index]=float(intersectionPixels)/float(referencePixels);captureCharacterSilhouetteIou_[index]=float(intersectionPixels)/float(unionPixels);captureCharacterReferenceSupported_[index]=1;captureReferenceAcceptedExcludingOpacity_[index]=capture->acceptedDrawFingerprintExcludingOpacity;captureReferenceAcceptedIncludingOpacity_[index]=capture->acceptedDrawFingerprintIncludingOpacity;captureReferenceIdentityOrderHash_[index]=capture->acceptedIdentityOrderHash;captureReferenceDepthSha256_[index]=depth->pixelSha256;captureReferenceDepthFinalFileSha256_[index]=depthPublication.finalFileSha256;captureReferenceDepthFinalFileBytes_[index]=depthPublication.finalFileBytes;captureDepthGeneration_[index]=std::uint32_t(depth->generation);captureDepthSupported_[index]=1;captureEffectiveCharacterMask_.clear();
    // reference只移除建築；若effective沒有任何建築像素，輪廓差異來自跨幀光柵而非遮擋。
  }else if(raw){
    captureRawRasterProvenance_[index]=capture->rasterProvenance;
    captureTemporalHistoryGeneration_[index]=capture->rasterProvenance.temporalHistoryReadGeneration;
    captureRawFemalePoseHash_[index]=femalePoseHash;captureRawMalePoseHash_[index]=malePoseHash;captureRawMonsterPoseHash_[index]=monsterPoseHash;captureRawFootPlacementHash_[index]=captureFrozenFootPlacementHash_;
    captureRawDepthSha256_[index]=depth->pixelSha256;captureRawDepthFinalFileSha256_[index]=depthPublication.finalFileSha256;captureRawDepthFinalFileBytes_[index]=depthPublication.finalFileBytes;captureRawBuildingCoverage_[index]=buildingCoverage;captureRawBuildingUnion_[index]=buildingUnion;captureRawMaximumVisibleCoverage_[index]=maximumVisibleCoverage;captureRawMaximumVisibleStableHigh_[index]=maximumVisibleIdentity.id.high;captureRawMaximumVisibleStableLow_[index]=maximumVisibleIdentity.id.low;captureRawMaximumVisibleGeneration_[index]=maximumVisibleIdentity.generation;
    if(dominantToken){const auto& identity=capture->tokenMap[dominantToken-1];captureOccluderStableHigh_[index]=identity.id.high;captureOccluderStableLow_[index]=identity.id.low;captureOccluderGeneration_[index]=identity.generation;captureRawOccluderCoverage_[index]=float(counts[dominantToken])/float(capture->pixels.size());}captureRawObjectIdMaskHash_[index]=hash;captureRawAcceptedExcludingOpacity_[index]=capture->acceptedDrawFingerprintExcludingOpacity;captureRawAcceptedIncludingOpacity_[index]=capture->acceptedDrawFingerprintIncludingOpacity;captureRawIdentityOrderHash_[index]=capture->acceptedIdentityOrderHash;captureRequestDrawRevision_[index]=capture->requestMetadata.drawListRevision;captureRequestCameraHash_[index]=capture->requestMetadata.cameraHash;captureRequestStateHash_[index]=capture->requestMetadata.stateHash;
  }else{
    const auto& expectation=GameplayCaptureExpectations3D[index];
    std::uint32_t requiredMatched{},forbiddenMatched{},generationConflicts{};
    captureSemanticFrameSerial_[index]=capture->rasterProvenance.frameSerial;
    const bool semanticExpectation=EvaluateGameplayCaptureSemantic3D(expectation,*capture,GameplayCaptureCamera3D(captureObservedCamera_[index]),GameplayCaptureLighting3D(captureObservedLighting_[index]),requiredMatched,forbiddenMatched,generationConflicts);
    captureSemanticRequiredMatched_[index]=requiredMatched;captureSemanticForbiddenMatched_[index]=forbiddenMatched;captureSemanticGenerationConflicts_[index]=generationConflicts;
    for(std::size_t required=0;required<expectation.requiredIdentityCount;++required)if(const auto* semantic=capture->Semantic(expectation.requiredIdentities[required])){captureSemanticRequiredPixels_[index][required]=semantic->nonzeroPixelCount;captureSemanticRequiredCoverage_[index][required]=semantic->coverage;}
    captureSemanticExpectationPassed_[index]=semanticExpectation?1u:0u;
    if(!semanticExpectation){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=typed-semantic-expectation required="<<requiredMatched<<":"<<expectation.requiredIdentityCount<<" forbidden="<<forbiddenMatched<<" generationConflicts="<<generationConflicts<<" camera="<<unsigned(captureObservedCamera_[index])<<":"<<unsigned(expectation.camera)<<" lighting="<<unsigned(captureObservedLighting_[index])<<":"<<unsigned(expectation.lighting);WriteCaptureIntegrityDiagnostic(true);return false;}
    captureCommittedEffectiveNames_[index]=captureRequestedEffectiveNames_[index];captureCommittedEffectivePaths_[index]=captureRequestedEffectivePaths_[index];captureSceneSubmissionSerial_[index]=phaseSceneStats->submissionSerial;captureSceneDrawRevision_[index]=phaseSceneStats->drawListRevision;captureSceneIdentityOrderFingerprint_[index]=phaseSceneStats->acceptedIdentityOrderFingerprint;
    captureLodInstances_[index]=std::uint32_t(phaseSceneStats->lodMeshInstances);captureLodTransitions_[index]=std::uint32_t(phaseSceneStats->lodTransitionInstances);captureLodDualSubmissions_[index]=std::uint32_t(phaseSceneStats->lodDualSubmittedInstances);captureLodBudgetFallbacks_[index]=std::uint32_t(phaseSceneStats->lodTransitionBudgetFallbacks);
    captureShadowProvenance_[index]=capture->directionalShadow;captureShadowCasters_[index]=std::uint32_t(capture->directionalShadow.casters);captureShadowDrawCalls_[index]=std::uint32_t(capture->directionalShadow.drawCalls);captureShadowCascadeDrawCalls_[index]=std::uint32_t(capture->directionalShadow.cascadeDrawCalls);
    captureShadowCascadeRendered_[index]=std::uint32_t(std::accumulate(capture->directionalShadow.rendered.begin(),capture->directionalShadow.rendered.end(),std::uint64_t{}));captureShadowCascadeReused_[index]=std::uint32_t(std::accumulate(capture->directionalShadow.reused.begin(),capture->directionalShadow.reused.end(),std::uint64_t{}));captureShadowActiveCascades_[index]=capture->directionalShadow.activeCascades;captureShadowFrameSerial_[index]=capture->directionalShadow.frameSerial;
    captureShadowEvidencePassed_[index]=EvaluateGameplayCaptureShadowEvidence3D(capture->directionalShadow,capture->rasterProvenance)?1u:0u;
#if AEVRYND_SHADOW_MAP_DIAGNOSTICS
    if(captureShadowMapDiagnostic_&&(index==0||index==8||index==9||index==10)){
      const auto shadowStem=std::filesystem::path(Names[index]).stem().wstring();const auto receiptPath=captureDirectory_/(shadowStem+L"-shadow-map-evidence.json");const auto receiptTemporary=receiptPath.wstring()+L".tmp";
      std::ofstream shadowReceipt(std::filesystem::path(receiptTemporary),std::ios::binary|std::ios::trunc);if(!shadowReceipt)return failReadback("shadow-map-receipt-open-failed");
      shadowReceipt<<"{\"source\":\"GameplayDirectionalShadowMapEvidence\",\"version\":1,\"runId\":\""<<runId_<<"\",\"capture\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"frameSerial\":"<<capture->directionalShadow.frameSerial<<",\"activeCascades\":"<<capture->directionalShadow.activeCascades<<",\"cascades\":[";
      for(unsigned cascade=0;cascade<capture->directionalShadow.activeCascades;++cascade){
        const auto shadow=EngineShadowDiagnosticAccess3D::CaptureDirectionalShadow(engine,cascade);if(!shadow||!shadow->Complete())return failReadback("shadow-map-capture-incomplete cascade="+std::to_string(cascade));
        const auto destination=captureDirectory_/(shadowStem+L"-shadow-cascade-"+std::to_wstring(cascade)+L".f32");const auto temporary=destination.wstring()+L".tmp";
        std::ofstream pixels(std::filesystem::path(temporary),std::ios::binary|std::ios::trunc);pixels.write(reinterpret_cast<const char*>(shadow->pixels.data()),std::streamsize(shadow->pixels.size()*sizeof(float)));pixels.flush();if(!pixels){pixels.close();std::filesystem::remove(temporary);return failReadback("shadow-map-pixel-publication-failed cascade="+std::to_string(cascade));}pixels.close();
        if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary);return failReadback("shadow-map-pixel-commit-failed cascade="+std::to_string(cascade));}
        std::array<std::byte,32> shadowHash{};if(!CaptureFileSha256(destination,shadowHash))return failReadback("shadow-map-pixel-hash-failed cascade="+std::to_string(cascade));if(cascade)shadowReceipt<<',';
        shadowReceipt<<"{\"cascade\":"<<cascade<<",\"width\":"<<shadow->width<<",\"height\":"<<shadow->height<<",\"rowPitch\":"<<shadow->rowPitch<<",\"file\":\""<<destination.filename().string()<<"\",\"bytes\":"<<shadow->pixels.size()*sizeof(float)<<",\"sha256\":\""<<CaptureShaHex(shadowHash)<<"\"}";
      }
      shadowReceipt<<"]}";shadowReceipt.flush();if(!shadowReceipt){shadowReceipt.close();std::filesystem::remove(receiptTemporary);return failReadback("shadow-map-receipt-publication-failed");}shadowReceipt.close();
      if(!MoveFileExW(receiptTemporary.c_str(),receiptPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(receiptTemporary);return failReadback("shadow-map-receipt-commit-failed");}
    }
#endif
    captureLodFrameSerial_[index]=capture->rasterProvenance.frameSerial;
    const bool predictedLodExact=expectation.camera!=GameplayCaptureCamera3D::God||(captureLodContracts_[index].Exact()&&captureGodLodTransitions_[index]>0&&captureLodTransitions_[index]==captureGodLodTransitions_[index]&&captureLodDualSubmissions_[index]==captureGodLodTransitions_[index]);
    captureLodPredictionMatched_[index]=predictedLodExact?1u:0u;
    captureLodEvidencePassed_[index]=EvaluateGameplayCaptureLodEvidence3D(expectation,capture->rasterProvenance.frameSerial,capture->rasterProvenance.frameSerial,captureLodInstances_[index],captureLodTransitions_[index],captureLodDualSubmissions_[index],captureLodBudgetFallbacks_[index])&&predictedLodExact?1u:0u;
    const auto& rawProvenance=captureRawRasterProvenance_[index];
    if(capture->rasterProvenance.frameSerial<=rawProvenance.frameSerial||capture->rasterProvenance.tokenGeneration<=rawProvenance.tokenGeneration||capture->rasterProvenance.viewProjectionHash!=rawProvenance.viewProjectionHash||capture->rasterProvenance.temporalHistoryReadGeneration!=rawProvenance.temporalHistoryReadGeneration||capture->rasterProvenance.temporalHistoryReadIndex!=rawProvenance.temporalHistoryReadIndex||capture->rasterProvenance.jitterIndex!=rawProvenance.jitterIndex||capture->rasterProvenance.jitterPixels.x!=rawProvenance.jitterPixels.x||capture->rasterProvenance.jitterPixels.y!=rawProvenance.jitterPixels.y){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=effective-raster-provenance-mismatch rawFrame="<<rawProvenance.frameSerial<<" effectiveFrame="<<capture->rasterProvenance.frameSerial<<" rawVp="<<rawProvenance.viewProjectionHash<<" effectiveVp="<<capture->rasterProvenance.viewProjectionHash<<" rawJitter="<<rawProvenance.jitterIndex<<" effectiveJitter="<<capture->rasterProvenance.jitterIndex;return false;}
    captureEffectiveRasterProvenance_[index]=capture->rasterProvenance;
    captureEffectiveFemalePoseHash_[index]=femalePoseHash;captureEffectiveMalePoseHash_[index]=malePoseHash;captureEffectiveMonsterPoseHash_[index]=monsterPoseHash;captureEffectiveFootPlacementHash_[index]=captureFrozenFootPlacementHash_;
    if(capture->requestMetadata!=capturePairMetadata_||captureRawAcceptedExcludingOpacity_[index]!=capture->acceptedDrawFingerprintExcludingOpacity||captureRawIdentityOrderHash_[index]!=capture->acceptedIdentityOrderHash){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=effective-parity metadata="<<(capture->requestMetadata==capturePairMetadata_)<<" excluding="<<captureRawAcceptedExcludingOpacity_[index]<<":"<<capture->acceptedDrawFingerprintExcludingOpacity<<" order="<<captureRawIdentityOrderHash_[index]<<":"<<capture->acceptedIdentityOrderHash;return false;}
    captureEffectiveDepthSha256_[index]=depth->pixelSha256;captureEffectiveDepthFinalFileSha256_[index]=depthPublication.finalFileSha256;captureEffectiveDepthFinalFileBytes_[index]=depthPublication.finalFileBytes;captureDepthGeneration_[index]=std::uint32_t(depth->generation);captureDepthSupported_[index]=1;captureEffectiveBuildingCoverage_[index]=buildingCoverage;captureEffectiveBuildingUnion_[index]=buildingUnion;captureEffectiveMaximumVisibleCoverage_[index]=maximumVisibleCoverage;captureEffectiveMaximumVisibleStableHigh_[index]=maximumVisibleIdentity.id.high;captureEffectiveMaximumVisibleStableLow_[index]=maximumVisibleIdentity.id.low;captureEffectiveMaximumVisibleGeneration_[index]=maximumVisibleIdentity.generation;
    captureEffectiveCharacterMask_.assign(capture->pixels.size(),0);if(playerToken)for(std::size_t pixel=0;pixel<capture->pixels.size();++pixel)captureEffectiveCharacterMask_[pixel]=isPlayerPixel(capture->pixels[pixel])?1u:0u;
    if(expectation.camera==GameplayCaptureCamera3D::First){
      const auto measure=[&](std::uint32_t token,std::uint32_t& pixelCount,std::uint32_t& minimumX,std::uint32_t& minimumY,std::uint32_t& maximumX,std::uint32_t& maximumY){minimumX=capture->width;minimumY=capture->height;maximumX=maximumY=pixelCount=0;for(unsigned y=0;y<capture->height;++y)for(unsigned x=0;x<capture->width;++x)if(capture->pixels[std::size_t(y)*capture->width+x]==token){++pixelCount;minimumX=std::min(minimumX,x);minimumY=std::min(minimumY,y);maximumX=std::max(maximumX,x);maximumY=std::max(maximumY,y);}return pixelCount!=0;};
      captureFirstPersonArmsToken_[index]=firstPersonArmsToken;captureFirstPersonWeaponToken_[index]=weaponToken;
      const bool armsMeasured=measure(firstPersonArmsToken,captureFirstPersonArmsPixels_[index],captureFirstPersonArmsMinimumX_[index],captureFirstPersonArmsMinimumY_[index],captureFirstPersonArmsMaximumX_[index],captureFirstPersonArmsMaximumY_[index]);
      const bool weaponMeasured=measure(weaponToken,captureFirstPersonWeaponPixels_[index],captureFirstPersonWeaponMinimumX_[index],captureFirstPersonWeaponMinimumY_[index],captureFirstPersonWeaponMaximumX_[index],captureFirstPersonWeaponMaximumY_[index]);
      // 肩部切口必須留在畫面外，因此手臂可從右緣進入；武器仍要求四邊完整安全距離。
      const bool armsComplete=armsMeasured&&captureFirstPersonArmsMinimumX_[index]>=capture->width/2&&captureFirstPersonArmsMinimumY_[index]>=66;
      const bool weaponComplete=weaponMeasured&&captureFirstPersonWeaponMinimumX_[index]>=16&&captureFirstPersonWeaponMinimumY_[index]>=32&&captureFirstPersonWeaponMaximumX_[index]+16<capture->width&&captureFirstPersonWeaponMaximumY_[index]+16<capture->height;
      if(!armsComplete||!weaponComplete){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=first-person-child-bounds arms="<<firstPersonArmsToken<<":"<<captureFirstPersonArmsPixels_[index]<<":"<<captureFirstPersonArmsMinimumX_[index]<<","<<captureFirstPersonArmsMinimumY_[index]<<","<<captureFirstPersonArmsMaximumX_[index]<<","<<captureFirstPersonArmsMaximumY_[index]<<" weapon="<<weaponToken<<":"<<captureFirstPersonWeaponPixels_[index]<<":"<<captureFirstPersonWeaponMinimumX_[index]<<","<<captureFirstPersonWeaponMinimumY_[index]<<","<<captureFirstPersonWeaponMaximumX_[index]<<","<<captureFirstPersonWeaponMaximumY_[index];return false;}
    }
    if(playerToken&&expectation.camera==GameplayCaptureCamera3D::Third){
      const auto separation=EvaluateGameplayActorScreenSeparation3D(capture->width,capture->height,capture->pixels,playerToken,companionToken);
      capturePlayerPixels_[index]=separation.playerPixels;captureCompanionPixels_[index]=separation.companionPixels;captureActorGapPixels_[index]=separation.gapPixels;captureActorSeparationPassed_[index]=separation.complete&&separation.safe?1u:0u;
      if(!captureActorSeparationPassed_[index]){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason=actor-silhouette-separation playerPixels="<<separation.playerPixels<<" companionPixels="<<separation.companionPixels<<" gap="<<separation.gapPixels<<" required="<<std::uint32_t(std::ceil(float(capture->width)*.05f));return false;}
      unsigned minimumX=capture->width,maximumX{},minimumY=capture->height,maximumY{};for(unsigned y=0;y<capture->height;++y)for(unsigned x=0;x<capture->width;++x)if(isPlayerPixel(capture->pixels[std::size_t(y)*capture->width+x])){minimumX=std::min(minimumX,x);maximumX=std::max(maximumX,x);minimumY=std::min(minimumY,y);maximumY=std::max(maximumY,y);}if(minimumX>maximumX||minimumY>maximumY)return failReadback("player-screen-bounds-empty");
      const auto contact=buildGpuContact(captureEffectiveFootPlacementHash_[index]);
      captureGpuFeetContactPassed_[index]=contact.Complete(capture->width,capture->height)?1u:0u;
      if(!captureGpuFeetContactPassed_[index])return failReadback("gpu-feet-contact bottom-band-split=0");
      captureLeftFootGroundGapPixels_[index]=contact.leftGapPixels;captureRightFootGroundGapPixels_[index]=contact.rightGapPixels;captureLeftFootDepthDelta_[index]=contact.leftDepthDelta;captureRightFootDepthDelta_[index]=contact.rightDepthDelta;
      captureContactLeftPixels_[index]=contact.leftFootPixels;captureContactRightPixels_[index]=contact.rightFootPixels;captureContactGroundSupportPixels_[index]=contact.groundSupportPixels;captureContactShadowPixels_[index]=contact.contactShadowPixels;captureContactFrameSerial_[index]=contact.frameSerial;captureContactPoseHash_[index]=contact.poseHash;
      const auto contactPath=captureDirectory_/(stem+L"-contact.png");if(!EncodeFrameCapturePng3D(contactPath,capture->width,capture->height,std::size_t(capture->width)*4,contact.rgba)||!CaptureFileSha256(contactPath,captureContactArtifactSha256_[index]))return failReadback("gpu-contact-artifact-publication-failed");
    }
    captureEffectiveAcceptedExcludingOpacity_[index]=capture->acceptedDrawFingerprintExcludingOpacity;captureEffectiveAcceptedIncludingOpacity_[index]=capture->acceptedDrawFingerprintIncludingOpacity;captureEffectiveIdentityOrderHash_[index]=capture->acceptedIdentityOrderHash;captureOccluderCoverage_[index]=dominantToken?float(counts[dominantToken])/float(capture->pixels.size()):0.f;captureEffectiveObjectIdSupported_[index]=1;captureObjectIdGeneration_[index]=capture->generation;captureObjectIdMaskHash_[index]=hash;captureObjectIdSubmitted_[index]=std::uint32_t(capture->submittedOpaqueStatic);captureObjectIdOpaqueComplete_[index]=capture->opaqueStaticComplete?1u:0u;captureObjectIdMaskedComplete_[index]=capture->maskedStaticComplete?1u:0u;captureObjectIdSkinnedComplete_[index]=capture->skinnedComplete?1u:0u;
    const bool applied=appliedOcclusionIdentity_==RenderObjectIdentity3D{{captureOccluderStableHigh_[index],captureOccluderStableLow_[index]},captureOccluderGeneration_[index]}&&appliedOcclusionOpacity_<1.f&&appliedOcclusionUpdate_&&!appliedOcclusionUpdate_.generationConflicts&&!appliedOcclusionUpdate_.missing;
    if(applied&&captureRawAcceptedIncludingOpacity_[index]==capture->acceptedDrawFingerprintIncludingOpacity)return failReadback("occlusion-opacity-fingerprint-unchanged");
    if(applied){captureFadedStableHigh_[index]=appliedOcclusionIdentity_.id.high;captureFadedStableLow_[index]=appliedOcclusionIdentity_.id.low;captureFadedGeneration_[index]=appliedOcclusionIdentity_.generation;captureOpacityMatchedStatic_[index]=std::uint32_t(appliedOcclusionUpdate_.matchedStatic);captureOpacityMatchedMultipart_[index]=std::uint32_t(appliedOcclusionUpdate_.matchedMultipart);}
    captureOpacityMissing_[index]=appliedOcclusionUpdate_.missing?1u:0u;captureOpacityGenerationConflict_[index]=appliedOcclusionUpdate_.generationConflicts?1u:0u;captureOcclusionAdjusted_[index]=applied&&captureRawOccluderCoverage_[index]>.08f&&captureOccluderCoverage_[index]<.08f?1u:0u;
  }
  if(effective){
    const auto failHdr=[&](const char* reason){std::ofstream(captureDirectory_/"gameplay-object-id-failure.txt",std::ios::trunc)<<"index="<<index<<" reason="<<reason<<" state="<<GameplayHdrStateName3D(gameplayHdrCaptureStates_[index]);return false;};
    if(gameplayHdrCaptureStates_[index]!=GameplayHdrCaptureState3D::Taken)return failHdr("hdr-state-not-taken");
    if(!productionBundle)return failHdr("production-bundle-missing");Aevrynd::GameplayLab::GameplayProductionHdrSidecarValidation3D validation{};auto sealed=Aevrynd::GameplayLab::BuildGameplayProductionHdrSidecar3D(*productionBundle,&validation);if(!sealed||!validation){std::ofstream(captureDirectory_/"gameplay-hdr-sidecar-validation-failure.txt",std::ios::trunc)<<"index="<<index<<" error="<<std::uint32_t(validation.error)<<" field="<<(validation.field?validation.field:"");return failHdr("hdr-sidecar-seal-invalid");}
    const auto destination=GameplayHdrCapturePath(index);if(destination.empty())return failHdr("hdr-destination-empty");if(std::filesystem::exists(destination))return failHdr("hdr-destination-already-exists");
    Aevrynd::GameplayLab::GameplayProductionHdrSidecarPublishReceipt3D published{};if(!Aevrynd::GameplayLab::PublishGameplayProductionHdrSidecar3D(*sealed,destination,&published))return failHdr("hdr-sidecar-publish-failed");
    gameplayHdrCaptureStates_[index]=GameplayHdrCaptureState3D::Published;
    std::error_code fileError;const auto fileBytes=std::filesystem::file_size(destination,fileError);const auto nonzero=[](const auto& hash){return std::ranges::any_of(hash,[](std::uint8_t value){return value!=0;});};const auto inspected=Aevrynd::GameplayLab::InspectGameplayProductionHdrSidecarFile3D(destination);
    if(fileError||!inspected||inspected->schemaVersion!=3||!published.byteCount||fileBytes!=published.byteCount||inspected->byteCount!=published.byteCount||inspected->fileHash!=published.fileHash||inspected->stableContentHash!=published.stableContentHash||inspected->payloadHash!=published.payloadHash||inspected->rootHash!=published.rootHash||published.stableContentHash!=sealed->Sidecar().canonical.stableContentHash||published.payloadHash!=sealed->Sidecar().canonical.payloadHash||published.rootHash!=sealed->Sidecar().canonical.rootHash||!nonzero(published.fileHash)||!nonzero(published.stableContentHash)||!nonzero(published.payloadHash)||!nonzero(published.rootHash)||!Aevrynd::GameplayLab::ValidateGameplayProductionHdrSidecar3D(sealed->Sidecar()))return failHdr("hdr-sidecar-post-publish-validation-failed");
    auto& receipt=gameplayHdrCaptureReceipts_[index];receipt={GameplayHdrCaptureState3D::Published,destination,inspected->fileHash,inspected->stableContentHash,inspected->payloadHash,inspected->rootHash,inspected->byteCount,hdr->receipt.rasterProvenance.frameSerial,hdr->receipt.requestMetadata.sequence.token.value,hdr->receipt.requestMetadata.sequence.index,false};
    sealed.reset();productionBundle.reset();receipt.state=GameplayHdrCaptureState3D::Verified;receipt.verified=true;gameplayHdrCaptureStates_[index]=GameplayHdrCaptureState3D::Verified;
  }
  if(rawHdrDiagnostic){
    if(!productionBundle||!hdr||hdr->receipt.requestMetadata.sequence.index!=0)return failReadback("raw-hdr-diagnostic-bundle-missing");
    Aevrynd::GameplayLab::GameplayProductionHdrSidecarValidation3D validation{};auto sealed=Aevrynd::GameplayLab::BuildGameplayProductionHdrSidecar3D(*productionBundle,&validation);if(!sealed||!validation)return failReadback("raw-hdr-diagnostic-seal-invalid");
    const auto destination=captureDirectory_/(std::filesystem::path(Names[index]).stem().wstring()+L"-raw.production-hdr.aevhdr");if(std::filesystem::exists(destination))return failReadback("raw-hdr-diagnostic-destination-exists");
    Aevrynd::GameplayLab::GameplayProductionHdrSidecarPublishReceipt3D published{};if(!Aevrynd::GameplayLab::PublishGameplayProductionHdrSidecar3D(*sealed,destination,&published)||!published.byteCount)return failReadback("raw-hdr-diagnostic-publish-failed");
  }
  return true;
}
void GameplayLabGame::UpdateCapture(Engine &engine,bool fixedSimulationStep) {
  if (captureDirectory_.empty())
    return;
  // 固定更新可能在下一次 OnRender 前執行多次；已排隊的階段必須等待同幀
  // Scene submission 綁定，禁止誤啟另一個讀回序列。
  if(capturePhaseRequestPending_||capturePresentationRequestPending_)
    return;
  if(capturePresentationWarmupActive_){
    if(!capturePairFrozen_||captureSequenceToken_||!capturePresentationLogicalToken_||captureIndex_>=Names.size())throw std::runtime_error("Gameplay presentation warm-up was not paired with a completed diagnostic sequence");
    if(!GameplayPresentationCaptureRequestDue3D(true,capturePresentationWarmupFramesCurrent_,captureDiagnosticAsyncWaitFrames_))return;
    capturePresentationWarmupActive_=false;capturePresentationRequestPending_=true;
    return;
  }
  const auto renderer = engine.GetRendererStats();
  if(capturePresentationPending_&&(renderer.successfulFrameCaptures>captureSuccess_||renderer.rejectedFrameCaptures>captureReject_)){
    if(renderer.rejectedFrameCaptures>captureReject_)throw std::runtime_error("Gameplay production presentation capture was rejected");
    const auto presentationPath=(captureDirectory_/Names[captureIndex_]).lexically_normal();
    const auto diagnosticPath=captureDirectory_/(std::filesystem::path(Names[captureIndex_]).stem().wstring()+L"-diagnostic.png");
    std::array<std::byte,32> presentationHash{},diagnosticHash{};
    const bool filesDistinct=CaptureFileSha256(presentationPath,presentationHash)&&CaptureFileSha256(diagnosticPath,diagnosticHash)&&presentationHash!=diagnosticHash;
    const auto frameSerial=renderer.lastDirectionalShadowProvenance.frameSerial;
    const bool aaParity=renderer.multisampleEnabled&&renderer.multisampleCount>1&&renderer.temporalAntiAliasingEnabled&&renderer.temporalHistoryUsed;
    const bool logicalPair=capturePresentationLogicalToken_&&capturePairMetadata_.cameraHash==captureRequestCameraHash_[captureIndex_]&&capturePairMetadata_.stateHash==captureRequestStateHash_[captureIndex_]&&capturePairMetadata_.sequenceInvariantHash&&frameSerial&&frameSerial!=captureEffectiveRasterProvenance_[captureIndex_].frameSerial;
    if(!filesDistinct||!aaParity||!logicalPair)throw std::runtime_error(
      "Gameplay presentation capture did not use the independent production AA/TAA raster: capture="+
      std::to_string(captureIndex_)+" filesDistinct="+std::to_string(filesDistinct)+
      " aaParity="+std::to_string(aaParity)+" logicalPair="+std::to_string(logicalPair)+
      " msaa="+std::to_string(renderer.multisampleEnabled)+" samples="+std::to_string(renderer.multisampleCount)+
      " taa="+std::to_string(renderer.temporalAntiAliasingEnabled)+" history="+std::to_string(renderer.temporalHistoryUsed)+
      " frameSerial="+std::to_string(frameSerial)+" effectiveFrameSerial="+
      std::to_string(captureEffectiveRasterProvenance_[captureIndex_].frameSerial));
    capturePresentationFrameSerial_[captureIndex_]=frameSerial;capturePresentationCameraHash_[captureIndex_]=capturePairMetadata_.cameraHash;capturePresentationStateHash_[captureIndex_]=capturePairMetadata_.stateHash;capturePresentationInvariantHash_[captureIndex_]=capturePairMetadata_.sequenceInvariantHash;capturePresentationMsaa_[captureIndex_]=renderer.multisampleEnabled;capturePresentationTaa_[captureIndex_]=renderer.temporalAntiAliasingEnabled;capturePresentationHistoryUsed_[captureIndex_]=renderer.temporalHistoryUsed;capturePresentationPaired_[captureIndex_]=1;
    if(!captureScenarioClock_.CompleteReadback(captureIndex_,temporalFixtureWarmupFrames_?2:Names.size()))throw std::runtime_error("Gameplay presentation completion was out of order");
    capturePresentationPending_=false;capturePresentationLogicalToken_={};captureScenePhaseLedger_.Clear();captureReferencePhase_=false;
    if(captureIndex_==9){
      if(rules_.PaletteOpen()){TickInput paletteToggle{};paletteToggle.togglePalette=true;if(!rules_.ApplyPalettePresentationInput(paletteToggle))throw std::runtime_error("Gameplay capture palette close input failed");}
      if(!captureEvidenceCollider_||!collisions_.Destroy(captureEvidenceCollider_))
        throw std::runtime_error("Gameplay capture palette evidence collider cleanup failed");
      captureEvidenceCollider_={};captureValidGhost_.reset();captureInvalidGhost_.reset();
      if(captureEvidenceCollider_)
        throw std::runtime_error("Gameplay capture palette evidence collider remained live");
      captureEvidenceColliderReleased_=true;
    }
    activeCaptureRoute_.Clear();++captureIndex_;capturePairFrozen_=false;captureTemporalWarmupActive_=false;capturePoseFrozen_=false;capturePreparedIndex_.reset();
    if(captureIndex_==4||captureIndex_==5)captureCrouchTapPending_=true;
    return;
  }
  if (capturePending_ && (renderer.successfulFrameCaptures > captureSuccess_ ||
                          renderer.rejectedFrameCaptures > captureReject_)) {
    if (renderer.rejectedFrameCaptures > captureReject_){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay paired framebuffer capture was rejected");}
    if (renderer.successfulFrameCaptures > captureSuccess_) {
      if(!ConsumeObjectIdCapture(engine,captureIndex_,captureRawPhase_,captureReferencePhase_)){const auto reason=captureReadbackFailure_;FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay paired GPU object-ID/depth/HDR readback failed closed: index="+std::to_string(captureIndex_)+" raw="+std::to_string(captureRawPhase_)+" reference="+std::to_string(captureReferencePhase_)+" reason="+reason);}
      if(captureRawPhase_){
        captureRawPhase_=false;capturePending_=false;
        capturePairMetadata_.sequence.index=1;capturePairMetadata_.drawListRevision=0;
        capturePhaseRequestPending_=true;
        return;
      }
      if(gameplayHdrCaptureStates_[captureIndex_]!=GameplayHdrCaptureState3D::Verified){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay effective HDR sidecar was not verified before phase progression");}
      const bool thirdCapture=(captureIndex_<9&&captureIndex_%3==1)||captureIndex_>=9;
      if(!captureReferencePhase_&&thirdCapture){
        captureReferencePhase_=true;capturePending_=false;
        capturePairMetadata_.sequence.index=2;capturePairMetadata_.drawListRevision=0;
        capturePhaseRequestPending_=true;
        return;
      }
      const auto gpuProvenance=engine.ReadbackCaptureSequenceGpuProvenance(captureSequenceToken_);if(gpuProvenance.token!=captureSequenceToken_||gpuProvenance.phases.size()!=captureSequencePhaseCount_){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay GPU sequence provenance was incomplete");}for(const auto& phase:gpuProvenance.phases){if(phase.phaseIndex>=3){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay GPU sequence provenance phase was invalid");}captureAtmospherePhaseProvenance_[captureIndex_][phase.phaseIndex]=phase.atmosphere;captureTemporalPhaseProvenance_[captureIndex_][phase.phaseIndex]=phase.temporalResolve;}
      const auto sequenceResult=engine.EndReadbackCaptureSequence3D(captureSequenceToken_);
      // 一個已封存鏡位必須先退役所有 GPU 階段，下一鏡位才能重用大氣與歷史資源。
      // 診斷讀回原本已提供此邊界；現在將它納入正式擷取契約，避免結果取決於是否編入診斷功能。
      engine.WaitRendererIdle3D();
      const std::uint64_t expectedHdrMask=captureRawHdrDiagnostic_?((1ull<<0)|(1ull<<1)):(1ull<<1);
      if(!sequenceResult||sequenceResult.receipt.expectedPhaseCount!=captureSequencePhaseCount_||sequenceResult.receipt.descriptor.expectedPhaseCount!=captureSequencePhaseCount_||sequenceResult.receipt.descriptor.jitterPolicy!=ReadbackCaptureSequenceJitterPolicy3D::FixedSample||sequenceResult.receipt.descriptor.fixedSampleIndex!=0||sequenceResult.receipt.descriptor.requiredLinearHdrPhaseMask!=expectedHdrMask){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay paired fixed-sample HDR readback capture sequence failed closed");}
      for(const auto& phase:sequenceResult.receipt.phases){if(phase.phaseIndex>=captureIntegrityDiagnostics_[captureIndex_].size()){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay frame-bound determinism phase index was invalid");}auto& diagnostic=captureIntegrityDiagnostics_[captureIndex_][phase.phaseIndex];if(!diagnostic.recorded||diagnostic.objectId!=phase.determinism||diagnostic.depth!=phase.determinism){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay frame-bound determinism phase evidence drifted");}diagnostic={captureIndex_,phase.phaseIndex,phase.determinism,phase.determinism,true};}
      const auto& effectiveDeterminism=sequenceResult.receipt.phases[1].determinism;captureDeterminismFrameSerial_[captureIndex_]=effectiveDeterminism.frameSerial;captureDeterminismIntegrity_[captureIndex_]=effectiveDeterminism.integrity;captureCanonicalDrawFingerprint_[captureIndex_]=effectiveDeterminism.canonical;if(!WriteCaptureIntegrityDiagnostic()){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay frame-bound determinism diagnostic publication failed");}
      captureSequenceReceipts_[captureIndex_]=sequenceResult.receipt;
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
      if(captureAtmosphereOutputDiagnostic_){
        const auto atmosphere=EngineAtmosphereDiagnosticAccess3D::CaptureVolumetricAtmosphereOutput(engine);if(!atmosphere||!atmosphere->Complete()){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay atmosphere output diagnostic readback failed closed");}
        const auto stem=std::filesystem::path(Names[captureIndex_]).stem().string();const auto payloadPath=captureDirectory_/(stem+"-atmosphere-rgba16f.bin");std::ofstream payload(payloadPath,std::ios::binary|std::ios::trunc);payload.write(reinterpret_cast<const char*>(atmosphere->rgba16f.data()),std::streamsize(atmosphere->rgba16f.size()));payload.flush();if(!payload){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay atmosphere output diagnostic payload publication failed");}payload.close();
        const auto receiptPath=captureDirectory_/(stem+"-atmosphere-output.json");std::ofstream receipt(receiptPath,std::ios::binary|std::ios::trunc);receipt<<"{\"schemaVersion\":1,\"source\":\"GameplayLabAtmosphereOutputDiagnostic\",\"runId\":\""<<runId_<<"\",\"name\":\""<<std::filesystem::path(Names[captureIndex_]).string()<<"\",\"sequenceToken\":"<<captureSequenceToken_.value<<",\"semanticSourcePhase\":0,\"capturedAfterPhase\":"<<(captureSequencePhaseCount_-1)<<",\"width\":"<<atmosphere->identity.width<<",\"height\":"<<atmosphere->identity.height<<",\"format\":\"R16G16B16A16_FLOAT\",\"sourceRowPitch\":"<<atmosphere->sourceRowPitch<<",\"rowBytes\":"<<atmosphere->identity.width*8<<",\"payloadBytes\":"<<atmosphere->rgba16f.size()<<",\"payloadPath\":\""<<payloadPath.filename().string()<<"\",\"payloadSha256\":\""<<CaptureShaHex(atmosphere->pixelSha256)<<"\",\"resourceIdentity\":"<<atmosphere->identity<<"}";receipt.flush();if(!receipt){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay atmosphere output diagnostic receipt publication failed");}
        const auto scene=EngineAtmosphereDiagnosticAccess3D::CaptureAtmosphereSceneSnapshot(engine);if(!scene||!scene->Complete()){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay atmosphere scene snapshot diagnostic readback failed closed");}
        const auto scenePayloadPath=captureDirectory_/(stem+"-atmosphere-scene-rgba16f.bin");std::ofstream scenePayload(scenePayloadPath,std::ios::binary|std::ios::trunc);scenePayload.write(reinterpret_cast<const char*>(scene->rgba16f.data()),std::streamsize(scene->rgba16f.size()));scenePayload.flush();if(!scenePayload){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay atmosphere scene snapshot diagnostic payload publication failed");}scenePayload.close();
        const auto sceneReceiptPath=captureDirectory_/(stem+"-atmosphere-scene.json");std::ofstream sceneReceipt(sceneReceiptPath,std::ios::binary|std::ios::trunc);sceneReceipt<<"{\"schemaVersion\":1,\"source\":\"GameplayLabAtmosphereSceneSnapshotDiagnostic\",\"runId\":\""<<runId_<<"\",\"name\":\""<<std::filesystem::path(Names[captureIndex_]).string()<<"\",\"sequenceToken\":"<<captureSequenceToken_.value<<",\"capturedAfterPhase\":"<<(captureSequencePhaseCount_-1)<<",\"width\":"<<scene->identity.width<<",\"height\":"<<scene->identity.height<<",\"format\":\"R16G16B16A16_FLOAT\",\"sourceRowPitch\":"<<scene->sourceRowPitch<<",\"rowBytes\":"<<scene->identity.width*8<<",\"payloadBytes\":"<<scene->rgba16f.size()<<",\"payloadPath\":\""<<scenePayloadPath.filename().string()<<"\",\"payloadSha256\":\""<<CaptureShaHex(scene->pixelSha256)<<"\",\"resourceIdentity\":"<<scene->identity<<"}";sceneReceipt.flush();if(!sceneReceipt){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay atmosphere scene snapshot diagnostic receipt publication failed");}
      }
#endif
      const auto requiredWarmup=temporalFixtureWarmupFrames_?temporalFixtureWarmupFrames_:GameplayTemporalCaptureWarmupFrames3D;
      if(!captureTemporalHistoryGeneration_[captureIndex_]||captureTemporalWarmupFrames_[captureIndex_]<requiredWarmup||captureTemporalReseedReasonMask_[captureIndex_]==0||sequenceResult.receipt.phases.empty()||sequenceResult.receipt.phases.front().raster.temporalHistoryReadGeneration!=captureTemporalHistoryGeneration_[captureIndex_]){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay temporal history receipt did not belong to the warmed capture sequence");}
      // 診斷序列結束後，從全新的 camera cut 固定呈現五幀，再擷取玩家實際
      // presentation。GPU readback 等待長短不得成為 TAA history 的隱含暖機量。
      capturePresentationLogicalToken_=captureSequenceToken_;captureSequenceToken_={};captureSequencePhaseCount_=0;capturePhaseRequestPending_=false;captureReferencePhase_=false;
      capturePresentationWarmupActive_=true;capturePresentationWarmupRemaining_=GameplayPresentationCaptureWarmupFrames3D;capturePresentationWarmupFramesCurrent_=0;
      ResetProductionCameraCut(GameplayTemporalReseedReason3D::CaptureShot);
      capturePending_=false;
      return;
    }
    capturePending_ = false;
  }
  if(temporalFixtureWarmupFrames_&&captureIndex_==2){
    const auto& sequence=captureSequenceReceipts_[1];
    const auto atmosphereStats=engine.GetRendererStats();
    std::ofstream out(captureDirectory_/"temporal-fixture-receipt.json",std::ios::trunc);
    out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    std::ofstream presentationOut(captureDirectory_/"temporal-fixture-presentation-receipt.json",std::ios::trunc);
    presentationOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayThirdDayPresentationParity\",\"capture\":\"third-day.png\",\"diagnosticCapture\":\"third-day-diagnostic.png\",\"logicalToken\":"<<sequence.token.value<<",\"frameSerial\":"<<capturePresentationFrameSerial_[1]<<",\"diagnosticFrameSerial\":"<<captureEffectiveRasterProvenance_[1].frameSerial<<",\"cameraHash\":"<<capturePresentationCameraHash_[1]<<",\"stateHash\":"<<capturePresentationStateHash_[1]<<",\"invariantHash\":"<<capturePresentationInvariantHash_[1]<<",\"paired\":"<<unsigned(capturePresentationPaired_[1])<<",\"msaaEnabled\":"<<unsigned(capturePresentationMsaa_[1])<<",\"taaEnabled\":"<<unsigned(capturePresentationTaa_[1])<<",\"historyUsed\":"<<unsigned(capturePresentationHistoryUsed_[1])<<",\"warmupFrames\":"<<captureTemporalWarmupFrames_[1]<<"}";presentationOut.close();
    out<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayThirdDayProductionTemporalFixture\",\"capture\":\"third-day.png\",\"warmupFrames\":"<<temporalFixtureWarmupFrames_<<",\"baselineWarmupFrames\":"<<GameplayTemporalCaptureWarmupFrames3D<<",\"baselineContract\":\"typed-third-day-warmup-v1\",\"isolationMutation\":\""<<(temporalFixtureCloudOff_?"cloud-off":"none")<<"\",\"recordedWarmupFrames\":"<<captureTemporalWarmupFrames_[1]<<",\"historyGeneration\":"<<captureTemporalHistoryGeneration_[1]<<",\"sequenceComplete\":"<<unsigned(sequence.Complete())<<",\"phaseCount\":"<<sequence.phases.size()<<",\"atmosphereSequenceDispatches\":"<<atmosphereStats.volumetricAtmosphereSequenceDispatches<<",\"atmosphereSequenceReuses\":"<<atmosphereStats.volumetricAtmosphereSequenceReuses<<",\"atmosphereCompositePasses\":"<<atmosphereStats.volumetricAtmosphereCompositePasses<<",\"atmosphereReuseRejects\":"<<atmosphereStats.volumetricAtmosphereSequenceReuseRejects<<",\"atmosphereLastRejectReason\":"<<unsigned(atmosphereStats.volumetricAtmosphereLastSequenceReuseReject)<<",\"atmosphereFingerprint\":"<<atmosphereStats.volumetricAtmosphereSequenceFingerprint<<",\"temporalSequenceDispatches\":"<<atmosphereStats.temporalSequenceDispatches<<",\"temporalSequenceReuses\":"<<atmosphereStats.temporalSequenceReuses<<",\"temporalReuseRejects\":"<<atmosphereStats.temporalSequenceReuseRejects<<",\"temporalLastRejectReason\":"<<unsigned(atmosphereStats.temporalLastSequenceReuseReject)<<",\"temporalFingerprint\":"<<atmosphereStats.temporalSequenceFingerprint<<",\"phaseProvenance\":[";for(std::size_t phase=0;phase<sequence.phases.size();++phase){if(phase)out<<',';const auto& atmosphere=captureAtmospherePhaseProvenance_[1][phase];const auto& temporal=captureTemporalPhaseProvenance_[1][phase];out<<"{\"phase\":"<<phase<<",\"atmosphere\":{\"fingerprint\":"<<atmosphere.fingerprint<<",\"outputIdentity\":"<<atmosphere.outputIdentity<<",\"fence\":"<<atmosphere.submissionFence<<",\"dispatch\":"<<unsigned(atmosphere.dispatched)<<",\"reuse\":"<<unsigned(atmosphere.reused)<<",\"reject\":"<<unsigned(atmosphere.rejection)<<"},\"temporal\":{\"fingerprint\":"<<temporal.fingerprint<<",\"outputIdentity\":"<<temporal.outputIdentity<<",\"fence\":"<<temporal.submissionFence<<",\"dispatch\":"<<unsigned(temporal.dispatched)<<",\"reuse\":"<<unsigned(temporal.reused)<<",\"reject\":"<<unsigned(temporal.rejection)<<"}}";}out<<"]}";out.close();
    engine.Quit();return;
  }
  if (captureIndex_ == Names.size()) {
    // GPU validation can make early frames miss the Ultra frame budget while
    // pipelines and validation bookkeeping warm up. Keep rendering the completed
    // scene until the existing three-consecutive-frame admission gate commits.
    if(engine.RequestedRenderQuality()==RenderQuality3D::Ultra&&engine.RenderQuality()!=RenderQuality3D::Ultra){
      const auto qualityStats=engine.GetRendererStats();
      if(qualityStats.submittedFrames!=captureUltraSettleLastSubmittedFrame_){captureUltraSettleLastSubmittedFrame_=qualityStats.submittedFrames;++captureUltraSettleFrames_;}
      if(captureUltraSettleFrames_>120)throw std::runtime_error(std::format(
        "Gameplay Ultra quality did not commit after capture settle: frames={} gpuNs={} shadowNs={} shadowRecorded={} shadowValid={} captureCopyNs={} captureCopyRecorded={} captureCopyValid={} atmosphereNs={} hzbPrepassNs={} hzbBuildNs={} hzbCullNs={} hzbColorNs={} errors={} warnings={}",
        captureUltraSettleFrames_,qualityStats.lastGpuFrameNanoseconds,
        qualityStats.lastShadowRenderingGpuTiming.nanoseconds,qualityStats.lastShadowRenderingGpuTiming.recorded,qualityStats.lastShadowRenderingGpuTiming.valid,
        qualityStats.lastProductionCaptureCopyGpuTiming.nanoseconds,qualityStats.lastProductionCaptureCopyGpuTiming.recorded,qualityStats.lastProductionCaptureCopyGpuTiming.valid,
        qualityStats.lastVolumetricAtmosphereGpuNanoseconds,qualityStats.lastHzbPrepassGpuNanoseconds,qualityStats.lastHzbBuildGpuNanoseconds,
        qualityStats.lastHzbCullGpuNanoseconds,qualityStats.lastHzbColorGpuNanoseconds,qualityStats.debugErrorCount,qualityStats.debugWarningCount));
      return;
    }
    std::ofstream presentationOut(captureDirectory_/"gameplay-presentation-receipt.json",std::ios::trunc);
    presentationOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabProductionPresentation\",\"captures\":[";
    for(std::size_t index=0;index<Names.size();++index){if(index)presentationOut<<',';presentationOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"frameSerial\":"<<capturePresentationFrameSerial_[index]<<",\"diagnosticFrameSerial\":"<<captureEffectiveRasterProvenance_[index].frameSerial<<",\"cameraHash\":"<<capturePresentationCameraHash_[index]<<",\"stateHash\":"<<capturePresentationStateHash_[index]<<",\"invariantHash\":"<<capturePresentationInvariantHash_[index]<<",\"paired\":"<<unsigned(capturePresentationPaired_[index])<<",\"msaaEnabled\":"<<unsigned(capturePresentationMsaa_[index])<<",\"taaEnabled\":"<<unsigned(capturePresentationTaa_[index])<<",\"historyUsed\":"<<unsigned(capturePresentationHistoryUsed_[index])<<",\"warmupFrames\":"<<captureTemporalWarmupFrames_[index]<<"}";}
    presentationOut<<"]}";presentationOut.close();
    std::ofstream temporalOut(captureDirectory_/"gameplay-temporal-history-receipt.json",std::ios::trunc);temporalOut<<std::setprecision(std::numeric_limits<float>::max_digits10);temporalOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabTemporalHistory\",\"minimumWarmupFrames\":"<<GameplayTemporalCaptureWarmupFrames3D<<",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)temporalOut<<',';temporalOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"historyReseedReasonMask\":"<<captureTemporalReseedReasonMask_[index]<<",\"historyWarmupFrames\":"<<captureTemporalWarmupFrames_[index]<<",\"historyGeneration\":"<<captureTemporalHistoryGeneration_[index]<<"}";}temporalOut<<"]}";temporalOut.close();
    std::ofstream referenceOpacityOut(captureDirectory_/"gameplay-reference-opacity-receipt.json",std::ios::trunc);referenceOpacityOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabReferenceOpacity\",\"buildingIdentitySource\":\"GameplayShowcaseEnvironment3D.Descriptors\",\"buildingIdentityDomain\":\""<<EnvironmentPartitionId<<"\",\"buildingIdentityGeneration\":1,\"descriptorKinds\":[";
    for(std::size_t descriptor=0;descriptor<showcaseEnvironment_.Descriptors().size();++descriptor){if(descriptor)referenceOpacityOut<<',';referenceOpacityOut<<unsigned(showcaseEnvironment_.Descriptors()[descriptor].kind);}
    referenceOpacityOut<<"],\"captures\":[";
    referenceOpacityOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    const auto buildingIdentities=BuildGameplayBuildingIdentities3D(showcaseEnvironment_.Descriptors(),EnvironmentPartitionId);
    for(std::size_t index=0;index<Names.size();++index){if(index)referenceOpacityOut<<',';const bool third=(index<9&&index%3==1)||index>=9;referenceOpacityOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"referencePhase\":"<<unsigned(third)<<",\"overrideMode\":\""<<(captureReferenceOverrideMode_[index]?"RemoveAll":"NoOverride")<<"\",\"removalThreshold\":"<<captureReferenceRemovalThreshold_[index]<<",\"requestedReferenceCount\":"<<captureReferenceRequestedCount_[index]<<",\"matchedReferenceCount\":"<<captureReferenceOpacityMatches_[index]<<",\"removedReferenceCount\":"<<captureReferenceRemovedCount_[index]<<",\"removedCoverage\":"<<captureReferenceRemovedCoverage_[index]<<",\"effectiveBuildingUnion\":"<<captureEffectiveBuildingUnion_[index]<<",\"effectiveFrameSerial\":"<<captureEffectiveRasterProvenance_[index].frameSerial<<",\"referenceFrameSerial\":"<<captureReferenceRasterProvenance_[index].frameSerial<<",\"effectiveSequenceIndex\":1,\"referenceSequenceIndex\":2,\"effectiveIncludingOpacity\":"<<captureEffectiveAcceptedIncludingOpacity_[index]<<",\"referenceIncludingOpacity\":"<<captureReferenceAcceptedIncludingOpacity_[index]<<",\"effectiveExcludingOpacity\":"<<captureEffectiveAcceptedExcludingOpacity_[index]<<",\"referenceExcludingOpacity\":"<<captureReferenceAcceptedExcludingOpacity_[index]<<",\"effectiveIdentityOrder\":"<<captureEffectiveIdentityOrderHash_[index]<<",\"referenceIdentityOrder\":"<<captureReferenceIdentityOrderHash_[index]<<",\"parts\":[";for(std::size_t part=0;part<6;++part){if(part)referenceOpacityOut<<',';referenceOpacityOut<<"{\"stableLow\":"<<buildingIdentities[part].id.low<<",\"requested\":"<<unsigned(captureReferenceRequestedById_[index][part])<<",\"matched\":"<<unsigned(captureReferenceMatchedById_[index][part])<<",\"missing\":"<<unsigned(captureReferenceMissingById_[index][part])<<",\"generationConflict\":"<<unsigned(captureReferenceConflictById_[index][part])<<",\"appliedOpacity\":";if(captureReferenceRequestedById_[index][part])referenceOpacityOut<<captureReferenceAppliedOpacityById_[index][part];else referenceOpacityOut<<"null";referenceOpacityOut<<'}';}referenceOpacityOut<<"]}";}
    referenceOpacityOut<<"]}";referenceOpacityOut.close();
    if(!authoritativeTrajectory_.checkpointCount||std::ranges::any_of(authoritativeTrajectory_.perAttackCheckpointCount,[](std::uint32_t count){return count==0;}))
      throw std::runtime_error("Gameplay authoritative trajectory evidence was incomplete");
    std::ofstream out(captureDirectory_ / "gameplay-receipt.json",
                      std::ios::trunc);
    out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    const auto s = rules_.GetStats();
    const auto p = physics_.Stats();
    const auto r = engine.GetRendererStats();
    const auto c = combat_->Stats();
    const auto palette = palette_->Stats();
    const auto femaleMaterialStats = femaleMaterialLibrary_.Stats();
    const auto maleMaterialStats = maleMaterialLibrary_.Stats();
    if(femaleMaterialStats.materials!=characterMaterialEvidence_.femaleCharacterMaterialCount||
       maleMaterialStats.materials!=characterMaterialEvidence_.maleCharacterMaterialCount)
      throw std::runtime_error("Gameplay character material evidence drifted");
    if (captureAuthoritativePhysicsSteps_ != 240)
      throw std::runtime_error(
          "Gameplay capture missed its authoritative physics tick");
    const auto frameBoundSkinnedInstances=*std::ranges::max_element(captureFrameSubmittedSkinned_);
    out << std::format(
        "{{\"runId\":\"{}\",\"source\":\"GameplayLabFramebuffer\",\"requestedRenderQuality\":\"{}\",\"actualRenderQuality\":\"{}\",\"developmentUltraRendererApplied\":{},\"developmentUltraTransitionApplied\":{},\"sourceFingerprint\":\"{}\",\"sourceFingerprintPassed\":{},\"entry\":\"-"
        "-capture-demo\",\"captures\":{},\"ticks\":{},"
        "\"authoritativePhysicsTick\":240,\"authoritativePhysicsSteps\":{},"
        "\"entities\":{},\"physicsBodies\":{},"
        "\"physicsSteps\":{},\"cameraSwitches\":{},\"jumps\":{},\"crouches\":{}"
        ",\"palettePreviews\":{},\"paletteValidPreviews\":{},"
        "\"paletteInvalidPreviews\":{},\"selectedCategory\":{},"
        "\"paletteCommits\":{},\"combatSweeps\":{},\"combatHits\":{},"
        "\"combatKills\":{},\"monsterHealth\":{},\"animationState\":{},"
        "\"monsterAnimationState\":{},\"corpseVisible\":{},\"femaleGpuDraws\":{"
        "},\"maleGpuDraws\":{},\"femaleModelIdentity\":{},"
        "\"maleModelIdentity\":{},\"femalePoseHash\":{},\"malePoseHash\":{},"
        "\"minimumControllerHeight\":{},\"headroomRejected\":{},"
        "\"crouchFeetInvariant\":{},\"weaponAttachmentPassed\":{},"
        "\"weaponHitWindowEvidence\":{},\"weaponPbrDraws\":{},"
        "\"weaponAnimationSyncPassed\":{},\"maximumWeaponPhaseError\":{},"
        "\"firstPersonBodyHidden\":{},\"weaponTrajectoryHash\":{},"
        "\"attackTrajectoryHashes\":[{},{},{}],"
        "\"authoritativeTrajectoryHash\":{},\"authoritativeAttackTrajectoryHashes\":[{},{},{}],"
        "\"authoritativeTrajectoryCheckpointCount\":{},\"authoritativeAttackCheckpointCounts\":[{},{},{}],"
        "\"femaleFirstPersonArms\":{{\"firstPersonArmSubmeshCount\":{},\"firstPersonArmTriangleCount\":{},\"firstPersonLeftArmTriangleCount\":{},\"firstPersonRightArmTriangleCount\":{}}},"
        "\"maleFirstPersonArms\":{{\"firstPersonArmSubmeshCount\":{},\"firstPersonArmTriangleCount\":{},\"firstPersonLeftArmTriangleCount\":{},\"firstPersonRightArmTriangleCount\":{}}},"
        "\"femaleCharacterMaterialCount\":{},\"maleCharacterMaterialCount\":{},\"characterMaterialCount\":{},"
        "\"characterMaterialLoadFailures\":{},\"skinnedGroups\":{},"
        "\"skinnedInstances\":{},\"rejectedSkinnedGroups\":{},"
        "\"residentSkinnedMeshBytes\":{},\"gpuValidationRequested\":{},"
        "\"gpuValidationAvailable\":{},\"gpuValidationExecuted\":{},"
        "\"debugLayerEnabled\":{},\"gpuValidationPassed\":{},\"gpuErrors\":{},"
        "\"gpuWarnings\":{},\"lighting\":[{{\"phase\":\"day\",\"sun\":[0.38,-1."
        "0,0.24],\"intensity\":1.8,\"exposure\":1.0,\"fogDensity\":0.006,"
        "\"shadow\":1}},{{\"phase\":\"dusk\",\"sun\":[-0.65,-0.34,0.28],"
        "\"intensity\":1.2,\"exposure\":0.9,\"fogDensity\":0.014,\"shadow\":1}}"
        ",{{\"phase\":\"night\",\"sun\":[0.25,-0.72,-0.34],\"intensity\":0.48,"
        "\"exposure\":0.72,\"fogDensity\":0.022,\"shadow\":1}}],\"developmentUltraDiagnostic\":{}}}",
        runId_,RenderQualityName3D(engine.RequestedRenderQuality()),RenderQualityName3D(engine.RenderQuality()),engine.DevelopmentUltraReceipt().rendererApplied?1:0,engine.DevelopmentUltraReceipt().transitionPolicyApplied?1:0, actualSourceFingerprint_,sourceFingerprintPassed_?1:0,captureIndex_,s.ticks, captureAuthoritativePhysicsSteps_, s.entities,
        p.bodies, p.steps, s.cameraSwitches,
        controller_.Stats().jumps, s.crouches, palette.previews,
        palette.validPreviews, palette.collisionRejected,
        unsigned(rules_.Category()), palette.commits, c.sweeps, c.hits, c.kills,
        combat_->Monster().health, unsigned((rules_.Female()?femaleAnimation_:maleAnimation_).State()),
        unsigned(monsterAnimation_.State()),
        combatSpawned_ && !combat_->Monster().alive ? 1 : 0, femaleGpuDraws_,
        maleGpuDraws_, femaleMesh_.handle.resourceIdentity,
        maleMesh_.handle.resourceIdentity, HashPose(femalePose_),
        HashPose(malePose_), minimumControllerHeight_, s.headroomRejected,
        crouchFeetInvariant_ ? 1 : 0, weaponAttachmentPassed_ ? 1 : 0,
        weaponHitWindowEvidence_ ? 1 : 0, weaponPbrDraws_,
        weaponAnimationSyncPassed_ ? 1 : 0, maximumWeaponPhaseError_,
        firstPersonBodyHidden_ ? 1 : 0, authoritativeTrajectory_.authoritativeHash,
        authoritativeTrajectory_.perAttackHash[0], authoritativeTrajectory_.perAttackHash[1], authoritativeTrajectory_.perAttackHash[2],
        authoritativeTrajectory_.authoritativeHash,
        authoritativeTrajectory_.perAttackHash[0],authoritativeTrajectory_.perAttackHash[1],authoritativeTrajectory_.perAttackHash[2],
        authoritativeTrajectory_.checkpointCount,
        authoritativeTrajectory_.perAttackCheckpointCount[0],authoritativeTrajectory_.perAttackCheckpointCount[1],authoritativeTrajectory_.perAttackCheckpointCount[2],
        femaleFirstPersonArmsEvidence_.submeshCount,femaleFirstPersonArmsEvidence_.retainedTriangles,femaleFirstPersonArmsEvidence_.leftArmTriangles,femaleFirstPersonArmsEvidence_.rightArmTriangles,
        maleFirstPersonArmsEvidence_.submeshCount,maleFirstPersonArmsEvidence_.retainedTriangles,maleFirstPersonArmsEvidence_.leftArmTriangles,maleFirstPersonArmsEvidence_.rightArmTriangles,
        characterMaterialEvidence_.femaleCharacterMaterialCount,characterMaterialEvidence_.maleCharacterMaterialCount,characterMaterialEvidence_.characterMaterialCount,
        femaleMaterialStats.loadFailures + maleMaterialStats.loadFailures,
        frameBoundSkinnedInstances?1u:0u, frameBoundSkinnedInstances,
        r.rejectedSkinnedGroups, r.residentSkinnedMeshBytes,
        r.gpuValidationRequested ? 1 : 0,
        r.gpuValidationAvailable ? 1 : 0, r.gpuValidationExecuted ? 1 : 0,
        r.debugLayerEnabled ? 1 : 0, r.gpuValidationPassed ? 1 : 0,
        r.debugErrorCount, r.debugWarningCount,GameplayUltraDiagnosticJson(engine));
    std::ofstream captureOut(captureDirectory_ / "gameplay-character-capture-receipt.json", std::ios::trunc);captureOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    captureOut << "{\"runId\":\"" << runId_ << "\",\"source\":\"GameplayLabCharacterPresentation\",\"requestedRenderQuality\":\""<<RenderQualityName3D(engine.RequestedRenderQuality())<<"\",\"actualRenderQuality\":\""<<RenderQualityName3D(engine.RenderQuality())<<"\",\"developmentUltraRendererApplied\":"<<(engine.DevelopmentUltraReceipt().rendererApplied?1:0)<<",\"developmentUltraTransitionApplied\":"<<(engine.DevelopmentUltraReceipt().transitionPolicyApplied?1:0)<<",\"sourceFingerprint\":\""<<actualSourceFingerprint_<<"\",\"femaleCharacterMaterialCount\":"<<characterMaterialEvidence_.femaleCharacterMaterialCount<<",\"maleCharacterMaterialCount\":"<<characterMaterialEvidence_.maleCharacterMaterialCount<<",\"characterMaterialCount\":"<<characterMaterialEvidence_.characterMaterialCount<<",\"femaleBodyMaterialSlotCount\":"<<(candidate7Active_?candidate7Materials_[0].size():characterMaterialEvidence_.femaleCharacterMaterialCount)<<",\"maleBodyMaterialSlotCount\":"<<characterMaterialEvidence_.maleCharacterMaterialCount<<","
      "\"femaleFirstPersonArms\":{\"firstPersonArmSubmeshCount\":"<<femaleFirstPersonArmsEvidence_.submeshCount<<",\"firstPersonArmTriangleCount\":"<<femaleFirstPersonArmsEvidence_.retainedTriangles<<",\"firstPersonLeftArmTriangleCount\":"<<femaleFirstPersonArmsEvidence_.leftArmTriangles<<",\"firstPersonRightArmTriangleCount\":"<<femaleFirstPersonArmsEvidence_.rightArmTriangles<<"},"
      "\"maleFirstPersonArms\":{\"firstPersonArmSubmeshCount\":"<<maleFirstPersonArmsEvidence_.submeshCount<<",\"firstPersonArmTriangleCount\":"<<maleFirstPersonArmsEvidence_.retainedTriangles<<",\"firstPersonLeftArmTriangleCount\":"<<maleFirstPersonArmsEvidence_.leftArmTriangles<<",\"firstPersonRightArmTriangleCount\":"<<maleFirstPersonArmsEvidence_.rightArmTriangles<<"},\"paletteReplayDiagnostics\":[";for(std::size_t replayIndex=0;replayIndex<capturePaletteReplayDiagnostics_.size();++replayIndex){if(replayIndex)captureOut<<',';const auto& replay=capturePaletteReplayDiagnostics_[replayIndex];captureOut<<"{\"observed\":"<<(replay.observed?1:0)<<",\"inputSource\":\"FixedTickReplayAuthoritativeGodCamera\",\"liveSuppressed\":"<<(replay.liveSuppressed?1:0)<<",\"inputBits\":"<<replay.inputBits<<",\"deltaSource\":\"CaptureFixed60Hz\",\"gameplayDelta\":"<<replay.gameplayDelta<<",\"gameplayDeltaRawBits\":"<<std::bit_cast<std::uint32_t>(replay.gameplayDelta)<<",\"cameraHash\":"<<replay.cameraHash<<",\"cameraAuthorityInputHash\":"<<replay.cameraAuthorityInputHash<<",\"cameraSubjectPoseHash\":"<<replay.cameraSubjectPoseHash<<",\"cameraProjectionMode\":"<<unsigned(replay.projectionMode)<<",\"cameraProjectionModeName\":\""<<CaptureProjectionName(replay.projectionMode)<<"\",\"cameraRawBits\":[";for(std::size_t bitIndex=0;bitIndex<replay.cameraRawBits.size();++bitIndex){if(bitIndex)captureOut<<',';captureOut<<replay.cameraRawBits[bitIndex];}captureOut<<"],\"placementRaySource\":\"ScreenPointToRayAuthoritativeGodCamera\",\"placementPlaneSource\":\"WorldYPlane\",\"placementGridSource\":\"ObjectPalette2mGrid\",\"placementPlaneHeight\":"<<replay.placementPlaneHeight<<",\"placementPlaneHeightRawBits\":"<<std::bit_cast<std::uint32_t>(replay.placementPlaneHeight)<<",\"gridStep\":"<<replay.gridStep<<",\"gridStepRawBits\":"<<std::bit_cast<std::uint32_t>(replay.gridStep)<<",\"footPlacementCameraDistanceSource\":\"PreviousFixedCamera\",\"footPlacementCameraDistance\":"<<replay.footPlacementCameraDistance<<",\"footPlacementCameraDistanceRawBits\":"<<std::bit_cast<std::uint32_t>(replay.footPlacementCameraDistance)<<",\"tick\":"<<replay.tick<<",\"normalizedPointer\":["<<replay.normalizedPointer.x<<','<<replay.normalizedPointer.y<<"],\"rayOrigin\":["<<replay.rayOrigin.x<<','<<replay.rayOrigin.y<<','<<replay.rayOrigin.z<<"],\"rayDirection\":["<<replay.rayDirection.x<<','<<replay.rayDirection.y<<','<<replay.rayDirection.z<<"],\"hitValid\":"<<(replay.hitValid?1:0)<<",\"projectedHit\":["<<replay.projectedHit.x<<','<<replay.projectedHit.y<<','<<replay.projectedHit.z<<"],\"previewActive\":"<<(replay.previewActive?1:0)<<",\"previewValid\":"<<(replay.previewValid?1:0)<<",\"snappedTarget\":["<<replay.snappedTarget.x<<','<<replay.snappedTarget.y<<','<<replay.snappedTarget.z<<"],\"undoDepth\":"<<replay.undoDepth<<",\"redoDepth\":"<<replay.redoDepth<<'}';}captureOut<<"],\"captures\":[";
    for(std::size_t index=0;index<Names.size();++index){
      if(index)captureOut<<",";
      const bool female=index<5||index==10;
      const char* camera=index<9?(index%3==0?"first":index%3==1?"third":"god"):"third";
      const bool thirdCamera=std::string_view(camera)=="third";
      if((thirdCamera&&(!std::isfinite(captureBuilderTopCenterRayElevation_[index])||
                        captureBuilderTopCenterRayElevation_[index]<captureBuilderMinimumTopCenterRayElevation_[index]||
                        captureBuilderMinimumTopCenterRayElevation_[index]<.03f||
                        !captureBuilderRejectedHorizon_[index]))||
         (!thirdCamera&&(captureBuilderTopCenterRayElevation_[index]!=0.f||
                         captureBuilderMinimumTopCenterRayElevation_[index]!=0.f||
                         captureBuilderRejectedHorizon_[index]!=0)))
        throw std::runtime_error("Gameplay character horizon evidence mismatch");
      const auto& firstPersonArms=female?femaleFirstPersonArmsEvidence_:maleFirstPersonArmsEvidence_;
      const bool firstCamera=std::string_view(camera)=="first";
      const std::uint32_t firstPersonArmSubmeshCount=firstCamera?firstPersonArms.submeshCount:0;
      const std::uint32_t firstPersonArmTriangleCount=firstCamera?firstPersonArms.retainedTriangles:0;
      const std::uint32_t firstPersonLeftArmTriangleCount=firstCamera?firstPersonArms.leftArmTriangles:0;
      const std::uint32_t firstPersonRightArmTriangleCount=firstCamera?firstPersonArms.rightArmTriangles:0;
      const auto expectedThirdPersonSlots=female&&candidate7Active_?candidate7Materials_[0].size():std::size_t(female?characterMaterialEvidence_.femaleCharacterMaterialCount:characterMaterialEvidence_.maleCharacterMaterialCount);
      if((firstCamera&&captureBodyMaterialSlots_[index]!=firstPersonArmSubmeshCount)||
         (!firstCamera&&captureBodyMaterialSlots_[index]!=expectedThirdPersonSlots)||
         (!firstCamera&&(firstPersonArmSubmeshCount||firstPersonArmTriangleCount||firstPersonLeftArmTriangleCount||firstPersonRightArmTriangleCount)))
        throw std::runtime_error(std::format(
          "Gameplay first-person arms capture evidence mismatch: index={} camera={} female={} actualBodySlots={} expectedFirstPersonSlots={} expectedThirdPersonSlots={} armTriangles={}/{}/{}",
          index,camera,female?1:0,captureBodyMaterialSlots_[index],firstPersonArmSubmeshCount,
          expectedThirdPersonSlots,
          firstPersonArmTriangleCount,firstPersonLeftArmTriangleCount,firstPersonRightArmTriangleCount));
      captureOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"character\":\""<<(female?"female-f2":"male-m2")<<"\",\"camera\":\""<<camera<<"\",\"cameraPath\":\"production-shared\",\"crouch\":"<<(index==4?1:0)<<",\"attack\":"<<(index==10?1:0)<<",\"bodyMaterialSlots\":"<<captureBodyMaterialSlots_[index]<<",\"firstPersonArmSubmeshCount\":"<<firstPersonArmSubmeshCount<<",\"firstPersonArmTriangleCount\":"<<firstPersonArmTriangleCount<<",\"firstPersonLeftArmTriangleCount\":"<<firstPersonLeftArmTriangleCount<<",\"firstPersonRightArmTriangleCount\":"<<firstPersonRightArmTriangleCount<<",\"weaponMaterialRanges\":"<<captureWeaponMaterialRanges_[index]<<",\"weaponAnchorError\":"<<captureWeaponAnchorErrors_[index]<<",\"weaponScreenCoverage\":"<<captureWeaponScreenCoverage_[index]<<",\"weaponMinimumDepth\":"<<captureWeaponMinimumDepth_[index]<<",\"weaponCenterExcluded\":"<<unsigned(captureWeaponCenterExcluded_[index])<<",\"weaponNearClipClear\":"<<unsigned(captureWeaponNearClipClear_[index])<<",\"weaponHudOverlap\":"<<unsigned(captureWeaponHudOverlap_[index])<<",\"weaponGripX\":"<<captureWeaponGripX_[index]<<",\"weaponGripY\":"<<captureWeaponGripY_[index]<<",\"weaponBladeY\":"<<captureWeaponBladeY_[index]<<",\"gripEdgeAnchored\":"<<unsigned(captureWeaponGripEdgeAnchored_[index])<<",\"bladeAboveGrip\":"<<unsigned(captureWeaponBladeAboveGrip_[index])<<",\"objectIdGeneration\":"<<captureObjectIdGeneration_[index]<<",\"objectIdMaskHash\":"<<captureObjectIdMaskHash_[index]<<",\"objectIdMask\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-object-id.png\",\"objectIdSubmittedOpaque\":"<<captureObjectIdSubmitted_[index]<<",\"objectIdOpaqueComplete\":"<<unsigned(captureObjectIdOpaqueComplete_[index])<<",\"objectIdMaskedComplete\":"<<unsigned(captureObjectIdMaskedComplete_[index])<<",\"objectIdSkinnedComplete\":"<<unsigned(captureObjectIdSkinnedComplete_[index])<<",\"occluderStableHigh\":"<<captureOccluderStableHigh_[index]<<",\"occluderStableLow\":"<<captureOccluderStableLow_[index]<<",\"occluderGeneration\":"<<captureOccluderGeneration_[index]<<",\"rawOccluderCoverage\":"<<captureRawOccluderCoverage_[index]<<",\"effectiveObjectIdSupported\":"<<unsigned(captureEffectiveObjectIdSupported_[index])<<",\"occluderCoverage\":";
      if(captureEffectiveObjectIdSupported_[index])captureOut<<captureOccluderCoverage_[index];else captureOut<<"null";
      captureOut<<",\"rawTokenMetadataSha256\":\""<<CaptureShaHex(captureRawTokenMetadataSha256_[index])<<"\",\"effectiveTokenMetadataSha256\":\""<<CaptureShaHex(captureEffectiveTokenMetadataSha256_[index])<<"\"";
      captureOut<<",\"rawColor\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-raw.png\",\"rawObjectIdMask\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-raw-object-id.png\",\"rawObjectIdMaskHash\":"<<captureRawObjectIdMaskHash_[index]<<",\"requestDrawRevision\":"<<captureRequestDrawRevision_[index]<<",\"requestCameraHash\":"<<captureRequestCameraHash_[index]<<",\"requestStateHash\":"<<captureRequestStateHash_[index]<<",\"rawAcceptedExcludingOpacity\":"<<captureRawAcceptedExcludingOpacity_[index]<<",\"effectiveAcceptedExcludingOpacity\":"<<captureEffectiveAcceptedExcludingOpacity_[index]<<",\"rawAcceptedIncludingOpacity\":"<<captureRawAcceptedIncludingOpacity_[index]<<",\"effectiveAcceptedIncludingOpacity\":"<<captureEffectiveAcceptedIncludingOpacity_[index]<<",\"rawIdentityOrderHash\":"<<captureRawIdentityOrderHash_[index]<<",\"effectiveIdentityOrderHash\":"<<captureEffectiveIdentityOrderHash_[index]<<",\"fadedStableHigh\":"<<captureFadedStableHigh_[index]<<",\"fadedStableLow\":"<<captureFadedStableLow_[index]<<",\"fadedGeneration\":"<<captureFadedGeneration_[index]<<",\"policyFadeStableHigh\":"<<capturePolicyFadeStableHigh_[index]<<",\"policyFadeStableLow\":"<<capturePolicyFadeStableLow_[index]<<",\"policyFadeGeneration\":"<<capturePolicyFadeGeneration_[index]<<",\"policyFadeOpacity\":"<<capturePolicyFadeOpacity_[index]<<",\"opacityMatchedStatic\":"<<captureOpacityMatchedStatic_[index]<<",\"opacityMatchedMultipart\":"<<captureOpacityMatchedMultipart_[index]<<",\"opacityMissing\":"<<unsigned(captureOpacityMissing_[index])<<",\"opacityGenerationConflict\":"<<unsigned(captureOpacityGenerationConflict_[index])<<",\"occlusionAdjusted\":"<<unsigned(captureOcclusionAdjusted_[index])<<",\"depthCaptureSupported\":"<<unsigned(captureDepthSupported_[index])<<",\"depthGatePass\":"<<unsigned(captureDepthSupported_[index]&&(!thirdCamera||captureGpuFeetContactPassed_[index]))<<",\"rawDepth\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-raw-depth.png\",\"effectiveDepth\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-depth.png\",\"referenceDepth\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-reference-depth.png\",\"rawDepthSha256\":\""<<CaptureShaHex(captureRawDepthSha256_[index])<<"\",\"effectiveDepthSha256\":\""<<CaptureShaHex(captureEffectiveDepthSha256_[index])<<"\",\"referenceDepthSha256\":\""<<CaptureShaHex(captureReferenceDepthSha256_[index])<<"\",\"rawDepthPixelSha256\":\""<<CaptureShaHex(captureRawDepthSha256_[index])<<"\",\"effectiveDepthPixelSha256\":\""<<CaptureShaHex(captureEffectiveDepthSha256_[index])<<"\",\"referenceDepthPixelSha256\":\""<<CaptureShaHex(captureReferenceDepthSha256_[index])<<"\",\"rawDepthFinalFileSha256\":\""<<CaptureShaHex(captureRawDepthFinalFileSha256_[index])<<"\",\"effectiveDepthFinalFileSha256\":\""<<CaptureShaHex(captureEffectiveDepthFinalFileSha256_[index])<<"\",\"referenceDepthFinalFileSha256\":\""<<CaptureShaHex(captureReferenceDepthFinalFileSha256_[index])<<"\",\"rawDepthFinalFileBytes\":"<<captureRawDepthFinalFileBytes_[index]<<",\"effectiveDepthFinalFileBytes\":"<<captureEffectiveDepthFinalFileBytes_[index]<<",\"referenceDepthFinalFileBytes\":"<<captureReferenceDepthFinalFileBytes_[index]<<",\"depthGeneration\":"<<captureDepthGeneration_[index]<<",\"gpuContactMask\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-contact.png\",\"leftFootGroundGapPixels\":"<<captureLeftFootGroundGapPixels_[index]<<",\"rightFootGroundGapPixels\":"<<captureRightFootGroundGapPixels_[index]<<",\"leftFootDepthDelta\":"<<captureLeftFootDepthDelta_[index]<<",\"rightFootDepthDelta\":"<<captureRightFootDepthDelta_[index]<<",\"gpuFeetContactPassed\":"<<unsigned(captureGpuFeetContactPassed_[index])<<",\"groundStableLow\":"<<captureGroundStableLow_[index]<<",\"groundSemantic\":\"terrain\",\"feetGroundError\":"<<captureFeetGroundError_[index]<<",\"feetContactPassed\":"<<unsigned(captureFeetContactPassed_[index]);
      const auto& canonicalState=captureCanonicalState_[index];captureOut<<",\"captureStateSchemaVersion\":"<<canonicalState.schemaVersion<<",\"captureStateValid\":"<<(canonicalState.valid?1:0)<<",\"captureStateEntityCount\":"<<canonicalState.entityDiagnostics.size()<<",\"captureStateCanonical\":"<<canonicalState.canonical<<",\"captureStateLegacy\":"<<canonicalState.legacy<<",\"captureStateEntities\":"<<canonicalState.entities<<",\"captureStateTopology\":"<<canonicalState.topology<<",\"captureStateTransform\":"<<canonicalState.transform<<",\"captureStateController\":"<<canonicalState.controller<<",\"captureStateYawPitch\":"<<canonicalState.yawPitch<<",\"captureStateRules\":"<<canonicalState.rules<<",\"captureStateCamera\":"<<canonicalState.camera<<",\"captureStatePalette\":"<<canonicalState.palette<<",\"captureStateAnimation\":"<<canonicalState.animation<<",\"captureStatePhysics\":"<<canonicalState.physics<<",\"captureStateEntityDiagnostics\":[";for(std::size_t entityIndex=0;entityIndex<canonicalState.entityDiagnostics.size();++entityIndex){if(entityIndex)captureOut<<',';const auto& entity=canonicalState.entityDiagnostics[entityIndex];captureOut<<"{\"stableId\":"<<entity.stableId<<",\"source\":\""<<(entity.initialSeed?"InitialSeed":"PaletteReplay")<<"\",\"category\":"<<unsigned(entity.category)<<",\"dynamic\":"<<(entity.dynamic?1:0)<<",\"alive\":"<<(entity.alive?1:0)<<",\"position\":["<<entity.position.x<<','<<entity.position.y<<','<<entity.position.z<<"],\"positionRawBits\":["<<entity.rawBits[0]<<','<<entity.rawBits[1]<<','<<entity.rawBits[2]<<"],\"rotation\":"<<entity.rotation<<",\"rotationRawBits\":"<<entity.rawBits[3]<<",\"topologyHash\":"<<entity.topologyHash<<",\"transformHash\":"<<entity.transformHash<<",\"bodyAvailable\":"<<(entity.bodyAvailable?1:0)<<",\"bodyPosition\":["<<entity.bodyPosition.x<<','<<entity.bodyPosition.y<<','<<entity.bodyPosition.z<<"],\"bodyVelocity\":["<<entity.bodyVelocity.x<<','<<entity.bodyVelocity.y<<','<<entity.bodyVelocity.z<<"],\"sceneAvailable\":"<<(entity.sceneAvailable?1:0)<<",\"scenePosition\":["<<entity.scenePosition.x<<','<<entity.scenePosition.y<<','<<entity.scenePosition.z<<"]}";}captureOut<<']';const auto& frameRender=captureSequenceReceipts_[index].phases[1].determinism.render;WriteIntegrityJson(captureOut,captureDeterminismFrameSerial_[index],captureDeterminismIntegrity_[index],captureCanonicalDrawFingerprint_[index],&frameRender);
      captureOut<<",\"contactArtifactSha256\":\""<<CaptureShaHex(captureContactArtifactSha256_[index])<<"\",\"contactLeftFootPixels\":"<<captureContactLeftPixels_[index]<<",\"contactRightFootPixels\":"<<captureContactRightPixels_[index]<<",\"contactGroundSupportPixels\":"<<captureContactGroundSupportPixels_[index]<<",\"contactShadowPixels\":"<<captureContactShadowPixels_[index]<<",\"contactFrameSerial\":"<<captureContactFrameSerial_[index]<<",\"contactPoseHash\":"<<captureContactPoseHash_[index];
      captureOut<<",\"playerPixels\":"<<capturePlayerPixels_[index]<<",\"companionPixels\":"<<captureCompanionPixels_[index]<<",\"actorGapPixels\":"<<captureActorGapPixels_[index]<<",\"actorSeparationPassed\":"<<unsigned(captureActorSeparationPassed_[index])<<",\"topCenterRayElevation\":"<<captureBuilderTopCenterRayElevation_[index]<<",\"topCenterRayElevationRawBits\":"<<std::bit_cast<std::uint32_t>(captureBuilderTopCenterRayElevation_[index])<<",\"minimumTopCenterRayElevation\":"<<captureBuilderMinimumTopCenterRayElevation_[index]<<",\"minimumTopCenterRayElevationRawBits\":"<<std::bit_cast<std::uint32_t>(captureBuilderMinimumTopCenterRayElevation_[index])<<",\"rejectedHorizon\":"<<captureBuilderRejectedHorizon_[index];
      captureOut<<",\"rawBuildingCoverage151To156\":[";for(std::size_t part=0;part<6;++part){if(part)captureOut<<",";captureOut<<captureRawBuildingCoverage_[index][part];}
      captureOut<<"],\"effectiveBuildingCoverage151To156\":[";for(std::size_t part=0;part<6;++part){if(part)captureOut<<",";captureOut<<captureEffectiveBuildingCoverage_[index][part];}
      captureOut<<"],\"rawBuildingUnion\":"<<captureRawBuildingUnion_[index]<<",\"effectiveBuildingUnion\":"<<captureEffectiveBuildingUnion_[index]<<",\"rawMaximumVisibleStableHigh\":"<<captureRawMaximumVisibleStableHigh_[index]<<",\"rawMaximumVisibleStableLow\":"<<captureRawMaximumVisibleStableLow_[index]<<",\"rawMaximumVisibleGeneration\":"<<captureRawMaximumVisibleGeneration_[index]<<",\"rawMaximumVisibleCoverage\":"<<captureRawMaximumVisibleCoverage_[index]<<",\"effectiveMaximumVisibleStableHigh\":"<<captureEffectiveMaximumVisibleStableHigh_[index]<<",\"effectiveMaximumVisibleStableLow\":"<<captureEffectiveMaximumVisibleStableLow_[index]<<",\"effectiveMaximumVisibleGeneration\":"<<captureEffectiveMaximumVisibleGeneration_[index]<<",\"effectiveMaximumVisibleCoverage\":"<<captureEffectiveMaximumVisibleCoverage_[index]<<",\"characterReferenceSupported\":"<<unsigned(captureCharacterReferenceSupported_[index])<<",\"characterReferenceDiagnosticOnly\":1,\"characterReferenceColor\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-reference.png\",\"characterReferenceMask\":\""<<std::filesystem::path(Names[index]).stem().string()<<"-reference-object-id.png\",\"characterReferenceCoverage\":"<<captureCharacterReferenceCoverage_[index]<<",\"characterEffectiveCoverage\":"<<captureCharacterEffectiveCoverage_[index]<<",\"characterVisibility\":"<<captureCharacterVisibility_[index]<<",\"characterSilhouetteIou\":"<<captureCharacterSilhouetteIou_[index]<<",\"referenceOpacityMatches\":"<<captureReferenceOpacityMatches_[index]<<",\"referenceAcceptedExcludingOpacity\":"<<captureReferenceAcceptedExcludingOpacity_[index]<<",\"referenceAcceptedIncludingOpacity\":"<<captureReferenceAcceptedIncludingOpacity_[index]<<",\"referenceIdentityOrderHash\":"<<captureReferenceIdentityOrderHash_[index]<<",\"rawFemalePoseHash\":"<<captureRawFemalePoseHash_[index]<<",\"effectiveFemalePoseHash\":"<<captureEffectiveFemalePoseHash_[index]<<",\"referenceFemalePoseHash\":"<<captureReferenceFemalePoseHash_[index]<<",\"rawMalePoseHash\":"<<captureRawMalePoseHash_[index]<<",\"effectiveMalePoseHash\":"<<captureEffectiveMalePoseHash_[index]<<",\"referenceMalePoseHash\":"<<captureReferenceMalePoseHash_[index]<<",\"rawMonsterPoseHash\":"<<captureRawMonsterPoseHash_[index]<<",\"effectiveMonsterPoseHash\":"<<captureEffectiveMonsterPoseHash_[index]<<",\"referenceMonsterPoseHash\":"<<captureReferenceMonsterPoseHash_[index]<<",\"rawFootPlacementHash\":"<<captureRawFootPlacementHash_[index]<<",\"effectiveFootPlacementHash\":"<<captureEffectiveFootPlacementHash_[index]<<",\"referenceFootPlacementHash\":"<<captureReferenceFootPlacementHash_[index]<<"}";
    }
    captureOut<<"],\"firstPersonViewmodels\":[";bool firstViewmodelEntry=true;for(std::size_t index=0;index<Names.size();++index){const bool first=index<9&&index%3==0;if(!first)continue;if(!firstViewmodelEntry)captureOut<<',';firstViewmodelEntry=false;captureOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"armsIdentity\":["<<GameplayCaptureFirstPersonArmsIdentity3D.id.high<<','<<GameplayCaptureFirstPersonArmsIdentity3D.id.low<<','<<GameplayCaptureFirstPersonArmsIdentity3D.generation<<"],\"armsToken\":"<<captureFirstPersonArmsToken_[index]<<",\"armsPixels\":"<<captureFirstPersonArmsPixels_[index]<<",\"armsBounds\":["<<captureFirstPersonArmsMinimumX_[index]<<','<<captureFirstPersonArmsMinimumY_[index]<<','<<captureFirstPersonArmsMaximumX_[index]<<','<<captureFirstPersonArmsMaximumY_[index]<<"],\"weaponIdentity\":["<<GameplayCaptureWeaponIdentity3D.id.high<<','<<GameplayCaptureWeaponIdentity3D.id.low<<','<<GameplayCaptureWeaponIdentity3D.generation<<"],\"weaponToken\":"<<captureFirstPersonWeaponToken_[index]<<",\"weaponPixels\":"<<captureFirstPersonWeaponPixels_[index]<<",\"weaponBounds\":["<<captureFirstPersonWeaponMinimumX_[index]<<','<<captureFirstPersonWeaponMinimumY_[index]<<','<<captureFirstPersonWeaponMaximumX_[index]<<','<<captureFirstPersonWeaponMaximumY_[index]<<"]}";}captureOut<<"]}";
    std::ofstream cameraPolicyOut(captureDirectory_/"gameplay-camera-policy-receipt.json",std::ios::trunc);cameraPolicyOut<<std::setprecision(std::numeric_limits<float>::max_digits10);cameraPolicyOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabCameraPolicy\",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)cameraPolicyOut<<',';cameraPolicyOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"paletteOpen\":"<<unsigned(captureBuilderPaletteOpen_[index])<<",\"minimumSubjectScreenArea\":"<<captureBuilderMinimumSubjectScreenArea_[index]<<",\"actualSubjectScreenArea\":"<<captureBuilderScreenArea_[index]<<'}';}cameraPolicyOut<<"]}";
    std::ofstream diagnosticOut(captureDirectory_/"gameplay-camera-depth-diagnostic-receipt.json",std::ios::trunc);diagnosticOut<<std::setprecision(std::numeric_limits<float>::max_digits10);diagnosticOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabCameraDepthDiagnostic\",\"captures\":[";
    for(std::size_t index=0;index<Names.size();++index){if(index)diagnosticOut<<',';diagnosticOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"builderValid\":"<<unsigned(captureBuilderValid_[index])<<",\"policyValid\":"<<unsigned(captureBuilderPolicyValid_[index])<<",\"matrixExact\":"<<unsigned(captureBuilderMatrixExact_[index])<<",\"candidateLow\":"<<captureBuilderCandidateLow_[index]<<",\"screenHeight\":"<<captureBuilderScreenHeight_[index]<<",\"screenWidth\":"<<captureBuilderScreenWidth_[index]<<",\"screenArea\":"<<captureBuilderScreenArea_[index]<<",\"headroom\":"<<captureBuilderHeadroom_[index]<<",\"feetOnScreen\":"<<unsigned(captureBuilderFeetOnScreen_[index])<<",\"yaw\":"<<captureBuilderYaw_[index]<<",\"pitch\":"<<captureBuilderPitch_[index]<<",\"elevation\":"<<captureBuilderElevation_[index]<<",\"arm\":"<<captureBuilderArm_[index]<<",\"cameraPosition\":["<<captureBuilderPositionX_[index]<<','<<captureBuilderPositionY_[index]<<','<<captureBuilderPositionZ_[index]<<"],\"expectedCameraHash\":"<<captureBuilderExpectedCameraHash_[index]<<",\"actualCameraHash\":"<<captureRequestCameraHash_[index]<<",\"playerToken\":"<<capturePlayerToken_[index]<<",\"terrainToken\":"<<captureTerrainToken_[index]<<",\"roiDominantToken\":"<<captureFootRoiDominantToken_[index]<<",\"roiDominantIdentity\":["<<captureFootRoiDominantHigh_[index]<<','<<captureFootRoiDominantLow_[index]<<','<<captureFootRoiDominantGeneration_[index]<<"],\"roiDepthMin\":"<<captureFootRoiDepthMin_[index]<<",\"roiDepthMax\":"<<captureFootRoiDepthMax_[index]<<",\"roiHistogram\":"<<(captureFootRoiHistogramJson_[index].empty()?"[]":captureFootRoiHistogramJson_[index])<<"}";}diagnosticOut<<"]}";
    diagnosticOut.close();std::ofstream correctedDiagnosticOut(captureDirectory_/"gameplay-camera-depth-diagnostic-receipt.json",std::ios::trunc);correctedDiagnosticOut<<std::setprecision(std::numeric_limits<float>::max_digits10);correctedDiagnosticOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabCameraDepthDiagnostic\",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)correctedDiagnosticOut<<',';correctedDiagnosticOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"projectionMode\":\""<<CaptureProjectionName(ProjectionMode(captureProjectionMode_[index]))<<"\",\"projectionModeValue\":"<<unsigned(captureProjectionMode_[index])<<",\"verticalFovRadians\":"<<captureVerticalFovRadians_[index]<<",\"orthographicHeight\":"<<captureOrthographicHeight_[index]<<",\"godActualTriangleCoverage\":"<<captureGodActualTriangleCoverage_[index]<<",\"godVisibleWorldDescriptors\":"<<captureGodVisibleWorldDescriptors_[index]<<",\"godBuildingUnion\":"<<captureGodBuildingUnion_[index]<<",\"godMaximumBuildingPart\":"<<captureGodMaximumBuildingPart_[index]<<",\"godRoofCoverage\":"<<captureGodRoofCoverage_[index]<<",\"godLodTransitions\":"<<captureGodLodTransitions_[index]<<",\"builderValid\":"<<unsigned(captureBuilderValid_[index])<<",\"policyValid\":"<<unsigned(captureBuilderPolicyValid_[index])<<",\"matrixExact\":"<<unsigned(captureBuilderMatrixExact_[index])<<",\"candidateLow\":"<<captureBuilderCandidateLow_[index]<<",\"screenHeight\":"<<captureBuilderScreenHeight_[index]<<",\"screenWidth\":"<<captureBuilderScreenWidth_[index]<<",\"screenArea\":"<<captureBuilderScreenArea_[index]<<",\"headroom\":"<<captureBuilderHeadroom_[index]<<",\"feetOnScreen\":"<<unsigned(captureBuilderFeetOnScreen_[index])<<",\"yaw\":"<<captureBuilderYaw_[index]<<",\"pitch\":"<<captureBuilderPitch_[index]<<",\"topCenterRayElevation\":"<<captureBuilderTopCenterRayElevation_[index]<<",\"topCenterRayElevationRawBits\":"<<std::bit_cast<std::uint32_t>(captureBuilderTopCenterRayElevation_[index])<<",\"minimumTopCenterRayElevation\":"<<captureBuilderMinimumTopCenterRayElevation_[index]<<",\"minimumTopCenterRayElevationRawBits\":"<<std::bit_cast<std::uint32_t>(captureBuilderMinimumTopCenterRayElevation_[index])<<",\"rejectedHorizon\":"<<captureBuilderRejectedHorizon_[index]<<",\"elevation\":"<<captureBuilderElevation_[index]<<",\"arm\":"<<captureBuilderArm_[index]<<",\"cameraPosition\":["<<captureBuilderPositionX_[index]<<','<<captureBuilderPositionY_[index]<<','<<captureBuilderPositionZ_[index]<<"],\"expectedCameraHash\":"<<captureBuilderExpectedCameraHash_[index]<<",\"actualCameraHash\":"<<captureRequestCameraHash_[index]<<",\"playerToken\":"<<capturePlayerToken_[index]<<",\"terrainToken\":"<<captureTerrainToken_[index]<<",\"roiDominantToken\":"<<captureFootRoiDominantToken_[index]<<",\"roiDominantIdentity\":["<<captureFootRoiDominantHigh_[index]<<','<<captureFootRoiDominantLow_[index]<<','<<captureFootRoiDominantGeneration_[index]<<"],\"roiDepthMin\":"<<captureFootRoiDepthMin_[index]<<",\"roiDepthMax\":"<<captureFootRoiDepthMax_[index]<<",\"roiHistogram\":"<<(captureFootRoiHistogramJson_[index].empty()?"[]":captureFootRoiHistogramJson_[index])<<'}';}correctedDiagnosticOut<<"]}";
    std::ofstream sequenceOut(captureDirectory_/"gameplay-readback-sequence-receipt.json",std::ios::trunc);
    sequenceOut<<std::setprecision(std::numeric_limits<float>::max_digits10);
    sequenceOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabReadbackSequence\",\"captures\":[";
    for(std::size_t index=0;index<Names.size();++index){
      if(index)sequenceOut<<',';
      const auto& receipt=captureSequenceReceipts_[index];
      sequenceOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()
                 <<"\",\"token\":"<<receipt.token.value
                 <<",\"expectedPhaseCount\":"<<receipt.expectedPhaseCount
                 <<",\"jitterPolicy\":"<<unsigned(receipt.descriptor.jitterPolicy)
                 <<",\"fixedSampleIndex\":"<<receipt.descriptor.fixedSampleIndex
                 <<",\"complete\":"<<unsigned(receipt.Complete())<<",\"phases\":[";
      for(std::size_t phaseIndex=0;phaseIndex<receipt.phases.size();++phaseIndex){
        if(phaseIndex)sequenceOut<<',';
        const auto& phase=receipt.phases[phaseIndex];
        sequenceOut<<"{\"index\":"<<phase.phaseIndex
                   <<",\"sequenceToken\":"<<phase.metadata.sequence.token.value
                   <<",\"sequenceIndex\":"<<phase.metadata.sequence.index
                   <<",\"drawListRevision\":"<<phase.metadata.drawListRevision
                   <<",\"cameraHash\":"<<phase.metadata.cameraHash
                   <<",\"stateHash\":"<<phase.metadata.stateHash
                   <<",\"invariantHash\":"<<phase.metadata.sequenceInvariantHash
                   <<",\"frameSerial\":"<<phase.raster.frameSerial
                   <<",\"viewProjectionHash\":"<<phase.raster.viewProjectionHash
                   <<",\"tokenGeneration\":"<<phase.raster.tokenGeneration
                   <<",\"historyGeneration\":"<<phase.raster.temporalHistoryReadGeneration
                   <<",\"historyIndex\":"<<phase.raster.temporalHistoryReadIndex
                   <<",\"jitterIndex\":"<<phase.raster.jitterIndex
                   <<",\"jitterX\":"<<phase.raster.jitterPixels.x
                   <<",\"jitterY\":"<<phase.raster.jitterPixels.y
                   <<",\"excludingOpacity\":"<<phase.drawFingerprintExcludingOpacity
                   <<",\"includingOpacity\":"<<phase.drawFingerprintIncludingOpacity
                   <<",\"identityOrder\":"<<phase.identityOrderHash;
        WriteIntegrityJson(sequenceOut,phase.determinism.frameSerial,phase.determinism.integrity,phase.determinism.canonical,&phase.determinism.render);
        sequenceOut<<",\"gpuProvenance\":{\"atmosphere\":";
        WriteAtmosphereReadbackProvenanceJson(sequenceOut,captureAtmospherePhaseProvenance_[index][phaseIndex]);
        sequenceOut<<",\"temporalResolve\":";
        WriteAtmosphereReadbackProvenanceJson(sequenceOut,captureTemporalPhaseProvenance_[index][phaseIndex]);
        sequenceOut<<"}}";
      }
      sequenceOut<<"]}";
    }
    sequenceOut<<"]}";
    std::ofstream drawIdentityOut(captureDirectory_/"gameplay-draw-identity-diagnostic-receipt.json",std::ios::trunc);drawIdentityOut<<std::setprecision(std::numeric_limits<float>::max_digits10);drawIdentityOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabDrawIdentityDiagnostic\",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)drawIdentityOut<<',';drawIdentityOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"drawIdentities\":"<<captureDrawIdentityCount_[index]<<",\"drawTerrainInstances\":"<<captureDrawTerrainCount_[index]<<",\"drawTerrainIdentityMatches\":"<<captureDrawTerrainIdentityCount_[index]<<",\"acceptedTerrainToken\":"<<captureTerrainToken_[index]<<"}";}drawIdentityOut<<"]}";
    std::ofstream drawGroundOut(captureDirectory_/"gameplay-draw-ground-identity-diagnostic-receipt.json",std::ios::trunc);drawGroundOut<<std::setprecision(std::numeric_limits<float>::max_digits10);drawGroundOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabDrawGroundIdentityDiagnostic\",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)drawGroundOut<<',';drawGroundOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"sceneToDrawGroundIdentityMatches\":"<<captureDrawGroundIdentityCount_[index]<<",\"acceptedGpuGroundToken\":"<<captureTerrainToken_[index]<<"}";}drawGroundOut<<"]}";
    std::ofstream drawGroundLayerOut(captureDirectory_/"gameplay-draw-ground-layer-diagnostic-receipt.json",std::ios::trunc);drawGroundLayerOut<<std::setprecision(std::numeric_limits<float>::max_digits10);drawGroundLayerOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabDrawGroundLayerDiagnostic\",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)drawGroundLayerOut<<',';drawGroundLayerOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"groundOpaque\":"<<captureDrawGroundOpaqueCount_[index]<<",\"groundShadowOnly\":"<<captureDrawGroundShadowOnlyCount_[index]<<"}";}drawGroundLayerOut<<"]}";
    std::ofstream groundDecisionOut(captureDirectory_/"gameplay-ground-visibility-decision-receipt.json",std::ios::trunc);groundDecisionOut<<std::setprecision(std::numeric_limits<float>::max_digits10);groundDecisionOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabGroundVisibilityDecision\",\"captures\":[";for(std::size_t index=0;index<Names.size();++index){if(index)groundDecisionOut<<',';groundDecisionOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"meshHandle\":"<<captureGroundMeshHandle_[index]<<",\"castsShadow\":"<<captureGroundCastsShadow_[index]<<",\"alphaMode\":"<<captureGroundAlphaMode_[index]<<",\"boundsRadius\":"<<captureGroundBoundsRadius_[index]<<",\"distanceMinusRadius\":"<<captureGroundDistanceMinusRadius_[index]<<",\"frustumAccepted\":"<<captureGroundFrustumAccepted_[index]<<",\"viewDepth\":"<<captureGroundViewDepth_[index]<<",\"projectedRadius\":"<<captureGroundProjectedRadius_[index]<<",\"rendererSubmittedMeshes\":"<<captureRendererSubmittedMeshes_[index]<<",\"rendererSubmittedTerrain\":"<<captureRendererSubmittedTerrain_[index]<<"}";}groundDecisionOut<<"]}";
    if(!WriteGameplayHdrCaptureReceipt())throw std::runtime_error("Gameplay production HDR aggregate receipt failed closed");
    engine.Quit();
    return;
  }
  if (!capturePending_ && captureScenarioClock_.ReadyToStage()) {
    const bool requiresPreparation=!capturePreparedIndex_||*capturePreparedIndex_!=captureIndex_;
    // 暫停/GPU 等待不是模擬步驟；不得在此改變場景權威狀態或決定凍結快照。
    if(requiresPreparation&&!fixedSimulationStep)return;
    if (captureIndex_ == 10) {
      const auto grab = palette_->Stats();
      if (combat_->Monster().alive ||
          combat_->CurrentAttack() != Attack3D::Attack3 ||
          !combat_->InHitWindow() || !grab.grabs || !grab.dragSteps ||
          !grab.drops)
        return;
    }
    if(requiresPreparation){
    // The palette evidence locations must be selected with the same camera that
    // will render the shot.  Selecting them with the previous capture camera
    // can satisfy the world-space distance gate yet land directly in front of
    // the final third-person camera, producing a screen-filling building.
    if (captureIndex_ < 9) {
      const auto route=BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[captureIndex_]);
      rules_.SetCharacterFemale(captureIndex_ < 5);
      rules_.SetLighting(route.lighting);
      rules_.SetCamera(route.camera);
    } else {
      // 材質面板保留男性編輯案例；戰鬥展示必須明確選用目前的 Candidate7
      // 女性主角，避免從前一張男性夜景繼承可變的呈現狀態。
      rules_.SetCharacterFemale(captureIndex_ == 10);
      rules_.SetCamera(CameraMode::ThirdPerson);
      rules_.SetLighting(captureIndex_ == 9 ? LightingPhase::Day
                                            : LightingPhase::Dusk);
    }
    ResetProductionCameraCut(GameplayTemporalReseedReason3D::CaptureShot);
    UpdateCamera(engine);
    if (captureIndex_ == 9 && !captureEvidenceCollider_) {
      const auto groundPoint=[](const Camera3D& camera,Vec2 pointer)->std::optional<Vec3>{const auto ray=ScreenPointToRay(camera,{1280,720},pointer);if(std::abs(ray.direction.y)<.0001f)return std::nullopt;const float t=-ray.origin.y/ray.direction.y;if(t<0||t>200)return std::nullopt;auto point=ray.origin+ray.direction*t;point.x=std::round(point.x*.5f)*2;point.z=std::round(point.z*.5f)*2;point.y=.5f;return point;};
      // 先用正式pointer搜尋找到當前world layout的空位，再在同點注入單一隔離碰撞體驗證invalid ghost。
      constexpr float minimumCapturePaletteRayDistance=4.f;
      const auto pointer=FindGameplayClearPlacementPointer3D(
          camera_,collisions_,minimumCapturePaletteRayDistance);
      const auto point=pointer?groundPoint(camera_,*pointer):std::nullopt;
      if(!point||!palette_->PreviewAt(*point,PaletteCategory::Building))
        throw std::runtime_error("Capture palette fixture found no valid collision-free placement");
      captureValidGhost_ = palette_->Preview().position;
      palette_->Cancel();

      // Reserve the first snapped cell only while searching. This forces the
      // production pointer finder to choose a second clear grid cell, then the
      // reservation is removed before either persistent evidence check.
      const auto reservation=collisions_.CreateBox(
          {{*point,{.5f,.5f,.5f}},4,0xffffffffu,0xfeed0001,false});
      if(!reservation)
        throw std::runtime_error("Capture palette fixture could not reserve valid placement");
      std::optional<Vec3> invalidPoint;
      try{
        const auto invalidPointer=FindGameplayClearPlacementPointer3D(
            camera_,collisions_,minimumCapturePaletteRayDistance);
        invalidPoint=invalidPointer?groundPoint(camera_,*invalidPointer):std::nullopt;
      }catch(...){
        collisions_.Destroy(reservation);
        throw;
      }
      if(!collisions_.Destroy(reservation))
        throw std::runtime_error("Capture palette fixture reservation cleanup failed");
      if(!invalidPoint||
         !GameplayPaletteCaptureGhostsDistinct3D(*captureValidGhost_,*invalidPoint)||
         !palette_->PreviewAt(*invalidPoint,PaletteCategory::Building))
        throw std::runtime_error("Capture palette fixture found no distinct second valid placement");
      palette_->Cancel();

      captureEvidenceCollider_=collisions_.CreateBox(
          {{*invalidPoint,{.5f,.5f,.5f}},4,0xffffffffu,0xfeed,false});
      if(!captureEvidenceCollider_)throw std::runtime_error("Capture palette fixture could not isolate invalid placement");
      if (palette_->PreviewAt(*invalidPoint, PaletteCategory::Building))
        throw std::runtime_error("Expected invalid capture ghost");
      captureInvalidGhost_ = palette_->Preview().position;
      if(!GameplayPaletteCaptureGhostsDistinct3D(*captureValidGhost_,*captureInvalidGhost_))
        throw std::runtime_error("Capture palette valid and invalid ghosts overlapped");
      palette_->Cancel();
      rules_.SetPaletteCategory(PaletteCategory::Lights);
    }
    const bool capturePaletteOpen=captureIndex_==9;
    if(rules_.PaletteOpen()!=capturePaletteOpen){TickInput paletteToggle{};paletteToggle.togglePalette=true;if(!rules_.ApplyPalettePresentationInput(paletteToggle))throw std::runtime_error("Gameplay capture palette open/close input failed");}
      capturePreparedIndex_=captureIndex_;capturePreparedSceneSubmitted_=false;captureTemporalWarmupActive_=false;capturePresentationWarmupActive_=false;capturePoseFrozen_=false;captureTemporalWarmupRemaining_=0;captureTemporalWarmupFramesCurrent_=0;captureTemporalReseedReasonCurrent_=0;capturePresentationWarmupRemaining_=0;capturePresentationWarmupFramesCurrent_=0;captureDiagnosticAsyncWaitFrames_=0;
      if(captureIndex_>=GameplayCaptureExpectations3D.size())throw std::runtime_error("Gameplay active capture route index was out of range");
      activeCaptureRoute_={captureIndex_,BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[captureIndex_]),GameplayActiveCaptureRoutePhase3D::Prepared};
      // 跨時段保留同一組已驗證的上帝視角構圖；後續 UpdateCamera 仍會依目前角色位置、碰撞、地形與 LOD 重新驗證。
      // 凍結決策只發生於 fixed simulation 呼叫，來源是本 tick 已完成的權威 pose/foot。
      captureFrozenFemalePose_=femalePose_;captureFrozenMalePose_=malePose_;captureFrozenMonsterPose_=monsterPose_;
      captureFrozenFootPlacementHash_=authorityFootPlacementHash_;capturePoseFrozen_=true;
      // 首張正式暖機之前沒有受控 capture 前史，不得繼承最後一個即時呈現幀的
      // palette/transform；該幀取決於 Update/Render 排程，會把不同 previous pose 帶入
      // velocity/temporal history。後續鏡頭保留前一個受控 capture 建立的正式前史。
      if(captureIndex_==0){playerHistory_.Invalidate();monsterHistory_.Invalidate();}
      ResetProductionCameraCut(GameplayTemporalReseedReason3D::CaptureShot);UpdateCamera(engine);
      // 與即時模式共用發布流程；重設相機不推進動畫或進場時間。
      PublishFirstPersonPose(0.f);captureFrozenFirstPersonPose_=firstPersonPose_;return;
    }
    // 除錯層可能讓固定更新快於實際呈現；物件面板必須完成十一個可見畫面後才能開始讀回。
    if(captureIndex_==9&&paletteUiDrawnFrames_<Names.size())return;
    if(rules_.Camera()==CameraMode::ThirdPerson&&cameraPolicyFadeIdentity_&&!cameraFadeSettled_)return;
    if(rules_.Camera()==CameraMode::ThirdPerson&&!GameplayCameraSelectionSettled3D(cameraPolicyReceipt_,cameraSelectedCandidate_))return;
    // 場景切換後必須先完成一次正式 submission，再開始固定暖機。
    // 否則 Update/Render 排程次序會決定 DrawIdentity 重設落在暖機之前或期間，
    // 使相同 capture 出現不同的 HDR 起始狀態。
    if(!GameplayCaptureWarmupMayBegin3D(capturePreparedSceneSubmitted_,engine.CurrentBufferedFrameIndex3D()))return;
    if(!captureTemporalWarmupActive_){activeCaptureRoute_.phase=GameplayActiveCaptureRoutePhase3D::Warming;captureTemporalWarmupActive_=true;captureTemporalWarmupRemaining_=temporalFixtureWarmupFrames_?temporalFixtureWarmupFrames_:GameplayTemporalCaptureWarmupFrames3D;captureTemporalWarmupFramesCurrent_=0;captureTemporalReseedReasonCurrent_=GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::CaptureShot);QueueTemporalHistoryReseed(GameplayTemporalReseedReason3D::CaptureShot);return;}
    if(!GameplayTemporalCaptureReady3D(captureTemporalWarmupActive_,capturePoseFrozen_,captureTemporalWarmupRemaining_))return;
    captureTemporalWarmupActive_=false;
    captureObservedCamera_[captureIndex_]=std::uint8_t(CaptureCamera(rules_.Camera()));captureObservedLighting_[captureIndex_]=std::uint8_t(CaptureLighting(rules_.Lighting()));
    captureReferenceRemovalThreshold_[captureIndex_]=GameplayReferenceBuildingOverridePlan(0.f).removalThreshold;
    if(rules_.Camera()==CameraMode::GodView){
      const auto route=BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[captureIndex_]);
      const auto viewProjection=CreateViewProjectionMatrix(camera_,1280.f/720.f);
      const GameplayCaptureProjectionEvidence3D projectionEvidence{camera_.projection.mode,camera_.projection.verticalFieldOfView,camera_.projection.orthographicHeight,godCameraBuildCache_.playerCoverage,godCameraBuildCache_.buildingUnion,godCameraBuildCache_.maximumBuildingPart,godCameraBuildCache_.roofCoverage,godCameraBuildCache_.visibleWorldDescriptors,godCameraBuildCache_.lodTransitions};
      const std::uint64_t candidateHash=CaptureCameraHash(camera_);
      captureBuilderValid_[captureIndex_]=godCameraBuildCacheValid_&&godCameraBuildCache_.valid?1u:0u;
      captureBuilderPolicyValid_[captureIndex_]=ValidateGameplayCaptureProductionRoute3D(route)&&projectionEvidence.Complete(GameplayCaptureCamera3D::God)?1u:0u;
      captureBuilderMatrixExact_[captureIndex_]=ValidateGameplayCaptureProductionProjectionMatrix3D(camera_,route,1280.f/720.f,viewProjection)?1u:0u;
      captureBuilderCandidateLow_[captureIndex_]=candidateHash;
      captureBuilderScreenHeight_[captureIndex_]=0;captureBuilderScreenWidth_[captureIndex_]=0;captureBuilderScreenArea_[captureIndex_]=godCameraBuildCache_.playerCoverage;captureBuilderMinimumSubjectScreenArea_[captureIndex_]=0;captureBuilderHeadroom_[captureIndex_]=0;captureBuilderFeetOnScreen_[captureIndex_]=1;
      captureBuilderYaw_[captureIndex_]=godCameraBuildCache_.azimuth;captureBuilderPitch_[captureIndex_]=godCameraBuildCache_.elevation;captureBuilderElevation_[captureIndex_]=godCameraBuildCache_.elevation;captureBuilderArm_[captureIndex_]=godCameraBuildCache_.distance;
      captureBuilderExpectedCameraHash_[captureIndex_]=candidateHash;
    }else{
      captureBuilderValid_[captureIndex_]=cameraBuilderReceipt_.valid?1u:0u;captureBuilderPolicyValid_[captureIndex_]=cameraPolicyReceipt_.valid?1u:0u;captureBuilderMatrixExact_[captureIndex_]=cameraBuilderMatrixExact_?1u:0u;captureBuilderCandidateLow_[captureIndex_]=cameraPolicyReceipt_.candidateId.low;captureBuilderScreenHeight_[captureIndex_]=cameraSelectedCandidate_.subjectScreenHeight;captureBuilderScreenWidth_[captureIndex_]=cameraSelectedCandidate_.subjectScreenWidth;captureBuilderScreenArea_[captureIndex_]=cameraSelectedCandidate_.subjectScreenArea;captureBuilderMinimumSubjectScreenArea_[captureIndex_]=cameraBuilderReceipt_.minimumSubjectScreenArea;captureBuilderHeadroom_[captureIndex_]=cameraSelectedCandidate_.headroom;captureBuilderFeetOnScreen_[captureIndex_]=cameraSelectedCandidate_.feetOnScreen?1u:0u;captureBuilderYaw_[captureIndex_]=cameraPolicyReceipt_.yawOffsetRadians;captureBuilderPitch_[captureIndex_]=cameraPolicyReceipt_.pitchRadians;captureBuilderElevation_[captureIndex_]=cameraPolicyReceipt_.verticalElevation;captureBuilderArm_[captureIndex_]=cameraPolicyReceipt_.arm;captureBuilderExpectedCameraHash_[captureIndex_]=CaptureCameraHash(cameraBuilderExpected_);
    }
    captureBuilderPaletteOpen_[captureIndex_]=rules_.PaletteOpen()?1u:0u;captureBuilderMinimumSubjectScreenArea_[captureIndex_]=rules_.PaletteOpen()?.08f:.05f;
    captureBuilderPositionX_[captureIndex_]=camera_.position.x;captureBuilderPositionY_[captureIndex_]=camera_.position.y;captureBuilderPositionZ_[captureIndex_]=camera_.position.z;
    if(rules_.Camera()==CameraMode::ThirdPerson){
      const auto horizon=::GameplayLab::EvaluateGameplayThirdPersonGroundHorizon3D(
          camera_.projection,cameraPolicyReceipt_.pitchRadians,
          cameraBuilderReceipt_.minimumTopCenterRayElevation);
      if(!horizon.accepted)throw std::runtime_error("Gameplay third-person camera violated the analytic ground-horizon gate");
      captureBuilderTopCenterRayElevation_[captureIndex_]=horizon.topCenterRayElevation;
      captureBuilderMinimumTopCenterRayElevation_[captureIndex_]=horizon.minimumTopCenterRayElevation;
      captureBuilderRejectedHorizon_[captureIndex_]=cameraBuilderReceipt_.rejectedHorizon;
    }else{
      captureBuilderTopCenterRayElevation_[captureIndex_]=0.f;
      captureBuilderMinimumTopCenterRayElevation_[captureIndex_]=0.f;
      captureBuilderRejectedHorizon_[captureIndex_]=0;
    }
    const bool thirdCapture=(captureIndex_<9&&captureIndex_%3==1)||captureIndex_>=9;
    captureSequencePhaseCount_=thirdCapture?3u:2u;
    if(!capturePoseFrozen_)throw std::runtime_error("Gameplay temporal warm-up did not freeze the canonical capture pose");
    captureTemporalWarmupFrames_[captureIndex_]=captureTemporalWarmupFramesCurrent_;captureTemporalReseedReasonMask_[captureIndex_]=captureTemporalReseedReasonCurrent_;
    const auto requiredEvidenceWarmup=temporalFixtureWarmupFrames_?temporalFixtureWarmupFrames_:GameplayTemporalCaptureWarmupFrames3D;
    if(captureTemporalWarmupFrames_[captureIndex_]<requiredEvidenceWarmup||!(captureTemporalReseedReasonMask_[captureIndex_]&GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::CaptureShot)))throw std::runtime_error("Gameplay temporal capture warm-up evidence was incomplete");
    const std::uint64_t frozenCameraHash=CaptureCameraHash(camera_);std::uint64_t animationState=CaptureStateSeed;MixCaptureHash(animationState,HashPose(captureFrozenFemalePose_));MixCaptureHash(animationState,HashPose(captureFrozenMalePose_));MixCaptureHash(animationState,HashPose(captureFrozenMonsterPose_));std::uint64_t physicsState=CaptureStateSeed;MixCaptureHash(physicsState,captureAuthoritativePhysicsSteps_);MixCaptureHash(physicsState,captureFrozenFootPlacementHash_);
    if(rules_.Camera()==CameraMode::FirstPerson)MixCaptureHash(animationState,captureFrozenFirstPersonPose_.hash);
    GameplayCanonicalCaptureStateInput3D canonicalInput{};canonicalInput.captureIndex=captureIndex_;canonicalInput.entities.assign(rules_.Entities().begin(),rules_.Entities().end());canonicalInput.controllerPosition=controller_.Position();canonicalInput.yaw=yaw_;canonicalInput.pitch=pitch_;canonicalInput.camera=rules_.Camera();canonicalInput.lighting=rules_.Lighting();canonicalInput.paletteCategory=rules_.Category();canonicalInput.female=rules_.Female();canonicalInput.paletteOpen=rules_.PaletteOpen();canonicalInput.grabbedEntity=rules_.Grabbed().value_or(0);canonicalInput.cameraHash=frozenCameraHash;canonicalInput.animationHash=animationState;canonicalInput.physicsHash=physicsState;captureCanonicalState_[captureIndex_]=BuildGameplayCanonicalCaptureState3D(canonicalInput);if(!captureCanonicalState_[captureIndex_].valid)throw std::runtime_error("Gameplay canonical capture state contained a non-finite or invalid entity");for(auto& diagnostic:captureCanonicalState_[captureIndex_].entityDiagnostics){diagnostic.initialSeed=std::ranges::find(initialSeedEntityIds_,diagnostic.stableId)!=initialSeedEntityIds_.end();if(const auto* body=static_cast<const PhysicsWorld3D&>(physics_).GetBody(palette_->Body(diagnostic.stableId))){diagnostic.bodyAvailable=true;diagnostic.bodyPosition=body->settings.transform.position;diagnostic.bodyVelocity=body->velocity;}if(const auto* scene=static_cast<const SceneECSRegistry3D&>(sceneEcs_).Transform(palette_->SceneEntity(diagnostic.stableId))){diagnostic.sceneAvailable=true;diagnostic.scenePosition=scene->value.position;}}const std::uint64_t frozenStateHash=captureCanonicalState_[captureIndex_].canonical;
    std::uint64_t sequenceInvariantHash=14695981039346656037ull;MixCaptureHash(sequenceInvariantHash,frozenCameraHash);MixCaptureHash(sequenceInvariantHash,frozenStateHash);MixCaptureHash(sequenceInvariantHash,HashPose(captureFrozenFemalePose_));MixCaptureHash(sequenceInvariantHash,HashPose(captureFrozenMalePose_));MixCaptureHash(sequenceInvariantHash,HashPose(captureFrozenMonsterPose_));MixCaptureHash(sequenceInvariantHash,showcaseProductionReceipt_.manifestFingerprint);if(rules_.Camera()==CameraMode::FirstPerson)MixCaptureHash(sequenceInvariantHash,captureFrozenFirstPersonPose_.hash);if(!sequenceInvariantHash)sequenceInvariantHash=1;
    // 物件識別會與呈現色彩共用每幀的動態／透明幾何。固定正式序列由槽位 0
    // 開始，確保 raw/effective/HDR 三個 phase 在跨程序重跑時仍落在相同的
    // 0/1/2 upload slice；等待期間只呈現已凍結姿態，不建立半套 sequence。
    if(engine.CurrentBufferedFrameIndex3D()!=0)return;
    activeCaptureRoute_.phase=GameplayActiveCaptureRoutePhase3D::Readback;
    // 第一階段只記錄可比性契約，不載入或重播舊姿態；正式讀回與品質 gate 完全保留。
    {
      const auto destination=captureDirectory_/(std::filesystem::path(Names[captureIndex_]).stem().wstring()+L".compare-v1.json");
      std::ofstream compare(destination,std::ios::trunc);
      if(!compare)throw std::runtime_error("Comparison contract open failed");
      const auto bits=[](float value){return std::bit_cast<std::uint32_t>(value);};
      const auto vectorBits=[&](Vec3 value){compare<<bits(value.x)<<','<<bits(value.y)<<','<<bits(value.z);};
      const bool cameraClear=CompareCaptureCollisionClear(collisions_,camera_.position,controller_.Center(),controller_.Extents(),
          !showcaseTerrain_.Contains(camera_.position.x,camera_.position.z)||camera_.position.y>showcaseTerrain_.SampleHeight(camera_.position.x,camera_.position.z)+.1f);
      compare<<"{\"schemaVersion\":1,\"replaySupported\":false,\"shot\":\""<<std::filesystem::path(Names[captureIndex_]).string()<<"\",\"collisionClear\":"<<(cameraClear?"true":"false")
             <<",\"cameraRawBits\":[";vectorBits(camera_.position);compare<<',';vectorBits(camera_.target);compare<<',';vectorBits(camera_.up);
      compare<<','<<unsigned(camera_.projection.mode)<<','<<bits(camera_.projection.verticalFieldOfView)<<','<<bits(camera_.projection.orthographicHeight)<<','<<bits(camera_.projection.nearPlane)<<','<<bits(camera_.projection.farPlane)<<"]";
      const auto matrixBits=[&](const Matrix4& matrix){bool first=true;for(const auto& row:matrix.values)for(float value:row){if(!first)compare<<',';first=false;compare<<bits(value);}};
      compare<<",\"actorRawBits\":[";matrixBits(GameplayPlayerWorld3D(candidate7Active_,rules_.Female(),candidate7PresentationScale_,controller_.Position(),yaw_));compare<<','<<rules_.Female()<<"]";
      compare<<",\"clipPhaseRaw\":["<<unsigned(femaleAnimation_.State())<<','<<bits(femaleAnimation_.NormalizedPhase())<<','<<unsigned(maleAnimation_.State())<<','<<bits(maleAnimation_.NormalizedPhase())<<','<<unsigned(monsterAnimation_.State())<<','<<bits(monsterAnimation_.NormalizedPhase())<<"]";
      compare<<",\"frozenPoseHashes\":[\""<<HashPose(captureFrozenFemalePose_)<<"\",\""<<HashPose(captureFrozenMalePose_)<<"\",\""<<HashPose(captureFrozenMonsterPose_)<<"\"]"
             <<",\"footPlacementHash\":\""<<captureFrozenFootPlacementHash_<<"\",\"controllerHash\":\""<<controller_.StateHash()<<"\"";
      compare<<",\"companionRawBits\":[";{const auto position=GameplayShowcaseMonsterPresentationPosition3D(combat_->Monster().position,yaw_,combat_->Monster().alive);const bool godCaptureStage=!captureDirectory_.empty()&&rules_.Camera()==CameraMode::GodView;const auto transform=GameplayShowcaseMonsterTransform3D(position,controller_.Position(),yaw_,godCaptureStage);matrixBits(CreateTransformMatrix(transform.position,transform.rotation,transform.scale));}compare<<','<<combat_->Monster().alive<<"]";
      compare<<",\"viewmodelHash\":\""<<(rules_.Camera()==CameraMode::FirstPerson?captureFrozenFirstPersonPose_.hash:0)<<"\",\"exposureRawBits\":"<<bits(showcaseEnvironment_.EvaluateLighting(ShowcaseLightingProfile3D(unsigned(rules_.Lighting()))).exposureEv)<<"}";
      compare.flush();if(!compare)throw std::runtime_error("Comparison contract write failed");
    }
    if(gameplayHdrCaptureStates_[captureIndex_]!=GameplayHdrCaptureState3D::Empty){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay HDR capture state was not empty before arm");}
    const std::uint64_t hdrPhaseMask=captureRawHdrDiagnostic_?((1ull<<0)|(1ull<<1)):(1ull<<1);
    const auto sequence=engine.BeginReadbackCaptureSequence3D(ReadbackCaptureSequenceDescriptor3D{captureSequencePhaseCount_,ReadbackCaptureSequenceJitterPolicy3D::FixedSample,0,hdrPhaseMask});
    if(!sequence){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay readback capture sequence begin failed closed");}
    captureSequenceToken_=*sequence;
    if(!captureScenarioClock_.BeginReadback(captureIndex_)){captureScenarioClock_.FailReadback(captureIndex_);FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay capture scenario readback begin was out of order");}
    if(!captureScenePhaseLedger_.Begin(captureSequenceToken_,captureIndex_,captureSequencePhaseCount_)){FailGameplayHdrCapture(engine,captureIndex_);throw std::runtime_error("Gameplay Scene phase ledger begin failed closed");}
    gameplayHdrCaptureStates_[captureIndex_]=GameplayHdrCaptureState3D::Armed;gameplayHdrCaptureReceipts_[captureIndex_]={};gameplayHdrCaptureReceipts_[captureIndex_].state=GameplayHdrCaptureState3D::Armed;
    capturePairMetadata_={0,frozenCameraHash,frozenStateHash,ReadbackEmptyReason3D::None,{captureSequenceToken_,0},sequenceInvariantHash};
    captureRawPhase_=true;captureReferencePhase_=false;capturePairFrozen_=true;capturePending_=false;capturePhaseRequestPending_=true;
  }
}
void GameplayLabGame::OnRender(Engine& engine,float){
  FinishProjectionFailure(engine);
  const auto integrityMode=capturePairFrozen_?DeterminismIntegrityMode3D::SealedCapture:DeterminismIntegrityMode3D::Runtime;if(!engine.Draw().SetDeterminismIntegrityMode3D(integrityMode))throw std::runtime_error("Gameplay typed determinism integrity mode was rejected");
  if(playerHistory_.HasPendingSubmission())playerHistory_.ResolveSubmitted(engine);
  if(monsterHistory_.HasPendingSubmission())monsterHistory_.ResolveSubmitted(engine);
  BuildWorld(engine);
  const bool frozenBeforeCaptureRequest=GameplayUseFrozenPose3D(capturePoseFrozen_,temporalFixtureWarmupFrames_!=0,capturePairFrozen_,captureTemporalWarmupActive_,captureScenarioClock_.ReadyToStage());
  RequestCapturePhase(engine);
  // Render 只選擇 fixed simulation 已發布的 authority/frozen snapshot；不得回寫或重新求值。
  const bool useFrozenPose=GameplayUseFrozenPose3D(capturePoseFrozen_,temporalFixtureWarmupFrames_!=0,capturePairFrozen_,captureTemporalWarmupActive_,captureScenarioClock_.ReadyToStage());
  const auto presentationPoses=SelectGameplayPresentationPoses3D(useFrozenPose,femalePose_,malePose_,monsterPose_,captureFrozenFemalePose_,captureFrozenMalePose_,captureFrozenMonsterPose_);
  const AnimationPose3D& presentationFemalePose=*presentationPoses.female;
  const AnimationPose3D& presentationMalePose=*presentationPoses.male;
  const AnimationPose3D& presentationMonsterPose=*presentationPoses.monster;
  const Matrix4 playerWorld=GameplayPlayerWorld3D(candidate7Active_,rules_.Female(),candidate7PresentationScale_,controller_.Position(),yaw_);
  const AnimationPose3D& activePose=rules_.Female()?presentationFemalePose:presentationMalePose;
  const SkinnedModel3D& activeModel=rules_.Female()?femaleModel_:maleModel_;
  const HumanoidSkeletonMap3D& activeRig=rules_.Female()?femaleAnimation_.Rig():maleAnimation_.Rig();
  if(rules_.Camera()==CameraMode::GodView&&!GameplayGodPoseAuthorityPass3D(activeCaptureRoute_.Active(),captureTemporalWarmupActive_,capturePairFrozen_,captureScenarioClock_.ReadyToStage(),capturePoseFrozen_,godCameraAuthorityReceipt_.valid,godCameraAuthorityReceipt_.layout.valid,godCameraAuthorityReceipt_.subjectPoseHash,HashPose(activePose))){
    // 固定檔名僅寫 capture 目錄；診斷失敗仍保留原本 fail-closed，不補算或竄改權威。
    if(!captureDirectory_.empty()){
      std::ofstream diagnostic(captureDirectory_/"god-pose-authority-mismatch.json",std::ios::binary|std::ios::trunc);
      diagnostic<<"{\"receiptSubjectHash\":\""<<godCameraAuthorityReceipt_.subjectPoseHash<<"\",\"activePoseHash\":\""<<HashPose(activePose)
        <<"\",\"liveFemaleHash\":\""<<HashPose(femalePose_)<<"\",\"liveMaleHash\":\""<<HashPose(malePose_)
        <<"\",\"frozenFemaleHash\":\""<<HashPose(captureFrozenFemalePose_)<<"\",\"frozenMaleHash\":\""<<HashPose(captureFrozenMalePose_)
        <<"\",\"receiptCameraHash\":\""<<godCameraAuthorityReceipt_.cameraHash<<"\",\"renderCameraHash\":\""<<CaptureCameraHash(camera_)
        <<"\",\"receiptValid\":"<<godCameraAuthorityReceipt_.valid<<",\"layoutValid\":"<<godCameraAuthorityReceipt_.layout.valid
        <<",\"female\":"<<rules_.Female()<<",\"tick\":"<<rules_.GetStats().ticks<<",\"captureIndex\":"<<captureIndex_
        <<",\"activeRoute\":"<<activeCaptureRoute_.Active()<<",\"poseFrozen\":"<<capturePoseFrozen_<<",\"fixtureWarmupFrames\":"<<temporalFixtureWarmupFrames_
        <<",\"pairFrozen\":"<<capturePairFrozen_<<",\"warmupActive\":"<<captureTemporalWarmupActive_
        <<",\"readyToStage\":"<<captureScenarioClock_.ReadyToStage()<<",\"advancesSimulation\":"<<captureScenarioClock_.AdvancesSimulation()
        <<",\"useFrozenPose\":"<<useFrozenPose<<",\"frozenBeforeCaptureRequest\":"<<frozenBeforeCaptureRequest<<",\"phaseRequestPending\":"<<capturePhaseRequestPending_
        <<",\"presentationRequestPending\":"<<capturePresentationRequestPending_<<",\"capturePending\":"<<capturePending_<<"}";
      diagnostic.flush();
    }
    throw std::runtime_error("Gameplay God camera subject pose did not match render authority pose");
  }
  DrawList& draw=engine.Draw();
  const auto bodyMesh=rules_.Female()?femaleMesh_.handle:maleMesh_.handle;
  const auto firstPersonArmsMesh=rules_.Female()?femaleFirstPersonArmsMesh_.handle:maleFirstPersonArmsMesh_.handle;
  const auto weaponMesh=rules_.Female()?femaleWeaponMesh_.handle:maleWeaponMesh_.handle;
  const auto& bodySubmeshes=rules_.Female()?femaleSubmeshes_:maleSubmeshes_;
  const auto& firstPersonArmsSubmeshes=rules_.Female()?femaleFirstPersonArmsSubmeshes_:maleFirstPersonArmsSubmeshes_;
  const auto& bodyMaterials=rules_.Female()?(candidate7Active_?candidate7Materials_[0]:femaleMaterials_):maleMaterials_;
  const auto& weaponSubmeshes=rules_.Female()?femaleWeaponSubmeshes_:maleWeaponSubmeshes_;
  const auto& weaponMaterials=rules_.Female()?femaleWeaponMaterials_:maleWeaponMaterials_;
  Matrix4 weaponWorld=playerWorld,armsWorld=playerWorld;
  const bool firstPerson=rules_.Camera()==CameraMode::FirstPerson;
  const auto& naturalPose=useFrozenPose?captureFrozenFirstPersonPose_:firstPersonPose_;
  if(firstPerson&&(!naturalPose.active||!naturalPose.output.valid||naturalPose.female!=rules_.Female()||
     naturalPose.sourcePoseHash!=HashPose(activePose)||naturalPose.cameraHash!=CaptureCameraHash(camera_)))
    throw std::runtime_error("First-person presentation snapshot did not match selected source/camera");
  const auto& renderPose=firstPerson?naturalPose.output.pose:activePose;
  const auto palette=AddSkinPaletteFromPose(draw,activeModel,renderPose,skinning_);
  if(rules_.Camera()==CameraMode::FirstPerson){
    Vec3 forward=Normalize(camera_.target-camera_.position),right=Normalize(Cross(camera_.up,forward)),up=Normalize(Cross(forward,right));
    if(Length(forward)<.001f||Length(right)<.001f||Length(up)<.001f)throw std::runtime_error("First-person weapon camera basis failed");
    Matrix4 cameraWorld=Matrix4::Identity();
    cameraWorld[0][0]=right.x;cameraWorld[1][0]=right.y;cameraWorld[2][0]=right.z;
    cameraWorld[0][1]=up.x;cameraWorld[1][1]=up.y;cameraWorld[2][1]=up.z;
    cameraWorld[0][2]=forward.x;cameraWorld[1][2]=forward.y;cameraWorld[2][2]=forward.z;
    cameraWorld[0][3]=camera_.position.x;cameraWorld[1][3]=camera_.position.y;cameraWorld[2][3]=camera_.position.z;
    // 固定步已完成肩部定位與手臂 IK；雙臂與武器共用同一姿勢及公尺尺度。
    const auto viewmodelLayout=GameplayFirstPersonViewmodelLayout(rules_.Female()?GameplayViewmodelSex3D::Female:GameplayViewmodelSex3D::Male);
    weaponWorld=naturalPose.output.modelToWorld;armsWorld=weaponWorld;
    // 只在相機附近加入短半徑補光，避免黃昏與夜間的雙臂失去輪廓。
    draw.AddPointLight({TransformPoint(cameraWorld,{.28f,.10f,.58f}),1.35f,{1.f,.88f,.72f,1.f},viewmodelLayout.keyIntensity});
    draw.AddPointLight({TransformPoint(cameraWorld,{-.22f,.02f,.76f}),1.10f,{.58f,.72f,1.f,1.f},viewmodelLayout.fillIntensity});
  }
  std::array<SkinnedRenderablePart3D,6> playerParts{};
  std::size_t playerPartCount=0;
  if(rules_.Camera()!=CameraMode::FirstPerson)
    playerParts[playerPartCount++]={bodyMesh,playerWorld,Matrix4::Identity(),std::uint32_t(activeModel.bindMesh.indices.size()),bodySubmeshes,bodyMaterials,{1}};
  else {
    // 第一人稱提交由正式角色資產導出的雙臂 mesh；骨架與 palette 仍沿用同一角色姿勢。
    if(!firstPersonArmsMesh||firstPersonArmsSubmeshes.empty())throw std::runtime_error("First-person arms resource is unavailable");
    playerParts[playerPartCount++]={firstPersonArmsMesh,armsWorld,Matrix4::Identity(),std::uint32_t(rules_.Female()?femaleFirstPersonArmsMesh_.indexCount:maleFirstPersonArmsMesh_.indexCount),firstPersonArmsSubmeshes,bodyMaterials,{3}};
  }
  playerParts[playerPartCount++]={weaponMesh,weaponWorld,Matrix4::Identity(),std::uint32_t(rules_.Female()?femaleWeaponMesh_.indexCount:maleWeaponMesh_.indexCount),weaponSubmeshes,weaponMaterials,{2}};
  if(candidate7Active_&&rules_.Female()&&!firstPerson)for(std::size_t i=0;i<3;++i)
    playerParts[playerPartCount++]={candidate7AttachmentMeshes_[i].handle,playerWorld,Matrix4::Identity(),std::uint32_t(candidate7AttachmentModels_[i].bindMesh.indices.size()),candidate7AttachmentSubmeshes_[i],candidate7Materials_[i+1],{std::uint64_t(4+i)}};
  firstPersonBodyHidden_=firstPersonBodyHidden_&&(!firstPerson||playerPartCount==2);
  const std::size_t playerDrawBegin=draw.skinnedMeshInstances.size();
  if(palette&&playerHistory_.Submit(draw,palette,std::span<const SkinnedRenderablePart3D>{playerParts.data(),playerPartCount},false,&skinnedGroupStats_)){
    for(std::size_t index=playerDrawBegin;index<draw.skinnedMeshInstances.size();++index){auto& instance=draw.skinnedMeshInstances[index];instance.identity=firstPerson&&instance.mesh==firstPersonArmsMesh?GameplayCaptureFirstPersonArmsIdentity3D:instance.mesh==weaponMesh?GameplayCaptureWeaponIdentity3D:GameplayCapturePlayerIdentity3D;}
    ++weaponPbrDraws_;
    if(rules_.Camera()!=CameraMode::FirstPerson){if(rules_.Female())++femaleGpuDraws_;else ++maleGpuDraws_;}
  }
  Matrix4 socketWorld{},bodySocketWorld{};
  if(!CharacterSocketTransform3D(activeModel.skeleton,activeRig,HumanoidJoint3D::RightHand,renderPose,weaponWorld,socketWorld)||
     (!firstPerson&&!CharacterSocketTransform3D(activeModel.skeleton,activeRig,HumanoidJoint3D::RightHand,activePose,playerWorld,bodySocketWorld)))
    throw std::runtime_error("Gameplay semantic weapon socket failed");
  const Vec3 handAnchor=TransformPoint(socketWorld,{});
  Vec3 weaponTip{};
  if(!ResolveGameplaySwordTipWorld3D(rules_.Female()?femaleSwordGeometry_:maleSwordGeometry_,socketWorld,weaponTip))
    throw std::runtime_error("Gameplay rendered sword tip contract is unavailable");
  const float thirdPersonAnchorError=firstPerson?0.f:Length(handAnchor-TransformPoint(bodySocketWorld,{}));
  weaponAttachmentPassed_=std::isfinite(handAnchor.x)&&std::isfinite(handAnchor.y)&&std::isfinite(handAnchor.z)&&thirdPersonAnchorError<=.001f;
  if(GameplayCaptureProjectionRecordingActive3D(capturePairFrozen_,captureIndex_,Names.size())){
    const auto captureRoute=BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[captureIndex_]);
    const bool projectionParametersValid=captureRoute.projection==ProjectionMode::Orthographic?(std::isfinite(camera_.projection.orthographicHeight)&&camera_.projection.orthographicHeight>=4.f&&camera_.projection.orthographicHeight<=180.f):(std::isfinite(camera_.projection.verticalFieldOfView)&&camera_.projection.verticalFieldOfView>.1f&&camera_.projection.verticalFieldOfView<3.f&&camera_.projection.orthographicHeight==0.f);
    const auto productionViewProjection=CreateViewProjectionMatrix(camera_,1280.f/720.f);
    if(camera_.projection.mode!=captureRoute.projection||!projectionParametersValid||!ValidateGameplayCaptureProductionProjectionMatrix3D(camera_,captureRoute,1280.f/720.f,productionViewProjection))throw std::runtime_error(std::format("Gameplay capture projection provenance invalid: index={} expected={} mode={} fov={} ortho={}",captureIndex_,unsigned(captureRoute.projection),unsigned(camera_.projection.mode),camera_.projection.verticalFieldOfView,camera_.projection.orthographicHeight));
    captureProjectionMode_[captureIndex_]=std::uint8_t(camera_.projection.mode);captureVerticalFovRadians_[captureIndex_]=camera_.projection.verticalFieldOfView;captureOrthographicHeight_[captureIndex_]=camera_.projection.orthographicHeight;
    if(rules_.Camera()==CameraMode::GodView){captureGodActualTriangleCoverage_[captureIndex_]=godCameraBuildCache_.playerCoverage;captureGodVisibleWorldDescriptors_[captureIndex_]=godCameraBuildCache_.visibleWorldDescriptors;captureGodBuildingUnion_[captureIndex_]=godCameraBuildCache_.buildingUnion;captureGodMaximumBuildingPart_[captureIndex_]=godCameraBuildCache_.maximumBuildingPart;captureGodRoofCoverage_[captureIndex_]=godCameraBuildCache_.roofCoverage;captureGodRiverCoverage_[captureIndex_]=godCameraBuildCache_.riverCoverage;captureGodVisibleBuildingComponents_[captureIndex_]=godCameraBuildCache_.visibleBuildingComponents;captureGodBuildingEdgeMask_[captureIndex_]=godCameraBuildCache_.buildingEdgeMask;captureGodLodTransitions_[captureIndex_]=std::uint32_t(std::ranges::count_if(captureSubmittedLodRows_[captureIndex_],&ShowcaseLodRenderRow3D::transitioning));}
    const GameplayCaptureProjectionEvidence3D projectionEvidence{camera_.projection.mode,camera_.projection.verticalFieldOfView,camera_.projection.orthographicHeight,captureGodActualTriangleCoverage_[captureIndex_],captureGodBuildingUnion_[captureIndex_],captureGodMaximumBuildingPart_[captureIndex_],captureGodRoofCoverage_[captureIndex_],captureGodVisibleWorldDescriptors_[captureIndex_],captureGodLodTransitions_[captureIndex_]};
    if(!projectionEvidence.Complete(CaptureCamera(rules_.Camera())))throw std::runtime_error("Gameplay capture projection evidence incomplete");
    captureBodyMaterialSlots_[captureIndex_]=rules_.Camera()==CameraMode::FirstPerson?std::uint32_t(firstPersonArmsSubmeshes.size()):std::uint32_t(bodyMaterials.size());
    captureWeaponMaterialRanges_[captureIndex_]=std::uint32_t(weaponSubmeshes.size());
    captureWeaponAnchorErrors_[captureIndex_]=thirdPersonAnchorError;
    // 發布證據只能由配對 GPU object-ID readback 寫入；CPU 覆蓋率僅供相機策略即時判斷。
    // Camera policy只是一項建議；paired effective GPU mask完成前不得宣稱已套用或已降低遮擋。
    capturePolicyFadeStableHigh_[captureIndex_]=cameraPolicyFadeIdentity_.id.high;
    capturePolicyFadeStableLow_[captureIndex_]=cameraPolicyFadeIdentity_.id.low;
    capturePolicyFadeGeneration_[captureIndex_]=cameraPolicyFadeIdentity_.generation;
    capturePolicyFadeOpacity_[captureIndex_]=cameraFadeAlpha_;
    const bool opacityApplied=!captureRawPhase_&&appliedOcclusionIdentity_&&appliedOcclusionOpacity_<1.f&&appliedOcclusionUpdate_;
    captureFadedStableHigh_[captureIndex_]=opacityApplied?appliedOcclusionIdentity_.id.high:0;
    captureFadedStableLow_[captureIndex_]=opacityApplied?appliedOcclusionIdentity_.id.low:0;
    captureFadedGeneration_[captureIndex_]=opacityApplied?appliedOcclusionIdentity_.generation:0;
    captureOpacityMatchedStatic_[captureIndex_]=opacityApplied?std::uint32_t(appliedOcclusionUpdate_.matchedStatic):0;
    captureOpacityMatchedMultipart_[captureIndex_]=opacityApplied?std::uint32_t(appliedOcclusionUpdate_.matchedMultipart):0;
    captureOpacityMissing_[captureIndex_]=opacityApplied&&appliedOcclusionUpdate_.missing?1u:0u;
    captureOpacityGenerationConflict_[captureIndex_]=opacityApplied&&appliedOcclusionUpdate_.generationConflicts?1u:0u;
    // 新配對的 raw frame 才清空狀態；reference frame 不得抹掉已完成的 effective GPU readback 證據。
    if(captureRawPhase_){captureEffectiveObjectIdSupported_[captureIndex_]=0;captureOcclusionAdjusted_[captureIndex_]=0;}
    const Vec3 feet=controller_.Position();
    const float groundHeight=showcaseTerrain_.SampleHeight(feet.x,feet.z);
    captureFeetGroundError_[captureIndex_]=std::abs(feet.y-groundHeight);
    const auto groundHit=environmentCollisions_?environmentCollisions_->Raycast(MakeRay3D(feet+Vec3{0,.08f,0},{0,-1,0}),.20f):Aevrynd::EnvironmentCollisionQueryHit3D{};
    captureGroundStableLow_[captureIndex_]=groundHit?groundHit.object.id.low:0;
    captureFeetContactPassed_[captureIndex_]=controller_.Grounded()&&captureFeetGroundError_[captureIndex_]<=.03f&&captureGroundStableLow_[captureIndex_]==1?1u:0u;
    if(firstPerson){
      SkinningWorkspace3D armsEvidenceWorkspace;Mesh3D posedArmsEvidence;
      const auto& armsEvidenceModel=rules_.Female()?femaleFirstPersonArmsModel_:maleFirstPersonArmsModel_;
      if(!SkinModelInto(armsEvidenceModel,renderPose,armsEvidenceWorkspace,posedArmsEvidence))throw std::runtime_error("First-person arms projection skinning failed");
      const auto projection=EvaluateGameplayFirstPersonViewmodelProjection3D(camera_,socketWorld,GameplayFirstPersonViewmodelLayout(rules_.Female()?GameplayViewmodelSex3D::Female:GameplayViewmodelSex3D::Male),rules_.Female()?femaleWeaponProjection_:maleWeaponProjection_,posedArmsEvidence.vertices,armsWorld);
      captureWeaponScreenCoverage_[captureIndex_]=projection.weaponCoverage;captureWeaponMinimumDepth_[captureIndex_]=projection.minimumDepth;
      captureWeaponCenterExcluded_[captureIndex_]=projection.centerExcluded;captureWeaponNearClipClear_[captureIndex_]=projection.nearClipClear;captureWeaponHudOverlap_[captureIndex_]=!projection.hudClear;
      captureWeaponGripX_[captureIndex_]=projection.gripX;captureWeaponGripY_[captureIndex_]=projection.gripY;captureWeaponBladeY_[captureIndex_]=projection.bladeY;
      captureWeaponGripEdgeAnchored_[captureIndex_]=projection.gripEdgeAnchored;captureWeaponBladeAboveGrip_[captureIndex_]=projection.bladeAboveGrip;
      if(!projection.Complete())PreserveProjectionFailure(engine,std::format("First-person viewmodel projection failed: coverage={} grip=({}, {}) blade={} weapon=({}, {})-({}, {}) arms=({}, {})-({}, {})",projection.weaponCoverage,projection.gripX,projection.gripY,projection.bladeY,projection.weaponMinimumX,projection.weaponMinimumY,projection.weaponMaximumX,projection.weaponMaximumY,projection.armsMinimumX,projection.armsMinimumY,projection.armsMaximumX,projection.armsMaximumY));
    }
  }
  if(combatSpawned_){
    const auto monsterPalette=AddSkinPaletteFromPose(draw,maleModel_,presentationMonsterPose,skinning_);
    const Vec3 monsterPresentation=GameplayShowcaseMonsterPresentationPosition3D(
        combat_->Monster().position,yaw_,combat_->Monster().alive);
    const bool godCaptureStage=!captureDirectory_.empty()&&rules_.Camera()==CameraMode::GodView;
    const auto monsterTransform=GameplayShowcaseMonsterTransform3D(
        monsterPresentation,controller_.Position(),yaw_,godCaptureStage);
    const Matrix4 monsterWorld=CreateTransformMatrix(monsterTransform.position,monsterTransform.rotation,monsterTransform.scale);
    const SkinnedRenderablePart3D monsterPart{maleMesh_.handle,monsterWorld,Matrix4::Identity(),std::uint32_t(maleModel_.bindMesh.indices.size()),maleSubmeshes_,maleMaterials_,{11}};
    const std::size_t monsterDrawBegin=draw.skinnedMeshInstances.size();
    if(monsterHistory_.Submit(draw,monsterPalette,std::span<const SkinnedRenderablePart3D>{&monsterPart,1},false,&skinnedGroupStats_))for(std::size_t index=monsterDrawBegin;index<draw.skinnedMeshInstances.size();++index)draw.skinnedMeshInstances[index].identity={{GameplayCharacterIdentityHigh,2},1};
  }
  const auto stats=rules_.GetStats();
  const bool cleanCapturePresentation=!captureDirectory_.empty()&&(capturePresentationWarmupActive_||capturePresentationRequestPending_||capturePresentationPending_);
  const auto hudPolicy=BuildGameplayHudPresentationPolicy3D(diagnosticsVisible_,rules_.PaletteOpen(),cleanCapturePresentation);
  if(cleanCapturePresentation)++captureCleanHudFrames_;
  if(GameplayHudShowsCompactInstructions3D(hudPolicy,cleanCapturePresentation))
    draw.Text({18,14},"[WASD] MOVE  [LMB] ATTACK  [TAB] BUILD  [F1] HELP",.9f,Cyan);
  if(hudPolicy.instructions){
    draw.Quad({640,32},{1280,64},{.008f,.018f,.022f,.92f});
    draw.Text({18,14},"[F1] HIDE HELP  [1/2/3,V] CAMERA  [WASD/SHIFT/SPACE/C] MOVE  [LMB] COMBO  [TAB/B] PALETTE",1.25f,Cyan);
  }
  if(hudPolicy.palette){
    draw.Quad({190,585},{360,230},{.008f,.018f,.022f,.9f});
    draw.Text({22,486},"OBJECT PALETTE",1.7f,Gold);
    draw.Text({22,658},std::format("ENT {}/{}  UNDO {} REDO {}",stats.entities,Rules3D::MaximumEntities,stats.undoDepth,stats.redoDepth),1.f,Gold);
    draw.Text({22,680},"DRAG/LMB PLACE  RMB/ESC CANCEL",.94f,Cyan);
    for(std::size_t index=0;index<GameplayObjectPaletteUi3D::Categories.size();++index){
      const auto bounds=paletteUi_->ItemBounds(index);const bool selected=rules_.Category()==GameplayObjectPaletteUi3D::Categories[index];
      draw.Quad({bounds.x+bounds.width*.5f,bounds.y+bounds.height*.5f},{bounds.width,bounds.height},selected?Color{.14f,.34f,.3f,.94f}:Color{.025f,.055f,.065f,.9f});
      draw.Text({bounds.x+9,bounds.y+4},std::format("[{}] {}",index+4,GameplayObjectPaletteUi3D::Labels[index]),1.05f,selected?Gold:Cyan);
    }
    ++paletteUiDrawnFrames_;
  }
  if(hudPolicy.combatStatus)draw.Text({790,684},std::format("MONSTER HP {}  COMBO HITS {} KILLS {}",combat_->Monster().health,combat_->Stats().hits,combat_->Stats().kills),1.1f,combat_->Monster().alive?Cyan:Gold);
  if(captureTemporalWarmupActive_&&captureTemporalWarmupRemaining_){--captureTemporalWarmupRemaining_;++captureTemporalWarmupFramesCurrent_;}
  if(capturePresentationWarmupActive_&&capturePresentationWarmupRemaining_){--capturePresentationWarmupRemaining_;++capturePresentationWarmupFramesCurrent_;}
  if(capturePairFrozen_&&capturePending_&&captureDiagnosticAsyncWaitFrames_<(std::numeric_limits<std::uint32_t>::max)())++captureDiagnosticAsyncWaitFrames_;
}
