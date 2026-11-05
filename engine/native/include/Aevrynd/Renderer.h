#pragma once
#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <cmath>
#include <windows.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>
#include "Math.h"
#include "ScreenOcclusionWindow3D.h"
#include "TerrainLayerCoverage3D.h"
#include "Viewport.h"
#include "EnvironmentIbl3D.h"
#include "Hlod3D.h"
#include "StableId3D.h"
#include "VolumetricAtmospherePassD3D12.h"
#include "FrameCapture3D.h"

namespace Aevrynd {
class IGpuRetirementReceiptSource3D;
struct Vertex { float x,y; uint32_t color; };
class Geometry2D {
public:
    void Assign(std::span<const Vertex> source){vertices_.clear();vertices_.reserve(source.size());bool initialized=false;for(std::size_t index=0;index+2<source.size();index+=3){const Vertex& a=source[index];const Vertex& b=source[index+1];const Vertex& c=source[index+2];if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(c.x)||!std::isfinite(c.y))continue;vertices_.insert(vertices_.end(),{a,b,c});for(const Vertex* vertex:{&a,&b,&c}){if(!initialized){bounds_={{vertex->x,vertex->y},{vertex->x,vertex->y}};initialized=true;}else{bounds_.minimum.x=(std::min)(bounds_.minimum.x,vertex->x);bounds_.minimum.y=(std::min)(bounds_.minimum.y,vertex->y);bounds_.maximum.x=(std::max)(bounds_.maximum.x,vertex->x);bounds_.maximum.y=(std::max)(bounds_.maximum.y,vertex->y);}}}if(!initialized)bounds_={};}
    void Clear(){vertices_.clear();bounds_={};}
    std::size_t TrimUnusedMemory(){const std::size_t before=RetainedBytes();std::vector<Vertex>(vertices_).swap(vertices_);const std::size_t after=RetainedBytes();return before>after?before-after:0;}
    [[nodiscard]] std::span<const Vertex> Vertices()const{return vertices_;}
    [[nodiscard]] Bounds2D Bounds()const{return bounds_;}
    [[nodiscard]] std::size_t RetainedBytes()const{return vertices_.capacity()*sizeof(Vertex);}
    [[nodiscard]] bool Empty()const{return vertices_.empty();}
private:
    std::vector<Vertex> vertices_{};
    Bounds2D bounds_{};
};
enum class DrawListCapacityProfile:uint8_t {Frame,Minimal2D};
enum class BlendMode2D:uint8_t {Alpha,Additive};
struct DrawSpan2D {size_t firstVertex{},vertexCount{};};
struct RadialLight2D {Vec2 center{},radius{32,16};Color color{1,1,1,.25f};float intensity{1.f};int segments{18};};
struct Vertex3D {float x,y,z;uint32_t normal;uint32_t color;float u{},v{};};
struct SkinnedVertex3D {float x,y,z;uint32_t normal;uint32_t color;float u{},v{};uint32_t tangent{};std::array<uint16_t,4> joints{},weights{};};
struct MeshHandle3D {uint32_t value{};uint64_t contentPrimary{},contentSecondary{};uint32_t vertexCount{},indexCount{};constexpr MeshHandle3D()=default;constexpr MeshHandle3D(uint32_t handle,uint64_t primary=0,uint64_t secondary=0,uint32_t vertices=0,uint32_t indices=0):value(handle),contentPrimary(primary),contentSecondary(secondary),vertexCount(vertices),indexCount(indices){}explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const MeshHandle3D& other)const{return value==other.value;}};
struct RenderObjectIdentity3D {StableObjectId3D id{};uint64_t generation{};StableObjectId3D hlodClusterId{};bool sharedLodDither{},hlodProxy{};explicit constexpr operator bool()const{return static_cast<bool>(id)&&generation!=0;}constexpr bool operator==(const RenderObjectIdentity3D& other)const{return id==other.id&&generation==other.generation;}};
static_assert(RenderObjectIdentity3D{{1,2},3,{4,5},true,true}==RenderObjectIdentity3D{{1,2},3,{6,7},false,false});
struct ObjectOcclusionOpacityUpdate3D {size_t matchedStatic{},matchedSkinned{},matchedMultipart{},generationConflicts{};bool missing{true};[[nodiscard]] explicit constexpr operator bool()const noexcept{return !missing&&!generationConflicts&&(matchedStatic||matchedSkinned);}};
[[nodiscard]] inline bool ValidOcclusionOpacity3D(float opacity)noexcept{return std::isfinite(opacity)&&opacity>=0.f&&opacity<=1.f;}
struct OcclusionDitherSeeds3D {uint32_t first{},second{};friend constexpr bool operator==(const OcclusionDitherSeeds3D&,const OcclusionDitherSeeds3D&)=default;};
class OcclusionDitherSeedRegistry3D {
public:
    void Reserve(size_t count){owners_.reserve(count);}
    [[nodiscard]] bool Register(RenderObjectIdentity3D identity,OcclusionDitherSeeds3D seeds){if(!identity)return true;const uint64_t key=(uint64_t(seeds.first)<<32)|seeds.second;const auto [found,inserted]=owners_.emplace(key,identity);if(!inserted&&found->second!=identity){++conflicts_;return false;}return true;}
    [[nodiscard]] size_t Conflicts()const noexcept{return conflicts_;}
private:
    std::unordered_map<uint64_t,RenderObjectIdentity3D> owners_{};size_t conflicts_{};
};
[[nodiscard]] constexpr uint64_t MixOcclusionIdentity64_3D(uint64_t value)noexcept{value^=value>>30;value*=0xbf58476d1ce4e5b9ull;value^=value>>27;value*=0x94d049bb133111ebull;return value^(value>>31);}
[[nodiscard]] constexpr OcclusionDitherSeeds3D OcclusionDitherSeedsFor3D(RenderObjectIdentity3D identity)noexcept{const uint64_t first=MixOcclusionIdentity64_3D(identity.id.high^std::rotl(identity.id.low,17)^identity.generation^0x243f6a8885a308d3ull),second=MixOcclusionIdentity64_3D(identity.id.low^std::rotl(identity.id.high,29)^std::rotl(identity.generation,7)^0x13198a2e03707344ull);return{uint32_t(first^(first>>32)),uint32_t(second^(second>>32))};}
[[nodiscard]] inline float OcclusionDitherThreshold3D(Vec3 objectCoordinate,OcclusionDitherSeeds3D seeds)noexcept{const auto quantize=[](float value){return uint32_t(int32_t(std::floor(double(value)*64.0)));};uint32_t noise=quantize(objectCoordinate.x)*0x9e3779b9u^std::rotl(quantize(objectCoordinate.y),11)^std::rotl(quantize(objectCoordinate.z),22)^seeds.first;noise^=std::rotl(seeds.second,7);noise^=noise>>16;noise*=0x7feb352du;noise^=noise>>15;noise*=0x846ca68bu;noise^=noise>>16;return float((double(noise)+.5)/4294967296.0);}
enum class ObjectIdCaptureCompatibility3D:uint8_t {Supported,DlssActive,RenderOutputSizeMismatch,NativeDepthUnavailable};
struct ObjectIdCaptureCompatibilityInput3D {bool dlssActive{};unsigned renderWidth{},renderHeight{},outputWidth{},outputHeight{};bool nativeDepthAvailable{};};
[[nodiscard]] constexpr ObjectIdCaptureCompatibility3D EvaluateObjectIdCaptureCompatibility3D(ObjectIdCaptureCompatibilityInput3D input)noexcept{if(input.dlssActive)return ObjectIdCaptureCompatibility3D::DlssActive;if(!input.renderWidth||!input.renderHeight||input.renderWidth!=input.outputWidth||input.renderHeight!=input.outputHeight)return ObjectIdCaptureCompatibility3D::RenderOutputSizeMismatch;if(!input.nativeDepthAvailable)return ObjectIdCaptureCompatibility3D::NativeDepthUnavailable;return ObjectIdCaptureCompatibility3D::Supported;}
enum class ReadbackEmptyReason3D:uint8_t {None,EmptyScene,NoVisibleSemanticPixels};
struct ReadbackCaptureSequenceToken3D {uint64_t value{};[[nodiscard]] explicit constexpr operator bool()const noexcept{return value!=0;}friend constexpr bool operator==(const ReadbackCaptureSequenceToken3D&,const ReadbackCaptureSequenceToken3D&)=default;};
struct ReadbackCaptureSequencePhase3D {ReadbackCaptureSequenceToken3D token{};uint32_t index{};[[nodiscard]] explicit constexpr operator bool()const noexcept{return static_cast<bool>(token);}friend constexpr bool operator==(const ReadbackCaptureSequencePhase3D&,const ReadbackCaptureSequencePhase3D&)=default;};
enum class ReadbackCaptureSequenceJitterPolicy3D:uint8_t {Current,FixedSample};
struct ReadbackCaptureSequenceDescriptor3D {uint32_t expectedPhaseCount{};ReadbackCaptureSequenceJitterPolicy3D jitterPolicy{ReadbackCaptureSequenceJitterPolicy3D::Current};uint32_t fixedSampleIndex{};uint64_t requiredLinearHdrPhaseMask{};friend constexpr bool operator==(const ReadbackCaptureSequenceDescriptor3D&,const ReadbackCaptureSequenceDescriptor3D&)=default;};
struct ObjectIdCaptureRequestMetadata3D {uint64_t drawListRevision{},cameraHash{},stateHash{};ReadbackEmptyReason3D emptyReason{ReadbackEmptyReason3D::None};ReadbackCaptureSequencePhase3D sequence{};uint64_t sequenceInvariantHash{};friend constexpr bool operator==(const ObjectIdCaptureRequestMetadata3D&,const ObjectIdCaptureRequestMetadata3D&)=default;};
using DepthCaptureRequestMetadata3D=ObjectIdCaptureRequestMetadata3D;
using DepthCaptureCompatibility3D=ObjectIdCaptureCompatibility3D;
using LinearHdrCaptureRequestMetadata3D=ObjectIdCaptureRequestMetadata3D;
struct ReadbackRasterProvenance3D {uint64_t frameSerial{},viewProjectionHash{},tokenGeneration{},temporalHistoryReadGeneration{};uint32_t jitterIndex{},temporalHistoryReadIndex{};Vec2 jitterPixels{};[[nodiscard]] bool Complete()const noexcept{return frameSerial&&viewProjectionHash&&tokenGeneration&&temporalHistoryReadGeneration&&jitterIndex<8&&temporalHistoryReadIndex<2&&std::isfinite(jitterPixels.x)&&std::isfinite(jitterPixels.y);}friend constexpr bool operator==(const ReadbackRasterProvenance3D& a,const ReadbackRasterProvenance3D& b)noexcept{return a.frameSerial==b.frameSerial&&a.viewProjectionHash==b.viewProjectionHash&&a.tokenGeneration==b.tokenGeneration&&a.temporalHistoryReadGeneration==b.temporalHistoryReadGeneration&&a.jitterIndex==b.jitterIndex&&a.temporalHistoryReadIndex==b.temporalHistoryReadIndex&&a.jitterPixels.x==b.jitterPixels.x&&a.jitterPixels.y==b.jitterPixels.y;}};
static constexpr uint32_t MaximumDirectionalShadowCascades3D=4;
struct DirectionalShadowFrameProvenance3D {uint64_t frameSerial{},resourceGeneration{};uint32_t activeCascades{};size_t casters{},drawCalls{},cascadeDrawCalls{};std::array<uint64_t,MaximumDirectionalShadowCascades3D> cascadeFingerprints{},rendered{},reused{};[[nodiscard]] bool Complete()const noexcept{if(!frameSerial||activeCascades>MaximumDirectionalShadowCascades3D)return false;if(!activeCascades)return!casters&&!drawCalls&&!cascadeDrawCalls;for(uint32_t cascade=0;cascade<MaximumDirectionalShadowCascades3D;++cascade){if(cascade<activeCascades){if(!cascadeFingerprints[cascade]||rendered[cascade]+reused[cascade]!=1)return false;}else if(rendered[cascade]||reused[cascade])return false;}return resourceGeneration&&casters&&drawCalls&&cascadeDrawCalls==drawCalls;}friend constexpr bool operator==(const DirectionalShadowFrameProvenance3D&,const DirectionalShadowFrameProvenance3D&)=default;};
enum class ReadbackCaptureSequenceStatus3D:uint8_t {Active,Complete,TokenMismatch,PhaseMismatch,PairIncomplete,CaptureNotTaken,Cancelled,ResizeInvalidated,DeviceLost};
enum class AtmosphereSequenceReuseReject3D:uint8_t {None,NoPhaseZero,TokenMismatch,FingerprintMismatch,ResourceGenerationMismatch,OutputMissing,OutputShapeMismatch,HistoryMismatch,FenceInvalid,ResizeInvalidated,QualityInvalidated,DeviceInvalidated};
struct AtmosphereDepthFingerprint3D {uint64_t resourceEpoch{};uint32_t width{},height{},format{},sampleCount{},state{};friend constexpr bool operator==(const AtmosphereDepthFingerprint3D&,const AtmosphereDepthFingerprint3D&)=default;};
struct AtmosphereSequenceFingerprintInput3D {uint64_t camera{},previousCamera{},sceneContent{},cloud{},quality{},historyGeneration{};AtmosphereDepthFingerprint3D depth{};uint32_t historyIndex{},width{},height{};float environmentTime{},viewportX{},viewportY{},viewportWidth{},viewportHeight{},resolutionScale{};friend constexpr bool operator==(const AtmosphereSequenceFingerprintInput3D&,const AtmosphereSequenceFingerprintInput3D&)=default;};
[[nodiscard]] inline uint64_t AtmosphereSequenceFingerprint3D(const AtmosphereSequenceFingerprintInput3D& input)noexcept{uint64_t hash=1469598103934665603ull;const auto mix=[&](const auto& field){const auto* value=reinterpret_cast<const uint8_t*>(&field);for(size_t index=0;index<sizeof(field);++index){hash^=value[index];hash*=1099511628211ull;}};mix(input.camera);mix(input.previousCamera);mix(input.sceneContent);mix(input.cloud);mix(input.quality);mix(input.historyGeneration);mix(input.depth.resourceEpoch);mix(input.depth.width);mix(input.depth.height);mix(input.depth.format);mix(input.depth.sampleCount);mix(input.depth.state);mix(input.historyIndex);mix(input.width);mix(input.height);mix(input.environmentTime);mix(input.viewportX);mix(input.viewportY);mix(input.viewportWidth);mix(input.viewportHeight);mix(input.resolutionScale);return hash;}
struct RenderResourceIdentity3D {uint64_t generation{};uint32_t width{},height{},format{},sampleCount{},state{};[[nodiscard]] constexpr bool Complete()const noexcept{return generation&&width&&height&&format&&sampleCount;}friend constexpr bool operator==(const RenderResourceIdentity3D&,const RenderResourceIdentity3D&)=default;};
struct AtmosphereReadbackProvenance3D {uint64_t fingerprint{},resourceCreates{},resourceReleases{},submissionFence{},historyGeneration{};RenderResourceIdentity3D outputIdentity{};uint32_t outputWidth{},outputHeight{},historyIndex{};AtmosphereSequenceReuseReject3D rejection{AtmosphereSequenceReuseReject3D::None};bool dispatched{},reused{};[[nodiscard]] constexpr bool Complete()const noexcept{return fingerprint&&outputIdentity.Complete()&&submissionFence&&outputWidth&&outputHeight&&historyGeneration&&(dispatched!=reused)&&rejection==AtmosphereSequenceReuseReject3D::None;}friend constexpr bool operator==(const AtmosphereReadbackProvenance3D&,const AtmosphereReadbackProvenance3D&)=default;};
struct ReadbackCaptureSequencePhaseGpuProvenance3D {uint32_t phaseIndex{};AtmosphereReadbackProvenance3D atmosphere{},temporalResolve{};friend constexpr bool operator==(const ReadbackCaptureSequencePhaseGpuProvenance3D&,const ReadbackCaptureSequencePhaseGpuProvenance3D&)=default;};
struct ReadbackCaptureSequenceGpuProvenance3D {ReadbackCaptureSequenceToken3D token{};std::vector<ReadbackCaptureSequencePhaseGpuProvenance3D> phases{};};
struct CanonicalDrawFingerprintReceipt3D {uint32_t schemaVersion{1};uint64_t topology{},material{},transform{},palette{},identity{},order{},legacyHash{};[[nodiscard]] constexpr bool Complete()const noexcept{return schemaVersion==1&&topology&&material&&transform&&palette&&identity&&order;}friend constexpr bool operator==(const CanonicalDrawFingerprintReceipt3D&,const CanonicalDrawFingerprintReceipt3D&)=default;};
enum class DeterminismIntegrityMode3D:uint8_t {Runtime,SealedCapture};
enum class DeterminismIntegrityRejectionReason3D:uint8_t {None,UnknownMode,MissingResidentMaterial,MissingContentIntegrity};
struct DeterminismIntegrityReceipt3D {DeterminismIntegrityMode3D requestedMode{DeterminismIntegrityMode3D::Runtime},actualMode{DeterminismIntegrityMode3D::Runtime};DeterminismIntegrityRejectionReason3D rejectionReason{DeterminismIntegrityRejectionReason3D::None};size_t sampledTexturedMaterials{},verifiedTexturedMaterials{},rejectedTexturedMaterials{};bool accepted{true};friend constexpr bool operator==(const DeterminismIntegrityReceipt3D&,const DeterminismIntegrityReceipt3D&)=default;};
struct FrameBoundRenderProvenance3D {uint64_t frameSerial{},sceneDrawRevision{},worldRevision{};size_t submittedMeshInstances{},submittedSkinnedInstances{},submittedTerrainInstances{},submittedWorldVertices{},submittedWorldIndices{};[[nodiscard]] constexpr bool Complete()const noexcept{return frameSerial&&sceneDrawRevision&&(!(submittedWorldVertices||submittedWorldIndices)||worldRevision);}friend constexpr bool operator==(const FrameBoundRenderProvenance3D&,const FrameBoundRenderProvenance3D&)=default;};
struct FrameBoundDeterminismEvidence3D {uint64_t frameSerial{};DeterminismIntegrityReceipt3D integrity{};CanonicalDrawFingerprintReceipt3D canonical{};FrameBoundRenderProvenance3D render{};[[nodiscard]] constexpr bool Complete()const noexcept{return frameSerial&&render.frameSerial==frameSerial&&render.Complete()&&canonical.Complete()&&integrity.sampledTexturedMaterials==integrity.verifiedTexturedMaterials+integrity.rejectedTexturedMaterials&&(integrity.accepted==(integrity.rejectedTexturedMaterials==0))&&(integrity.accepted?integrity.rejectionReason==DeterminismIntegrityRejectionReason3D::None:integrity.rejectionReason!=DeterminismIntegrityRejectionReason3D::None);}friend constexpr bool operator==(const FrameBoundDeterminismEvidence3D&,const FrameBoundDeterminismEvidence3D&)=default;};
enum class LinearHdrReadbackSource3D:uint8_t {Unknown,ProductionHdrSceneColor};
enum class LinearHdrPixelFormat3D:uint32_t {Unknown,R16G16B16A16Float=10};
struct LinearHdrFiniteStatistics3D {uint64_t finiteChannelCount{},nanChannelCount{},positiveInfinityChannelCount{},negativeInfinityChannelCount{},negativeFiniteChannelCount{},aboveOneFiniteChannelCount{};float minimumFinite{},maximumFinite{};double finiteSum{};[[nodiscard]] bool Complete(size_t expectedChannels)const noexcept{return expectedChannels&&!nanChannelCount&&!positiveInfinityChannelCount&&!negativeInfinityChannelCount&&finiteChannelCount==expectedChannels&&negativeFiniteChannelCount<=finiteChannelCount&&aboveOneFiniteChannelCount<=finiteChannelCount&&std::isfinite(minimumFinite)&&std::isfinite(maximumFinite)&&std::isfinite(finiteSum)&&minimumFinite<=maximumFinite;}friend constexpr bool operator==(const LinearHdrFiniteStatistics3D&,const LinearHdrFiniteStatistics3D&)=default;};
struct LinearHdrReadbackReceipt3D {uint32_t schemaVersion{1};LinearHdrReadbackSource3D source{LinearHdrReadbackSource3D::Unknown};LinearHdrPixelFormat3D format{LinearHdrPixelFormat3D::Unknown};unsigned width{},height{},tightRowBytes{},gpuRowPitch{};size_t rawHalfByteCount{};uint64_t resourceGeneration{},submissionFence{};std::array<std::byte,32> rawHalfSha256{};LinearHdrCaptureRequestMetadata3D requestMetadata{};ReadbackRasterProvenance3D rasterProvenance{};FrameBoundDeterminismEvidence3D determinism{};LinearHdrFiniteStatistics3D statistics{};[[nodiscard]] bool Complete()const noexcept{if(!width||!height||width>(std::numeric_limits<unsigned>::max)()/8||size_t(height)>(std::numeric_limits<size_t>::max)()/(size_t(width)*8))return false;const size_t expectedRowBytes=size_t(width)*8,expectedBytes=expectedRowBytes*height;const bool metadata=requestMetadata.drawListRevision&&requestMetadata.cameraHash&&requestMetadata.stateHash&&requestMetadata.emptyReason==ReadbackEmptyReason3D::None&&(!requestMetadata.sequence||requestMetadata.sequenceInvariantHash);return schemaVersion==1&&source==LinearHdrReadbackSource3D::ProductionHdrSceneColor&&format==LinearHdrPixelFormat3D::R16G16B16A16Float&&tightRowBytes==expectedRowBytes&&gpuRowPitch>=tightRowBytes&&(gpuRowPitch%256)==0&&rawHalfByteCount==expectedBytes&&resourceGeneration&&submissionFence&&std::ranges::any_of(rawHalfSha256,[](std::byte value){return value!=std::byte{};})&&metadata&&rasterProvenance.Complete()&&determinism.Complete()&&determinism.frameSerial==rasterProvenance.frameSerial&&statistics.Complete(expectedBytes/2);}friend constexpr bool operator==(const LinearHdrReadbackReceipt3D&,const LinearHdrReadbackReceipt3D&)=default;};
[[nodiscard]] bool ValidateLinearHdrReadbackPayloadHash3D(std::span<const std::byte> bytes,const std::array<std::byte,32>& expected)noexcept;
struct LinearHdrReadback3D {LinearHdrReadbackReceipt3D receipt{};std::vector<std::byte> rawHalfBytes{};[[nodiscard]] bool Complete()const noexcept{return receipt.Complete()&&rawHalfBytes.size()==receipt.rawHalfByteCount&&ValidateLinearHdrReadbackPayloadHash3D(rawHalfBytes,receipt.rawHalfSha256);}};
[[nodiscard]] bool CanonicalLinearHdrReadbackReceiptDigest3D(const LinearHdrReadbackReceipt3D& receipt,std::array<std::byte,32>& digest)noexcept;
struct ReadbackCaptureSequencePhaseReceipt3D {uint32_t phaseIndex{};ObjectIdCaptureRequestMetadata3D metadata{};ReadbackRasterProvenance3D raster{};FrameBoundDeterminismEvidence3D determinism{};uint64_t drawFingerprintExcludingOpacity{},drawFingerprintIncludingOpacity{},identityOrderHash{};bool linearHdrCaptured{};LinearHdrReadbackReceipt3D linearHdrReceipt{};std::array<std::byte,32> linearHdrReceiptDigest{};friend constexpr bool operator==(const ReadbackCaptureSequencePhaseReceipt3D&,const ReadbackCaptureSequencePhaseReceipt3D&)=default;};
struct ReadbackCaptureSequenceReceipt3D {ReadbackCaptureSequenceToken3D token{};uint32_t expectedPhaseCount{};std::vector<ReadbackCaptureSequencePhaseReceipt3D> phases{};ReadbackCaptureSequenceDescriptor3D descriptor{};[[nodiscard]] bool Complete()const noexcept{const bool validPolicy=descriptor.jitterPolicy==ReadbackCaptureSequenceJitterPolicy3D::Current||descriptor.jitterPolicy==ReadbackCaptureSequenceJitterPolicy3D::FixedSample,validFixedSample=descriptor.jitterPolicy!=ReadbackCaptureSequenceJitterPolicy3D::FixedSample||descriptor.fixedSampleIndex<8,validHdrMask=expectedPhaseCount<=64&&(expectedPhaseCount==64||(descriptor.requiredLinearHdrPhaseMask>>expectedPhaseCount)==0);if(!token||!expectedPhaseCount||descriptor.expectedPhaseCount!=expectedPhaseCount||!validPolicy||!validFixedSample||!validHdrMask||phases.size()!=expectedPhaseCount)return false;for(size_t index=0;index<phases.size();++index){const auto& phase=phases[index];if(phase.phaseIndex!=index||phase.metadata.sequence.token!=token||phase.metadata.sequence.index!=index||!phase.metadata.drawListRevision||!phase.metadata.cameraHash||!phase.metadata.stateHash||phase.metadata.emptyReason!=ReadbackEmptyReason3D::None||!phase.metadata.sequenceInvariantHash||!phase.drawFingerprintExcludingOpacity||!phase.drawFingerprintIncludingOpacity||!phase.identityOrderHash||!phase.raster.Complete()||!phase.determinism.Complete()||phase.determinism.frameSerial!=phase.raster.frameSerial||descriptor.jitterPolicy==ReadbackCaptureSequenceJitterPolicy3D::FixedSample&&phase.raster.jitterIndex!=descriptor.fixedSampleIndex)return false;const bool hdrRequired=(descriptor.requiredLinearHdrPhaseMask&(uint64_t{1}<<index))!=0;if(phase.linearHdrCaptured!=hdrRequired)return false;std::array<std::byte,32> hdrDigest{};if(phase.linearHdrCaptured){if(!phase.linearHdrReceipt.Complete()||phase.linearHdrReceipt.requestMetadata!=phase.metadata||phase.linearHdrReceipt.rasterProvenance!=phase.raster||phase.linearHdrReceipt.determinism!=phase.determinism||!CanonicalLinearHdrReadbackReceiptDigest3D(phase.linearHdrReceipt,hdrDigest)||hdrDigest!=phase.linearHdrReceiptDigest)return false;}else if(phase.linearHdrReceipt!=LinearHdrReadbackReceipt3D{}||std::ranges::any_of(phase.linearHdrReceiptDigest,[](std::byte value){return value!=std::byte{};}))return false;if(index){const auto& previous=phases[index-1];if(phase.metadata.cameraHash!=previous.metadata.cameraHash||phase.metadata.sequenceInvariantHash!=previous.metadata.sequenceInvariantHash||phase.drawFingerprintExcludingOpacity!=previous.drawFingerprintExcludingOpacity||phase.identityOrderHash!=previous.identityOrderHash||phase.raster.frameSerial<=previous.raster.frameSerial||phase.raster.tokenGeneration<=previous.raster.tokenGeneration||phase.raster.temporalHistoryReadGeneration!=previous.raster.temporalHistoryReadGeneration||phase.raster.temporalHistoryReadIndex!=previous.raster.temporalHistoryReadIndex||phase.raster.viewProjectionHash!=previous.raster.viewProjectionHash||phase.raster.jitterIndex!=previous.raster.jitterIndex||phase.raster.jitterPixels.x!=previous.raster.jitterPixels.x||phase.raster.jitterPixels.y!=previous.raster.jitterPixels.y)return false;}}return true;}};
struct ReadbackCaptureSequenceResult3D {ReadbackCaptureSequenceStatus3D status{ReadbackCaptureSequenceStatus3D::DeviceLost};ReadbackCaptureSequenceReceipt3D receipt{};[[nodiscard]] explicit operator bool()const noexcept{return status==ReadbackCaptureSequenceStatus3D::Complete&&receipt.Complete();}};
struct DepthCapture3D {unsigned width{},height{},rowPitch{};std::vector<float> pixels;std::array<std::byte,32> pixelSha256{};uint64_t generation{};DepthCaptureRequestMetadata3D requestMetadata{};ReadbackRasterProvenance3D rasterProvenance{};FrameBoundDeterminismEvidence3D determinism{};size_t foregroundPixelCount{};bool worldPassSubmitted{};DepthCaptureCompatibility3D compatibility{DepthCaptureCompatibility3D::Supported};[[nodiscard]] bool Complete()const noexcept{const bool shape=compatibility==DepthCaptureCompatibility3D::Supported&&width&&height&&rowPitch>=width*sizeof(float)&&(rowPitch%256)==0&&pixels.size()==size_t(width)*height&&rasterProvenance.Complete()&&determinism.Complete()&&determinism.frameSerial==rasterProvenance.frameSerial;if(!shape)return false;switch(requestMetadata.emptyReason){case ReadbackEmptyReason3D::None:return worldPassSubmitted&&foregroundPixelCount>0;case ReadbackEmptyReason3D::EmptyScene:return!worldPassSubmitted&&foregroundPixelCount==0;case ReadbackEmptyReason3D::NoVisibleSemanticPixels:return worldPassSubmitted&&foregroundPixelCount==0;}return false;}};
struct ObjectIdSemanticCoverage3D {RenderObjectIdentity3D identity{};size_t nonzeroPixelCount{};double coverage{};friend constexpr bool operator==(const ObjectIdSemanticCoverage3D&,const ObjectIdSemanticCoverage3D&)=default;};
enum class ObjectIdMaterialBindingSource3D:uint8_t {Unknown,ProductionObjectTokenPath};
struct ObjectIdAuthoritativeMaterialBinding3D {RenderObjectIdentity3D identity{};uint64_t materialFingerprint{},bindingHash{};uint32_t order{},partCount{};[[nodiscard]] constexpr bool Complete()const noexcept{return identity&&materialFingerprint&&bindingHash&&partCount;}friend constexpr bool operator==(const ObjectIdAuthoritativeMaterialBinding3D&,const ObjectIdAuthoritativeMaterialBinding3D&)=default;};
[[nodiscard]] bool CanonicalObjectIdMaterialBindingsDigest3D(ObjectIdMaterialBindingSource3D source,std::span<const ObjectIdAuthoritativeMaterialBinding3D> rows,std::array<std::byte,32>& digest)noexcept;
struct ObjectIdCapture3D {unsigned width{},height{};std::vector<uint32_t> pixels;std::vector<RenderObjectIdentity3D> tokenMap;uint64_t generation{};std::vector<ObjectIdSemanticCoverage3D> semanticCoverage;std::vector<ObjectIdAuthoritativeMaterialBinding3D> materialBindings;ObjectIdCaptureRequestMetadata3D requestMetadata{};ReadbackRasterProvenance3D rasterProvenance{};FrameBoundDeterminismEvidence3D determinism{};DirectionalShadowFrameProvenance3D directionalShadow{};uint64_t acceptedDrawFingerprintExcludingOpacity{},acceptedDrawFingerprintIncludingOpacity{},acceptedIdentityOrderHash{};size_t submittedOpaqueStatic{},submittedSkinned{},semanticPixelCount{},rejectedMissingIdentity{},rejectedMaskedStatic{},rejectedSkinned{},rejectedGenerationConflict{},rejectedOcclusionSeedCollision{},rejectedMaterialBindings{};bool opaqueStaticComplete{},maskedStaticComplete{},skinnedComplete{};ObjectIdCaptureCompatibility3D compatibility{ObjectIdCaptureCompatibility3D::Supported};ObjectIdMaterialBindingSource3D materialBindingSource{ObjectIdMaterialBindingSource3D::Unknown};std::array<std::byte,32> materialBindingsDigest{};[[nodiscard]] const RenderObjectIdentity3D* Resolve(uint32_t token)const noexcept{return token&&token<=tokenMap.size()?&tokenMap[token-1]:nullptr;}[[nodiscard]] const ObjectIdSemanticCoverage3D* Semantic(RenderObjectIdentity3D identity)const noexcept{const auto found=std::ranges::find(semanticCoverage,identity,&ObjectIdSemanticCoverage3D::identity);return found==semanticCoverage.end()?nullptr:&*found;}[[nodiscard]] size_t RequiredIdentityPixels(RenderObjectIdentity3D identity)const noexcept{const auto* value=Semantic(identity);return value?value->nonzeroPixelCount:0;}[[nodiscard]] bool HasWrongGenerationTokenOrPixels(StableObjectId3D id,uint64_t expectedGeneration)const noexcept{return std::ranges::any_of(semanticCoverage,[&](const auto& value){return value.identity.id==id&&value.identity.generation!=expectedGeneration;});}[[nodiscard]] bool Complete()const noexcept{std::array<std::byte,32> rowsDigest{};const bool base=compatibility==ObjectIdCaptureCompatibility3D::Supported&&width&&height&&opaqueStaticComplete&&maskedStaticComplete&&skinnedComplete&&!rejectedMissingIdentity&&!rejectedMaskedStatic&&!rejectedSkinned&&!rejectedGenerationConflict&&!rejectedOcclusionSeedCollision&&!rejectedMaterialBindings&&pixels.size()==size_t(width)*height&&rasterProvenance.Complete()&&determinism.Complete()&&determinism.frameSerial==rasterProvenance.frameSerial&&semanticCoverage.size()==tokenMap.size()&&materialBindings.size()==tokenMap.size()&&materialBindingSource==ObjectIdMaterialBindingSource3D::ProductionObjectTokenPath&&CanonicalObjectIdMaterialBindingsDigest3D(materialBindingSource,materialBindings,rowsDigest)&&rowsDigest==materialBindingsDigest;if(!base)return false;size_t coveragePixels{};for(size_t index=0;index<semanticCoverage.size();++index){const auto& value=semanticCoverage[index];const auto& binding=materialBindings[index];const size_t rasterPixels=std::ranges::count(pixels,uint32_t(index+1));if(value.identity!=tokenMap[index]||binding.order!=index||binding.identity!=tokenMap[index]||!binding.Complete()||value.nonzeroPixelCount!=rasterPixels||value.nonzeroPixelCount>pixels.size()-coveragePixels||!std::isfinite(value.coverage)||value.coverage<0.0||value.coverage>1.0||std::abs(value.coverage-double(value.nonzeroPixelCount)/double(pixels.size()))>1e-12)return false;coveragePixels+=value.nonzeroPixelCount;}if(coveragePixels!=semanticPixelCount)return false;const size_t submitted=submittedOpaqueStatic+submittedSkinned;switch(requestMetadata.emptyReason){case ReadbackEmptyReason3D::None:return!tokenMap.empty()&&submitted>0&&semanticPixelCount>0;case ReadbackEmptyReason3D::EmptyScene:return tokenMap.empty()&&materialBindings.empty()&&submitted==0&&semanticPixelCount==0;case ReadbackEmptyReason3D::NoVisibleSemanticPixels:return!tokenMap.empty()&&submitted>0&&semanticPixelCount==0;}return false;}};

class ProductionReadbackBundle3D {
public:
    ProductionReadbackBundle3D(const ProductionReadbackBundle3D&)=delete;
    ProductionReadbackBundle3D& operator=(const ProductionReadbackBundle3D&)=delete;
    ProductionReadbackBundle3D(ProductionReadbackBundle3D&&)noexcept=default;
    ProductionReadbackBundle3D& operator=(ProductionReadbackBundle3D&&)noexcept=default;
    [[nodiscard]] bool Complete()const noexcept;
    [[nodiscard]] const LinearHdrReadback3D& Hdr()const noexcept{return hdr_;}
    [[nodiscard]] const ObjectIdCapture3D& ObjectId()const noexcept{return objectId_;}
    [[nodiscard]] const DepthCapture3D& Depth()const noexcept{return depth_;}
    [[nodiscard]] const ReadbackCaptureSequenceDescriptor3D& SequenceDescriptor()const noexcept{return descriptor_;}
private:
    ProductionReadbackBundle3D(LinearHdrReadback3D hdr,ObjectIdCapture3D objectId,DepthCapture3D depth,ReadbackCaptureSequenceDescriptor3D descriptor)noexcept:hdr_(std::move(hdr)),objectId_(std::move(objectId)),depth_(std::move(depth)),descriptor_(descriptor){}
    LinearHdrReadback3D hdr_{};ObjectIdCapture3D objectId_{};DepthCapture3D depth_{};ReadbackCaptureSequenceDescriptor3D descriptor_{};
    friend class D3D12Renderer;
};
struct SkeletonCompatibilityId3D {uint64_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const SkeletonCompatibilityId3D&)const=default;};
struct SkinnedMeshHandle3D {uint32_t value{};SkeletonCompatibilityId3D skeletonCompatibilityId{};uint32_t jointCount{};uint64_t resourceIdentity{},contentPrimary{},contentSecondary{};uint32_t vertexCount{},indexCount{};constexpr SkinnedMeshHandle3D()=default;constexpr SkinnedMeshHandle3D(uint32_t handle,SkeletonCompatibilityId3D compatibility,uint32_t joints=0,uint64_t identity=0,uint64_t primary=0,uint64_t secondary=0,uint32_t vertices=0,uint32_t indices=0):value(handle),skeletonCompatibilityId(compatibility),jointCount(joints),resourceIdentity(identity?identity:(handle?0x9e3779b97f4a7c15ull^uint64_t(handle)*0xbf58476d1ce4e5b9ull:0)),contentPrimary(primary),contentSecondary(secondary),vertexCount(vertices),indexCount(indices){}explicit constexpr operator bool()const{return value!=0&&resourceIdentity!=0;}constexpr bool operator==(const SkinnedMeshHandle3D& other)const{return value==other.value&&resourceIdentity==other.resourceIdentity;}};
struct SkinPaletteRange3D {uint32_t firstJoint{},jointCount{},generation{};SkeletonCompatibilityId3D skeletonCompatibilityId{};explicit constexpr operator bool()const{return jointCount!=0;}friend constexpr bool operator==(const SkinPaletteRange3D&,const SkinPaletteRange3D&)=default;};
struct SkinPaletteJoint3D {Matrix4 skin{};std::array<Vec4,3> normalRows{};};
struct SkinnedSubmissionReceipt3D {uint64_t submissionNonce{};uint32_t drawGeneration{},groupId{};uint64_t resourceIdentity{};explicit constexpr operator bool()const{return submissionNonce&&drawGeneration&&groupId&&resourceIdentity;}constexpr bool operator==(const SkinnedSubmissionReceipt3D&)const=default;};
enum class SkinnedSubmissionResult3D:uint8_t {Unknown,Accepted,Rejected,Expired};
struct MaterialTextureSetHandle3D {uint32_t value{};uint64_t semanticIdentity{};constexpr MaterialTextureSetHandle3D()=default;constexpr MaterialTextureSetHandle3D(uint32_t handle,uint64_t semantic=0):value(handle),semanticIdentity(semantic){}explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const MaterialTextureSetHandle3D& other)const{return value==other.value;}};
struct GpuUploadReceipt3D {uint64_t deviceEpoch{},fenceValue{};explicit constexpr operator bool()const{return deviceEpoch!=0&&fenceValue!=0;}constexpr bool operator==(const GpuUploadReceipt3D&)const=default;};
struct RendererRuntimeEpoch3D {uint64_t deviceEpoch{},resizeEpoch{};[[nodiscard]] explicit constexpr operator bool()const noexcept{return deviceEpoch&&resizeEpoch;}constexpr bool operator==(const RendererRuntimeEpoch3D&)const=default;};
class GpuRetirementReceipt3D {
public:
    constexpr GpuRetirementReceipt3D()=default;
    [[nodiscard]] explicit constexpr operator bool()const noexcept{return deviceEpoch_!=0&&fenceValue_!=0;}
    [[nodiscard]] constexpr uint64_t DeviceEpoch()const noexcept{return deviceEpoch_;}
    [[nodiscard]] constexpr uint64_t FenceValue()const noexcept{return fenceValue_;}
    friend constexpr bool operator==(const GpuRetirementReceipt3D&,const GpuRetirementReceipt3D&)=default;
private:
    constexpr GpuRetirementReceipt3D(uint64_t deviceEpoch,uint64_t fenceValue)noexcept:deviceEpoch_(deviceEpoch),fenceValue_(fenceValue){}
    uint64_t deviceEpoch_{},fenceValue_{};
    friend class D3D12Renderer;
    friend class IGpuRetirementReceiptSource3D;
};
struct AssetLeaseState3D {std::uint64_t identity{};};
using AssetLease3D=std::shared_ptr<const AssetLeaseState3D>;
enum class MaterialSurfaceMapEncoding3D:uint8_t {RoughnessR,PackedOrm,PackedOrmHeight};
// 高度是獨立線性資料；舊兩種編碼的 alpha 行為保持，不能由全域開關偷換其語義。
constexpr std::array<uint8_t,4> CanonicalMaterialSurfaceTexel3D(std::array<uint8_t,4> texel,MaterialSurfaceMapEncoding3D encoding){return encoding==MaterialSurfaceMapEncoding3D::PackedOrmHeight?texel:encoding==MaterialSurfaceMapEncoding3D::PackedOrm?std::array<uint8_t,4>{texel[0],texel[1],texel[2],texel[1]}:std::array<uint8_t,4>{255,texel[0],255,texel[0]};}
struct MaterialTextureIntegrity3D {std::uint64_t expectedBytes{};std::string expectedSha256;};
struct MaterialTextureLayerDefinition3D {std::filesystem::path baseColorPath,roughnessPath,normalPath;MaterialSurfaceMapEncoding3D surfaceEncoding{MaterialSurfaceMapEncoding3D::RoughnessR};std::array<MaterialTextureIntegrity3D,3> integrity{};};
enum class MaterialAlphaMode3D:uint8_t {Opaque,Mask,Blend};
constexpr MaterialAlphaMode3D NormalizeMaterialAlphaMode3D(MaterialAlphaMode3D value){return value==MaterialAlphaMode3D::Mask||value==MaterialAlphaMode3D::Blend?value:MaterialAlphaMode3D::Opaque;}
struct MaterialTextureSetDefinition3D {std::filesystem::path baseColorPath,roughnessPath;unsigned maximumResolution{2048};std::filesystem::path normalPath;std::array<MaterialTextureLayerDefinition3D,2> additionalLayers{};MaterialAlphaMode3D alphaMode{MaterialAlphaMode3D::Opaque};float alphaCutoff{.5f};MaterialSurfaceMapEncoding3D surfaceEncoding{MaterialSurfaceMapEncoding3D::RoughnessR};std::array<MaterialTextureIntegrity3D,3> integrity{};};
struct Surface3D {float roughness{.72f},metallic{},emissive{},occlusion{1.f};};
inline bool ValidMaterialSurfaceEncodingDefinition3D(const MaterialTextureSetDefinition3D& definition){
    const auto known=[](MaterialSurfaceMapEncoding3D value){return value==MaterialSurfaceMapEncoding3D::RoughnessR||value==MaterialSurfaceMapEncoding3D::PackedOrm||value==MaterialSurfaceMapEncoding3D::PackedOrmHeight;};
    if(!known(definition.surfaceEncoding))return false;
    for(const auto& layer:definition.additionalLayers)if(!known(layer.surfaceEncoding))return false;
    if(definition.additionalLayers[1].surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrmHeight)return false;
    const bool baseHeight=definition.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    const bool leafHeight=definition.additionalLayers[0].surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    if(!baseHeight&&!leafHeight)return true;
    if(definition.alphaMode!=MaterialAlphaMode3D::Opaque)return false;
    const auto sealed=[](const std::filesystem::path& color,const std::filesystem::path& surface,
        const std::filesystem::path& normal,const std::array<MaterialTextureIntegrity3D,3>& integrity){
        if(color.empty()||surface.empty()||normal.empty())return false;
        for(const auto& seal:integrity){if(!seal.expectedBytes||seal.expectedSha256.size()!=64)return false;
            for(char c:seal.expectedSha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
        return true;
    };
    if(baseHeight&&!sealed(definition.baseColorPath,definition.roughnessPath,definition.normalPath,definition.integrity))return false;
    const auto& leaf=definition.additionalLayers[0];
    if(leafHeight&&!sealed(leaf.baseColorPath,leaf.roughnessPath,leaf.normalPath,leaf.integrity))return false;
    return true;
}
// 僅宣告完整封印的地面基層／落葉層；真正內容雜湊由 renderer admission 再核。
inline uint32_t MaterialHeightLayerMask3D(const MaterialTextureSetDefinition3D& definition){
    if(!ValidMaterialSurfaceEncodingDefinition3D(definition))return 0;
    return (definition.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrmHeight?1u:0u)|
        (definition.additionalLayers[0].surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrmHeight?2u:0u);
}
constexpr uint32_t PackSurface3D(Surface3D surface){return PackColorRgba8({surface.roughness,surface.metallic,surface.emissive,surface.occlusion});}
enum class MaterialClass3D:uint8_t {Surface,Water,Foliage,Terrain};
enum class CharacterShadingModel3D:uint8_t {Default,Skin,Hair,Eye,Cloth};
// Terrain 的 normal detail 不讀逐 instance normalScale；保留一個可精確表示的
// 負值作 authored-biome UV 契約 sentinel；通用 layer coverage 使用另一個
// 明確 marker。其他值（包括一般負 scale）都不得啟用這兩條路徑。
inline constexpr float TerrainAuthoredBiomeNormalScaleMarker3D=-15.5f;
static constexpr std::size_t MaximumMeshInstancesPerFrame3D=65536;
static constexpr uint32_t MeshInstanceHasTexturesBit3D=1u,MeshInstanceUsesUvBit3D=2u,MeshInstanceMaterialClassShift3D=2u,MeshInstanceMaterialClassMask3D=3u<<MeshInstanceMaterialClassShift3D,MeshInstanceLodCoverageShift3D=4u,MeshInstanceLodCoverageMask3D=255u<<MeshInstanceLodCoverageShift3D,MeshInstanceLodInvertBit3D=1u<<12,MeshInstanceLodSeedShift3D=13u,MeshInstanceLodSeedMask3D=7u<<MeshInstanceLodSeedShift3D,MeshInstanceHlodSharedSeedBit3D=1u<<31;
constexpr MaterialClass3D NormalizeMaterialClass3D(MaterialClass3D value){return value==MaterialClass3D::Water||value==MaterialClass3D::Foliage||value==MaterialClass3D::Terrain?value:MaterialClass3D::Surface;}
constexpr uint32_t PackMeshInstanceFlags3D(bool hasTextures,bool usesUv,MaterialClass3D materialClass){return(hasTextures?MeshInstanceHasTexturesBit3D:0u)|(hasTextures&&usesUv?MeshInstanceUsesUvBit3D:0u)|(uint32_t(NormalizeMaterialClass3D(materialClass))<<MeshInstanceMaterialClassShift3D);}
constexpr MaterialClass3D UnpackMeshInstanceMaterialClass3D(uint32_t value){return NormalizeMaterialClass3D(MaterialClass3D((value&MeshInstanceMaterialClassMask3D)>>MeshInstanceMaterialClassShift3D));}
inline uint32_t PackMeshInstanceLodData3D(float transition,uint32_t seed,bool invert=false){transition=Clamp(std::isfinite(transition)?transition:1.f,0.f,1.f);invert=invert||(seed&8u)!=0;const uint32_t quantized=uint32_t(std::lround(transition*255.f));return(quantized<<MeshInstanceLodCoverageShift3D)|(invert?MeshInstanceLodInvertBit3D:0u)|((seed&7u)<<MeshInstanceLodSeedShift3D);}
constexpr CharacterShadingModel3D NormalizeCharacterShadingModel3D(CharacterShadingModel3D value){return value==CharacterShadingModel3D::Skin||value==CharacterShadingModel3D::Hair||value==CharacterShadingModel3D::Eye||value==CharacterShadingModel3D::Cloth?value:CharacterShadingModel3D::Default;}
inline uint32_t PackSkinnedTextureScale3D(float value){return uint32_t(std::lround(Clamp(std::isfinite(value)?value/16.f:0.f,0.f,1.f)*255.f));}
inline float UnpackSkinnedTextureScale3D(uint32_t value,unsigned shift){return(std::max)(.001f,float((value>>shift)&255u)*(16.f/255.f));}
inline uint32_t PackSkinnedMeshInstanceData3D(bool hasTextures,bool usesUv,MaterialClass3D materialClass,float baseTextureScale,float detailTextureScale,CharacterShadingModel3D shadingModel=CharacterShadingModel3D::Default){return PackMeshInstanceFlags3D(hasTextures,usesUv,materialClass)|PackMeshInstanceLodData3D(1.f,uint32_t(NormalizeCharacterShadingModel3D(shadingModel)))|(PackSkinnedTextureScale3D(baseTextureScale)<<16)|(PackSkinnedTextureScale3D(detailTextureScale)<<24);}
inline uint32_t PackSkinnedMeshInstanceData3D(bool hasTextures,bool usesUv,MaterialClass3D materialClass,float textureScale,CharacterShadingModel3D shadingModel=CharacterShadingModel3D::Default){return PackSkinnedMeshInstanceData3D(hasTextures,usesUv,materialClass,textureScale,textureScale,shadingModel);}
constexpr CharacterShadingModel3D UnpackSkinnedCharacterShadingModel3D(uint32_t value){return NormalizeCharacterShadingModel3D(CharacterShadingModel3D((value>>13)&7u));}
static constexpr uint32_t SkinnedPaletteBaseMask3D=4095u,SkinnedAlphaCutoffShift3D=12u,SkinnedAlphaMaskBit3D=1u<<20,SkinnedDoubleSidedBit3D=1u<<21,SkinnedNormalScaleShift3D=22u,SkinnedNormalScaleMask3D=1023u<<SkinnedNormalScaleShift3D;
inline uint32_t PackSkinnedPaletteMaterialData3D(uint32_t paletteBase,MaterialAlphaMode3D alphaMode,float alphaCutoff,bool doubleSided,float normalScale=1.f){const uint32_t cutoff=uint32_t(std::lround(Clamp(std::isfinite(alphaCutoff)?alphaCutoff:.5f,0.f,1.f)*255.f));const int32_t fixedNormalScale=std::clamp(int32_t(std::lround(Clamp(std::isfinite(normalScale)?normalScale:1.f,-16.f,15.96875f)*32.f)),-512,511);return(paletteBase&SkinnedPaletteBaseMask3D)|(cutoff<<SkinnedAlphaCutoffShift3D)|(NormalizeMaterialAlphaMode3D(alphaMode)==MaterialAlphaMode3D::Mask?SkinnedAlphaMaskBit3D:0u)|(doubleSided?SkinnedDoubleSidedBit3D:0u)|((uint32_t(fixedNormalScale)&1023u)<<SkinnedNormalScaleShift3D);}
constexpr uint32_t UnpackSkinnedPaletteBase3D(uint32_t value){return value&SkinnedPaletteBaseMask3D;}
constexpr bool UnpackSkinnedAlphaMasked3D(uint32_t value){return(value&SkinnedAlphaMaskBit3D)!=0;}
constexpr bool UnpackSkinnedDoubleSided3D(uint32_t value){return(value&SkinnedDoubleSidedBit3D)!=0;}
constexpr float UnpackSkinnedAlphaCutoff3D(uint32_t value){return float((value>>SkinnedAlphaCutoffShift3D)&255u)/255.f;}
constexpr float UnpackSkinnedNormalScale3D(uint32_t value){const uint32_t encoded=(value&SkinnedNormalScaleMask3D)>>SkinnedNormalScaleShift3D;const int32_t signedValue=encoded&512u?int32_t(encoded)-1024:int32_t(encoded);return float(signedValue)/32.f;}
constexpr float UnpackMeshInstanceLodCoverage3D(uint32_t value){return float((value&MeshInstanceLodCoverageMask3D)>>MeshInstanceLodCoverageShift3D)/255.f;}
constexpr uint32_t UnpackMeshInstanceLodSeed3D(uint32_t value){return(value&MeshInstanceLodSeedMask3D)>>MeshInstanceLodSeedShift3D;}
constexpr bool UnpackMeshInstanceLodInvert3D(uint32_t value){return(value&MeshInstanceLodInvertBit3D)!=0;}
inline bool MeshInstanceLodSampleSurvives3D(uint32_t value,float threshold){const float transition=UnpackMeshInstanceLodCoverage3D(value);threshold=Clamp(std::isfinite(threshold)?threshold:1.f,0.f,1.f);return UnpackMeshInstanceLodInvert3D(value)?threshold>transition:threshold<=transition;}
enum class MeshRenderLayer3D:uint8_t {Opaque,Transparent,Overlay,ShadowOnly};
static constexpr uint8_t DirectionalShadowNearCascadeBit3D=1u,DirectionalShadowFarCascadeBit3D=2u,DirectionalShadowCascade2Bit3D=4u,DirectionalShadowCascade3Bit3D=8u,DirectionalShadowAllCascades3D=15u;
enum class ShadowCascadeSelection3D:uint8_t {Automatic,Manual};
enum ShadowCascadeInvalidationReason3D:uint32_t {ShadowCascadeInvalidationNone3D=0,ShadowCascadeInvalidationCameraLight3D=1u<<0,ShadowCascadeInvalidationResource3D=1u<<1,ShadowCascadeInvalidationTopology3D=1u<<2,ShadowCascadeInvalidationTransform3D=1u<<3,ShadowCascadeInvalidationPalette3D=1u<<4,ShadowCascadeInvalidationWorld3D=1u<<5};
struct MeshInstance3D {MeshHandle3D mesh{};Matrix4 transform{Matrix4::Identity()};Matrix4 normalTransform{Matrix4::Identity()};uint32_t tint{0xffffffffu},surface{PackSurface3D({})};MeshRenderLayer3D layer{MeshRenderLayer3D::Opaque};MaterialTextureSetHandle3D textures{};float textureScale{.25f},normalScale{1.f};bool useUv{};MaterialClass3D materialClass{MaterialClass3D::Surface};float cameraDepth{};bool castsShadow{true};float lodCoverage{1.f};uint32_t lodSeed{};uint8_t shadowCascadeMask{DirectionalShadowAllCascades3D};ShadowCascadeSelection3D shadowCascadeSelection{ShadowCascadeSelection3D::Automatic};Matrix4 previousTransform{Matrix4::Identity()};bool motionValid{};MaterialAlphaMode3D alphaMode{MaterialAlphaMode3D::Opaque};float alphaCutoff{.5f};bool doubleSided{};RenderObjectIdentity3D identity{};float occlusionOpacity{1.f};};
class StaticMeshSubmission3D {
public:
    StaticMeshSubmission3D()=default;
    explicit StaticMeshSubmission3D(std::span<const MeshInstance3D> instances):instances_(std::make_shared<const std::vector<MeshInstance3D>>(instances.begin(),instances.end())),contentRevision_(NextRevision()),layoutRevision_(contentRevision_),cacheEligible_(std::ranges::all_of(instances,[](const MeshInstance3D& instance){return instance.layer==MeshRenderLayer3D::Opaque&&instance.alphaMode==MaterialAlphaMode3D::Opaque&&!instance.motionValid&&!instance.castsShadow;})){}
    [[nodiscard]] std::span<const MeshInstance3D> Instances()const{return instances_?std::span<const MeshInstance3D>{*instances_}:std::span<const MeshInstance3D>{};}
    [[nodiscard]] uint64_t ContentRevision()const{return contentRevision_;}
    [[nodiscard]] uint64_t LayoutRevision()const{return layoutRevision_;}
    [[nodiscard]] size_t RetainedBytes()const{return instances_?instances_->capacity()*sizeof(MeshInstance3D):0;}
    [[nodiscard]] bool CacheEligible()const{return cacheEligible_;}
    [[nodiscard]] std::weak_ptr<const void> LifetimeToken()const{return instances_;}
    explicit operator bool()const{return instances_!=nullptr;}
private:
    static uint64_t NextRevision(){static std::atomic<uint64_t> next{1};return next.fetch_add(1,std::memory_order_relaxed);}
    std::shared_ptr<const std::vector<MeshInstance3D>> instances_{};uint64_t contentRevision_{},layoutRevision_{};bool cacheEligible_{};
};
struct SkinnedMeshInstance3D {SkinnedMeshHandle3D mesh{};SkinPaletteRange3D palette{},previousPalette{};Matrix4 transform{Matrix4::Identity()},previousTransform{Matrix4::Identity()};Matrix4 normalTransform{Matrix4::Identity()};uint32_t tint{0xffffffffu},surface{PackSurface3D({})};MaterialTextureSetHandle3D textures{};float textureScale{.25f},normalScale{1.f},baseTextureScale{.25f},detailTextureScale{.25f};uint32_t firstIndex{},indexCount{};bool useUv{};MaterialClass3D materialClass{MaterialClass3D::Surface};CharacterShadingModel3D shadingModel{CharacterShadingModel3D::Default};MaterialAlphaMode3D alphaMode{MaterialAlphaMode3D::Opaque};float alphaCutoff{.5f};bool doubleSided{},castsShadow{true},motionValid{};uint8_t shadowCascadeMask{DirectionalShadowAllCascades3D};uint32_t submissionGroupId{},submissionDrawGeneration{};uint64_t submissionNonce{},submissionResourceIdentity{};RenderObjectIdentity3D identity{};float occlusionOpacity{1.f};};
constexpr bool TransparentBackToFront3D(const MeshInstance3D& left,const MeshInstance3D& right){return left.cameraDepth>right.cameraDepth;}
struct DirectionalLight3D {Vec3 direction{.4f,-1.f,.3f};float ambient{.28f};Color color{1.f,.96f,.86f,1.f};float intensity{.72f};};
struct PhaseRadianceProfile3D {
    Vec3 skyZenith{.16f,.28f,.43f};float skyIntensity{1.f};
    Vec3 skyHorizon{.62f,.67f,.69f};float horizonExponent{.42f};
    Vec3 groundBounce{.16f,.15f,.12f};float sunAngularRadius{.00935f};
    Vec3 analyticDiffuse{.34f,.36f,.35f};float analyticDiffuseIntensity{1.f};
    Vec3 sunDisk{1.f,.91f,.72f};float sunDiskIntensity{8.f};
    uint64_t phaseFingerprint{1};
    float lunarDetailStrength{};
    float stellarDetailStrength{};
};
[[nodiscard]] inline bool NormalizePhaseRadianceProfile3D(const PhaseRadianceProfile3D& source,PhaseRadianceProfile3D& result)noexcept{
    const auto valid=[](float v){return std::isfinite(v)&&v>=0.f;};
    if(!source.phaseFingerprint||!valid(source.skyZenith.x)||!valid(source.skyZenith.y)||!valid(source.skyZenith.z)||!valid(source.skyIntensity)||!valid(source.skyHorizon.x)||!valid(source.skyHorizon.y)||!valid(source.skyHorizon.z)||!valid(source.horizonExponent)||!valid(source.groundBounce.x)||!valid(source.groundBounce.y)||!valid(source.groundBounce.z)||!valid(source.sunAngularRadius)||!valid(source.analyticDiffuse.x)||!valid(source.analyticDiffuse.y)||!valid(source.analyticDiffuse.z)||!valid(source.analyticDiffuseIntensity)||!valid(source.sunDisk.x)||!valid(source.sunDisk.y)||!valid(source.sunDisk.z)||!valid(source.sunDiskIntensity)||!valid(source.lunarDetailStrength)||!valid(source.stellarDetailStrength))return false;
    result=source;const auto c=[](Vec3 v){return Vec3{Clamp(v.x,0.f,16.f),Clamp(v.y,0.f,16.f),Clamp(v.z,0.f,16.f)};};result.skyZenith=c(source.skyZenith);result.skyHorizon=c(source.skyHorizon);result.groundBounce=c(source.groundBounce);result.analyticDiffuse=c(source.analyticDiffuse);result.sunDisk=c(source.sunDisk);result.skyIntensity=Clamp(source.skyIntensity,0.f,8.f);result.horizonExponent=Clamp(source.horizonExponent,.05f,8.f);result.sunAngularRadius=Clamp(source.sunAngularRadius,.001f,.05f);result.analyticDiffuseIntensity=Clamp(source.analyticDiffuseIntensity,0.f,8.f);result.sunDiskIntensity=Clamp(source.sunDiskIntensity,0.f,64.f);result.lunarDetailStrength=Clamp(source.lunarDetailStrength,0.f,1.f);result.stellarDetailStrength=Clamp(source.stellarDetailStrength,0.f,1.f);return true;
}
struct EnvironmentLightingBlendSettings3D {
    Vec3 diffuseRgbScale{1.f,1.f,1.f};float diffuseIntensity{1.f};
    Vec3 specularRgbScale{1.f,1.f,1.f};float specularIntensity{1.f};
    float analyticDiffuseWeight{},iblDiffuseWeight{1.f},analyticSpecularWeight{},iblSpecularWeight{1.f};
    Vec3 objectFillRadiance{};float objectFillIntensity{};
    uint64_t phaseFingerprint{1};
    friend constexpr bool operator==(const EnvironmentLightingBlendSettings3D& a,const EnvironmentLightingBlendSettings3D& b){return a.diffuseRgbScale.x==b.diffuseRgbScale.x&&a.diffuseRgbScale.y==b.diffuseRgbScale.y&&a.diffuseRgbScale.z==b.diffuseRgbScale.z&&a.diffuseIntensity==b.diffuseIntensity&&a.specularRgbScale.x==b.specularRgbScale.x&&a.specularRgbScale.y==b.specularRgbScale.y&&a.specularRgbScale.z==b.specularRgbScale.z&&a.specularIntensity==b.specularIntensity&&a.analyticDiffuseWeight==b.analyticDiffuseWeight&&a.iblDiffuseWeight==b.iblDiffuseWeight&&a.analyticSpecularWeight==b.analyticSpecularWeight&&a.iblSpecularWeight==b.iblSpecularWeight&&a.objectFillRadiance.x==b.objectFillRadiance.x&&a.objectFillRadiance.y==b.objectFillRadiance.y&&a.objectFillRadiance.z==b.objectFillRadiance.z&&a.objectFillIntensity==b.objectFillIntensity&&a.phaseFingerprint==b.phaseFingerprint;}
};
[[nodiscard]] inline bool NormalizeEnvironmentLightingBlendSettings3D(const EnvironmentLightingBlendSettings3D& source,EnvironmentLightingBlendSettings3D& result)noexcept{
    const auto finiteNonnegative=[](float value){return std::isfinite(value)&&value>=0.f;};
    if(!source.phaseFingerprint||!finiteNonnegative(source.diffuseRgbScale.x)||!finiteNonnegative(source.diffuseRgbScale.y)||!finiteNonnegative(source.diffuseRgbScale.z)||!finiteNonnegative(source.diffuseIntensity)||!finiteNonnegative(source.specularRgbScale.x)||!finiteNonnegative(source.specularRgbScale.y)||!finiteNonnegative(source.specularRgbScale.z)||!finiteNonnegative(source.specularIntensity)||!finiteNonnegative(source.analyticDiffuseWeight)||!finiteNonnegative(source.iblDiffuseWeight)||!finiteNonnegative(source.analyticSpecularWeight)||!finiteNonnegative(source.iblSpecularWeight)||!finiteNonnegative(source.objectFillRadiance.x)||!finiteNonnegative(source.objectFillRadiance.y)||!finiteNonnegative(source.objectFillRadiance.z)||!finiteNonnegative(source.objectFillIntensity))return false;
    const float diffuseWeight=source.analyticDiffuseWeight+source.iblDiffuseWeight,specularWeight=source.analyticSpecularWeight+source.iblSpecularWeight;
    if(!std::isfinite(diffuseWeight)||!std::isfinite(specularWeight)||diffuseWeight<=0.f||specularWeight<=0.f)return false;
    result=source;
    result.diffuseRgbScale={Clamp(source.diffuseRgbScale.x,0.f,4.f),Clamp(source.diffuseRgbScale.y,0.f,4.f),Clamp(source.diffuseRgbScale.z,0.f,4.f)};
    result.diffuseIntensity=Clamp(source.diffuseIntensity,0.f,8.f);
    result.specularRgbScale={Clamp(source.specularRgbScale.x,0.f,4.f),Clamp(source.specularRgbScale.y,0.f,4.f),Clamp(source.specularRgbScale.z,0.f,4.f)};
    result.specularIntensity=Clamp(source.specularIntensity,0.f,8.f);
    result.objectFillRadiance={Clamp(source.objectFillRadiance.x,0.f,16.f),Clamp(source.objectFillRadiance.y,0.f,16.f),Clamp(source.objectFillRadiance.z,0.f,16.f)};result.objectFillIntensity=Clamp(source.objectFillIntensity,0.f,8.f);
    result.analyticDiffuseWeight=source.analyticDiffuseWeight/diffuseWeight;result.iblDiffuseWeight=source.iblDiffuseWeight/diffuseWeight;
    result.analyticSpecularWeight=source.analyticSpecularWeight/specularWeight;result.iblSpecularWeight=source.iblSpecularWeight/specularWeight;
    return true;
}
[[nodiscard]] inline uint64_t EnvironmentLightingBlendFingerprint3D(const EnvironmentLightingBlendSettings3D& value)noexcept{
    uint64_t hash=1469598103934665603ull;
    const auto append=[&](uint32_t item){for(unsigned byte=0;byte<4;++byte)hash=(hash^uint8_t(item>>(byte*8)))*1099511628211ull;};
    for(float item:{value.diffuseRgbScale.x,value.diffuseRgbScale.y,value.diffuseRgbScale.z,value.diffuseIntensity,value.specularRgbScale.x,value.specularRgbScale.y,value.specularRgbScale.z,value.specularIntensity,value.analyticDiffuseWeight,value.iblDiffuseWeight,value.analyticSpecularWeight,value.iblSpecularWeight,value.objectFillRadiance.x,value.objectFillRadiance.y,value.objectFillRadiance.z,value.objectFillIntensity})append(std::bit_cast<uint32_t>(item));
    append(uint32_t(value.phaseFingerprint));append(uint32_t(value.phaseFingerprint>>32));return hash;
}
[[nodiscard]] inline Vec3 ApplyEnvironmentLightingBlend3D(Vec3 analytic,Vec3 ibl,bool environmentAvailable,bool specular,const EnvironmentLightingBlendSettings3D& source)noexcept{
    EnvironmentLightingBlendSettings3D settings{};if(!NormalizeEnvironmentLightingBlendSettings3D(source,settings))return{};
    const Vec3 scale=specular?settings.specularRgbScale:settings.diffuseRgbScale;const float intensity=specular?settings.specularIntensity:settings.diffuseIntensity;
    const float analyticWeight=specular?settings.analyticSpecularWeight:settings.analyticDiffuseWeight,iblWeight=specular?settings.iblSpecularWeight:settings.iblDiffuseWeight;
    const Vec3 lighting=environmentAvailable?analytic*analyticWeight+ibl*iblWeight:analytic;
    return{(std::max)(0.f,lighting.x)*scale.x*intensity,(std::max)(0.f,lighting.y)*scale.y*intensity,(std::max)(0.f,lighting.z)*scale.z*intensity};
}
struct EnvironmentLightingBlendReceipt3D {
    EnvironmentLightingBlendSettings3D applied{};uint64_t appliedFingerprint{};float postProcessExposureScale{1.f};bool linearHdrUnexposed{},environmentActive{};
    [[nodiscard]] bool Complete()const noexcept{EnvironmentLightingBlendSettings3D normalized{};return NormalizeEnvironmentLightingBlendSettings3D(applied,normalized)&&std::abs(applied.analyticDiffuseWeight+applied.iblDiffuseWeight-1.f)<=1e-5f&&std::abs(applied.analyticSpecularWeight+applied.iblSpecularWeight-1.f)<=1e-5f&&appliedFingerprint==EnvironmentLightingBlendFingerprint3D(applied)&&std::isfinite(postProcessExposureScale)&&postProcessExposureScale>0.f&&linearHdrUnexposed;}
};
struct TimeOfDayExposureSettings3D {
    bool enabled{true};
    float dayExposureEv{.2f},duskExposureEv{.6f},nightExposureEv{2.f};
    float shadowLift{.018f},toeStrength{.22f},shoulderStrength{1.f};
    friend constexpr bool operator==(const TimeOfDayExposureSettings3D&,const TimeOfDayExposureSettings3D&)=default;
};
inline TimeOfDayExposureSettings3D NormalizeTimeOfDayExposureSettings3D(TimeOfDayExposureSettings3D value){const auto finite=[](float v,float fallback,float minimum,float maximum){return Clamp(std::isfinite(v)?v:fallback,minimum,maximum);};value.dayExposureEv=finite(value.dayExposureEv,.2f,-2.f,1.f);value.duskExposureEv=finite(value.duskExposureEv,.6f,value.dayExposureEv,1.f);value.nightExposureEv=finite(value.nightExposureEv,2.f,value.duskExposureEv,2.f);value.shadowLift=finite(value.shadowLift,.018f,0.f,.08f);value.toeStrength=finite(value.toeStrength,.22f,0.f,1.f);value.shoulderStrength=finite(value.shoulderStrength,1.f,.5f,2.f);return value;}
inline uint64_t TimeOfDayExposureFingerprint3D(const TimeOfDayExposureSettings3D& source){const auto value=NormalizeTimeOfDayExposureSettings3D(source);uint64_t hash=14695981039346656037ull;const auto mix=[&](uint32_t bits){for(unsigned byte=0;byte<4;++byte){hash^=uint8_t(bits>>(byte*8));hash*=1099511628211ull;}};mix(value.enabled?1u:0u);for(float field:{value.dayExposureEv,value.duskExposureEv,value.nightExposureEv,value.shadowLift,value.toeStrength,value.shoulderStrength})mix(std::bit_cast<uint32_t>(field));return hash;}
enum class TimeOfDayExposureApplyReason3D : uint8_t {Applied,Disabled,ForcedFailure};
struct TimeOfDayExposureReceipt3D {TimeOfDayExposureSettings3D requested{},actual{};uint64_t fingerprint{};float requestedTotalEv{},actualTotalEv{},phaseExposureEv{},shadowLift{},toeStrength{},shoulderStrength{};VolumetricAtmospherePhase3D phase{VolumetricAtmospherePhase3D::Day};TimeOfDayExposureApplyReason3D reason{TimeOfDayExposureApplyReason3D::Disabled};bool applied{},rolledBack{};};
struct SoftDirectionalShadowSettings3D {bool enabled{true};float blockerSearchRadiusTexels{24.f},lightSourceRadiusTexels{128.f},maximumPenumbraRadiusTexels{32.f};};
inline SoftDirectionalShadowSettings3D NormalizeSoftDirectionalShadowSettings3D(SoftDirectionalShadowSettings3D settings){settings.blockerSearchRadiusTexels=Clamp(std::isfinite(settings.blockerSearchRadiusTexels)?settings.blockerSearchRadiusTexels:24.f,1.f,24.f);settings.lightSourceRadiusTexels=Clamp(std::isfinite(settings.lightSourceRadiusTexels)?settings.lightSourceRadiusTexels:128.f,1.f,128.f);settings.maximumPenumbraRadiusTexels=Clamp(std::isfinite(settings.maximumPenumbraRadiusTexels)?settings.maximumPenumbraRadiusTexels:32.f,1.45f,32.f);return settings;}
inline float DirectionalShadowPenumbraRadius3D(float receiverDepth,float averageBlockerDepth,const SoftDirectionalShadowSettings3D& source={}){const SoftDirectionalShadowSettings3D settings=NormalizeSoftDirectionalShadowSettings3D(source);if(!settings.enabled||!std::isfinite(receiverDepth)||!std::isfinite(averageBlockerDepth)||receiverDepth<=averageBlockerDepth||averageBlockerDepth<=0.f)return 1.45f;const float ratio=(receiverDepth-averageBlockerDepth)/(std::max)(averageBlockerDepth,.0001f);return Clamp(ratio*settings.lightSourceRadiusTexels,1.45f,settings.maximumPenumbraRadiusTexels);}
struct DirectionalShadowCascadeSettings3D {uint32_t cascadeCount{4},resolution{2048};float nearDistance{.1f},farDistance{180.f},splitLambda{.65f},transitionFraction{.08f};friend constexpr bool operator==(const DirectionalShadowCascadeSettings3D&,const DirectionalShadowCascadeSettings3D&)=default;};
inline DirectionalShadowCascadeSettings3D NormalizeDirectionalShadowCascadeSettings3D(DirectionalShadowCascadeSettings3D value){value.cascadeCount=(std::clamp)(value.cascadeCount,1u,MaximumDirectionalShadowCascades3D);value.resolution=(std::clamp)(value.resolution,512u,4096u);value.nearDistance=Clamp(std::isfinite(value.nearDistance)?value.nearDistance:.1f,.01f,10.f);value.farDistance=Clamp(std::isfinite(value.farDistance)?value.farDistance:180.f,value.nearDistance+.01f,2000.f);value.splitLambda=Clamp(std::isfinite(value.splitLambda)?value.splitLambda:.65f,0.f,1.f);value.transitionFraction=Clamp(std::isfinite(value.transitionFraction)?value.transitionFraction:.08f,0.f,.25f);return value;}
inline std::array<float,MaximumDirectionalShadowCascades3D> DirectionalShadowPracticalSplits3D(DirectionalShadowCascadeSettings3D source={}){const auto value=NormalizeDirectionalShadowCascadeSettings3D(source);std::array<float,MaximumDirectionalShadowCascades3D> result{};for(uint32_t index=1;index<=value.cascadeCount;++index){const float ratio=float(index)/float(value.cascadeCount),logarithmic=value.nearDistance*std::pow(value.farDistance/value.nearDistance,ratio),uniform=value.nearDistance+(value.farDistance-value.nearDistance)*ratio;result[index-1]=index==value.cascadeCount?value.farDistance:std::lerp(uniform,logarithmic,value.splitLambda);}for(uint32_t index=value.cascadeCount;index<MaximumDirectionalShadowCascades3D;++index)result[index]=value.farDistance;return result;}
inline uint32_t SelectDirectionalShadowCascade3D(float cameraDistance,const std::array<float,MaximumDirectionalShadowCascades3D>& splits,uint32_t cascadeCount){if(!std::isfinite(cameraDistance))return 0;cascadeCount=(std::clamp)(cascadeCount,1u,MaximumDirectionalShadowCascades3D);for(uint32_t index=0;index+1<cascadeCount;++index)if(cameraDistance<splits[index])return index;return cascadeCount-1;}
inline float DirectionalShadowLinearDepth3D(const Matrix4& viewProjection,Vec3 worldPosition,Vec3 cameraPosition){Vec3 axis{viewProjection[3][0],viewProjection[3][1],viewProjection[3][2]};if(Length(axis)<1e-6f)axis={viewProjection[2][0],viewProjection[2][1],viewProjection[2][2]};const float length=Length(axis);if(length<1e-6f||!std::isfinite(length))return 0.f;return std::abs(Dot(worldPosition-cameraPosition,Normalize(axis)));}
inline uint8_t DirectionalShadowDepthBandMask3D(float centerDepth,float casterReach,const std::array<float,MaximumDirectionalShadowCascades3D>& splits,DirectionalShadowCascadeSettings3D source={}){const auto settings=NormalizeDirectionalShadowCascadeSettings3D(source);if(!std::isfinite(centerDepth)||!std::isfinite(casterReach)||centerDepth<0.f||casterReach<0.f)return DirectionalShadowAllCascades3D;uint8_t mask{};for(uint32_t cascade=0;cascade<settings.cascadeCount;++cascade){const float minimum=cascade? splits[cascade-1]:settings.nearDistance,maximum=splits[cascade],blend=(maximum-minimum)*settings.transitionFraction;if(centerDepth+casterReach>=minimum-blend&&centerDepth-casterReach<=maximum+blend)mask|=uint8_t(1u<<cascade);}return mask?mask:uint8_t(1u<<SelectDirectionalShadowCascade3D(centerDepth,splits,settings.cascadeCount));}
struct DirectionalShadowCascadeFit3D {
    std::array<Matrix4,MaximumDirectionalShadowCascades3D> lightViewProjections{};
    std::array<std::array<Vec3,8>,MaximumDirectionalShadowCascades3D> sliceCorners{};
    std::array<float,MaximumDirectionalShadowCascades3D> splits{};
    bool valid{},fallback{},reversedDepth{};
};
DirectionalShadowCascadeFit3D FitDirectionalShadowCascades3D(const Matrix4& viewProjection,Vec3 cameraPosition,Vec3 lightDirection,DirectionalShadowCascadeSettings3D settings={},float casterDepthPadding=32.f,float receiverDepthPadding=4.f);
struct ContactShadowSettings3D {bool enabled{true};uint32_t maximumSteps{12};float maximumDistance{1.5f},thickness{.04f},strength{.8f};friend constexpr bool operator==(const ContactShadowSettings3D&,const ContactShadowSettings3D&)=default;};
inline ContactShadowSettings3D NormalizeContactShadowSettings3D(ContactShadowSettings3D value){value.maximumSteps=(std::clamp)(value.maximumSteps,1u,24u);value.maximumDistance=Clamp(std::isfinite(value.maximumDistance)?value.maximumDistance:1.5f,.05f,5.f);value.thickness=Clamp(std::isfinite(value.thickness)?value.thickness:.04f,.001f,.25f);value.strength=Clamp(std::isfinite(value.strength)?value.strength:.8f,0.f,1.f);return value;}
enum class ContactShadowTemporalReset3D:uint32_t {None=0,CameraCut=1,Resize=2,Quality=4,HistoryInvalid=8,ReprojectionInvalid=16,DepthRejected=32,MotionRejected=64,Disabled=128};
struct ContactShadowTemporalConstants3D {float historyWeight{.85f},depthThreshold{.1f},motionThresholdPixels{24.f},darkeningHistoryWeight{.65f};uint32_t historyValid{},cameraCut{},resized{},qualityChanged{},outputWidth{},outputHeight{};float inverseOutputWidth{},inverseOutputHeight{};Vec2 currentJitterPixels{},previousJitterPixels{};};
static_assert(sizeof(ContactShadowTemporalConstants3D)==64);
struct ContactShadowTemporalSample3D {float currentVisibility{1.f},historyVisibility{1.f},currentDepth{1.f},historyDepth{1.f},motionPixels{};bool reprojectionValid{true};};
struct ContactShadowTemporalResult3D {float visibility{1.f};ContactShadowTemporalReset3D rejection{ContactShadowTemporalReset3D::None};bool historyAccepted{};};
[[nodiscard]] inline ContactShadowTemporalResult3D EvaluateContactShadowTemporal3D(ContactShadowTemporalSample3D sample,ContactShadowTemporalConstants3D constants,bool enabled=true)noexcept{
    const float currentVisibility=Clamp(std::isfinite(sample.currentVisibility)?sample.currentVisibility:1.f,0.f,1.f);
    const auto reject=[&](ContactShadowTemporalReset3D reason){return ContactShadowTemporalResult3D{currentVisibility,reason,false};};
    if(!enabled)return reject(ContactShadowTemporalReset3D::Disabled);if(constants.cameraCut)return reject(ContactShadowTemporalReset3D::CameraCut);if(constants.resized)return reject(ContactShadowTemporalReset3D::Resize);if(constants.qualityChanged)return reject(ContactShadowTemporalReset3D::Quality);if(!constants.historyValid)return reject(ContactShadowTemporalReset3D::HistoryInvalid);if(!sample.reprojectionValid)return reject(ContactShadowTemporalReset3D::ReprojectionInvalid);
    if(!std::isfinite(sample.currentVisibility)||!std::isfinite(sample.historyVisibility))return reject(ContactShadowTemporalReset3D::ReprojectionInvalid);
    const float depthThreshold=Clamp(std::isfinite(constants.depthThreshold)?constants.depthThreshold:.01f,0.f,1.f);if(!std::isfinite(sample.currentDepth)||!std::isfinite(sample.historyDepth)||std::abs(sample.currentDepth-sample.historyDepth)>depthThreshold)return reject(ContactShadowTemporalReset3D::DepthRejected);
    const float motionThreshold=(std::max)(0.f,std::isfinite(constants.motionThresholdPixels)?constants.motionThresholdPixels:24.f);if(!std::isfinite(sample.motionPixels)||std::abs(sample.motionPixels)>motionThreshold)return reject(ContactShadowTemporalReset3D::MotionRejected);
    const float historyWeight=Clamp(std::isfinite(constants.historyWeight)?constants.historyWeight:.85f,0.f,.98f),history=Clamp(sample.historyVisibility,0.f,1.f);const float darkeningHistoryWeight=Clamp(std::isfinite(constants.darkeningHistoryWeight)?constants.darkeningHistoryWeight:.65f,0.f,historyWeight);const float effectiveHistoryWeight=currentVisibility<history?darkeningHistoryWeight:historyWeight;return {currentVisibility+(history-currentVisibility)*effectiveHistoryWeight,ContactShadowTemporalReset3D::None,true};
}
constexpr uint32_t SelectDirectionalShadowCascade3D(float cameraDistance,float splitDistance=32.f){return std::isfinite(cameraDistance)&&cameraDistance>=splitDistance?1u:0u;}
inline float SnapDirectionalShadowCoordinate3D(float value,float texelWorldSize){return std::isfinite(value)&&std::isfinite(texelWorldSize)&&texelWorldSize>0.f?std::round(value/texelWorldSize)*texelWorldSize:value;}
inline float DirectionalShadowCascadeBlend3D(float cameraDistance,float splitDistance=32.f,float transitionWidth=6.f){if(!std::isfinite(cameraDistance)||!std::isfinite(splitDistance)||!std::isfinite(transitionWidth)||transitionWidth<=0.f)return cameraDistance>=splitDistance?1.f:0.f;const float value=Clamp((cameraDistance-(splitDistance-transitionWidth*.5f))/transitionWidth,0.f,1.f);return value*value*(3.f-2.f*value);}
inline uint8_t DirectionalShadowCasterCascadeMask3D(Vec3 center,float radius,Vec3 cameraPosition,Vec3 lightDirection,float splitDistance=32.f,float transitionWidth=6.f){
    // 以包圍球加上光線水平投影估算陰影可到達範圍；資料不可靠時保守保留兩級聯。
    if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)||!std::isfinite(radius)||radius<0.f||!std::isfinite(cameraPosition.x)||!std::isfinite(cameraPosition.y)||!std::isfinite(cameraPosition.z)||!std::isfinite(lightDirection.x)||!std::isfinite(lightDirection.y)||!std::isfinite(lightDirection.z)||!std::isfinite(splitDistance)||splitDistance<=0.f||!std::isfinite(transitionWidth)||transitionWidth<0.f)return DirectionalShadowAllCascades3D;
    lightDirection=Normalize(lightDirection);const float vertical=std::abs(lightDirection.y);if(vertical<.05f)return DirectionalShadowAllCascades3D;
    const float horizontal=std::sqrt(lightDirection.x*lightDirection.x+lightDirection.z*lightDirection.z),shadowReach=radius*(1.f+horizontal/vertical),distance=Length(center-cameraPosition),halfTransition=transitionWidth*.5f;
    if(!std::isfinite(shadowReach)||!std::isfinite(distance))return DirectionalShadowAllCascades3D;
    uint8_t mask{};if(distance-shadowReach<=splitDistance+halfTransition)mask|=DirectionalShadowNearCascadeBit3D;if(distance+shadowReach>=splitDistance-halfTransition)mask|=DirectionalShadowFarCascadeBit3D;return mask;
}
inline float RayleighPhase3D(float cosine){cosine=Clamp(cosine,-1.f,1.f);return 3.f*(1.f+cosine*cosine)/(16.f*3.14159265359f);}
inline float MiePhase3D(float cosine,float anisotropy=.76f){cosine=Clamp(cosine,-1.f,1.f);anisotropy=Clamp(anisotropy,-.8f,.85f);const float squared=anisotropy*anisotropy,denominator=(std::max)(1e-6f,1.f+squared-2.f*anisotropy*cosine);return(1.f-squared)/(4.f*3.14159265359f*std::pow(denominator,1.5f));}
inline float WaterFresnel3D(float viewNormalCosine,float reflectance=.02f){const float inverse=1.f-Clamp(viewNormalCosine,0.f,1.f),inverse2=inverse*inverse;return Clamp(reflectance+(1.f-reflectance)*inverse2*inverse2*inverse,0.f,1.f);}
inline float WaterTransmission3D(float absorption,float pathLength){if(!std::isfinite(absorption)||!std::isfinite(pathLength))return 0.f;return std::exp(-(std::max)(0.f,absorption)*(std::max)(0.f,pathLength));}
inline float WaterVerticalShoreDepth3D(float surfaceHeight,float bottomHeight){if(!std::isfinite(surfaceHeight)||!std::isfinite(bottomHeight))return 0.f;return Clamp(surfaceHeight-bottomHeight,0.f,8.f);}
inline float WaterShoreCoverage3D(float verticalDepth,float derivativeWidth,float startDepth,float endDepth){if(!std::isfinite(verticalDepth)||!std::isfinite(derivativeWidth)||!std::isfinite(startDepth)||!std::isfinite(endDepth))return 0.f;verticalDepth=Clamp(verticalDepth,0.f,8.f);startDepth=Clamp(startDepth,0.f,4.f);endDepth=Clamp(endDepth,startDepth+.001f,8.f);const float maximumDerivative=(std::max)(.002f,(std::min)(.08f,(endDepth-startDepth)*.25f));derivativeWidth=Clamp(std::abs(derivativeWidth)*1.5f,.002f,maximumDerivative);const float low=startDepth-derivativeWidth,high=endDepth+derivativeWidth,t=Clamp((verticalDepth-low)/(high-low),0.f,1.f);return 1.f-t*t*(3.f-2.f*t);}
struct WaterOpticsSettings3D {bool screenSpaceRefraction{true};float refractionStrength{.018f},depthScale{.35f},maximumUvOffset{.035f};};
inline WaterOpticsSettings3D NormalizeWaterOpticsSettings3D(WaterOpticsSettings3D settings){settings.refractionStrength=Clamp(std::isfinite(settings.refractionStrength)?settings.refractionStrength:.018f,0.f,.12f);settings.depthScale=Clamp(std::isfinite(settings.depthScale)?settings.depthScale:.35f,0.f,4.f);settings.maximumUvOffset=Clamp(std::isfinite(settings.maximumUvOffset)?settings.maximumUvOffset:.035f,0.f,.2f);return settings;}
inline Vec2 WaterRefractionOffset3D(Vec2 surfaceSlope,float thickness,const WaterOpticsSettings3D& source={}){const WaterOpticsSettings3D settings=NormalizeWaterOpticsSettings3D(source);if(!settings.screenSpaceRefraction||!std::isfinite(surfaceSlope.x)||!std::isfinite(surfaceSlope.y)||!std::isfinite(thickness))return{};const float scale=(std::min)(settings.maximumUvOffset,settings.refractionStrength*Clamp(thickness*settings.depthScale,0.f,1.f));return{Clamp(surfaceSlope.x*scale,-settings.maximumUvOffset,settings.maximumUvOffset),Clamp(surfaceSlope.y*scale,-settings.maximumUvOffset,settings.maximumUvOffset)};}
struct WaterSurfaceSettings3D {
    bool enabled{true};
    Vec3 absorption{.42f,.16f,.08f},deepColor{.012f,.055f,.07f};
    float reflectance{.0203732f},minimumRoughness{.045f},maximumRoughness{.18f};
    float broadWaveScale{.38f},broadWaveStrength{.058f},broadWaveSpeed{.72f};
    float detailWaveScale{1.73f},detailWaveStrength{.027f},detailWaveSpeed{1.91f};
    float foamStartDepth{.06f},foamEndDepth{.72f},foamStrength{.88f};
    float shallowAlpha{.34f},deepAlpha{.91f},maximumNightRadiance{1.25f};
};
inline WaterSurfaceSettings3D NormalizeWaterSurfaceSettings3D(WaterSurfaceSettings3D s){
    const auto finite=[](float value,float fallback,float minimum,float maximum){return Clamp(std::isfinite(value)?value:fallback,minimum,maximum);};
    s.absorption={finite(s.absorption.x,.42f,0.f,4.f),finite(s.absorption.y,.16f,0.f,4.f),finite(s.absorption.z,.08f,0.f,4.f)};
    s.deepColor={finite(s.deepColor.x,.012f,0.f,4.f),finite(s.deepColor.y,.055f,0.f,4.f),finite(s.deepColor.z,.07f,0.f,4.f)};
    s.reflectance=finite(s.reflectance,.0203732f,.005f,.12f);s.minimumRoughness=finite(s.minimumRoughness,.045f,.02f,.6f);s.maximumRoughness=finite(s.maximumRoughness,.18f,s.minimumRoughness,1.f);
    s.broadWaveScale=finite(s.broadWaveScale,.38f,.001f,8.f);s.broadWaveStrength=finite(s.broadWaveStrength,.058f,0.f,.3f);s.broadWaveSpeed=finite(s.broadWaveSpeed,.72f,-8.f,8.f);
    s.detailWaveScale=finite(s.detailWaveScale,1.73f,.001f,16.f);s.detailWaveStrength=finite(s.detailWaveStrength,.027f,0.f,.2f);s.detailWaveSpeed=finite(s.detailWaveSpeed,1.91f,-16.f,16.f);
    s.foamStartDepth=finite(s.foamStartDepth,.06f,0.f,4.f);s.foamEndDepth=finite(s.foamEndDepth,.72f,s.foamStartDepth+.001f,8.f);s.foamStrength=finite(s.foamStrength,.88f,0.f,1.f);
    s.shallowAlpha=finite(s.shallowAlpha,.34f,0.f,1.f);s.deepAlpha=finite(s.deepAlpha,.91f,s.shallowAlpha,1.f);s.maximumNightRadiance=finite(s.maximumNightRadiance,1.25f,.005f,8.f);return s;
}
inline uint64_t WaterSurfaceFingerprint3D(const WaterSurfaceSettings3D& source){const auto s=NormalizeWaterSurfaceSettings3D(source);uint64_t hash=14695981039346656037ull;const auto mix=[&](uint32_t value){for(int i=0;i<4;++i){hash^=(value>>(i*8))&255u;hash*=1099511628211ull;}};mix(s.enabled?1u:0u);for(float value:{s.absorption.x,s.absorption.y,s.absorption.z,s.deepColor.x,s.deepColor.y,s.deepColor.z,s.reflectance,s.minimumRoughness,s.maximumRoughness,s.broadWaveScale,s.broadWaveStrength,s.broadWaveSpeed,s.detailWaveScale,s.detailWaveStrength,s.detailWaveSpeed,s.foamStartDepth,s.foamEndDepth,s.foamStrength,s.shallowAlpha,s.deepAlpha,s.maximumNightRadiance})mix(std::bit_cast<uint32_t>(value));return hash;}
struct WaterSurfaceReceipt3D {WaterSurfaceSettings3D normalizedSettings{};uint64_t fingerprint{};bool normalized{},enabledRequested{},singleCompositeRequested{};};
struct ScreenSpaceReflectionSettings3D {bool enabled{true};float intensity{.82f},maximumDistance{36.f},thickness{.65f},maximumRoughness{.28f};uint32_t sampleCount{12};friend constexpr bool operator==(const ScreenSpaceReflectionSettings3D&,const ScreenSpaceReflectionSettings3D&)=default;};
inline ScreenSpaceReflectionSettings3D NormalizeScreenSpaceReflectionSettings3D(ScreenSpaceReflectionSettings3D settings){settings.intensity=Clamp(std::isfinite(settings.intensity)?settings.intensity:.82f,0.f,1.f);settings.maximumDistance=Clamp(std::isfinite(settings.maximumDistance)?settings.maximumDistance:36.f,1.f,160.f);settings.thickness=Clamp(std::isfinite(settings.thickness)?settings.thickness:.65f,.02f,4.f);settings.maximumRoughness=Clamp(std::isfinite(settings.maximumRoughness)?settings.maximumRoughness:.28f,.05f,1.f);settings.sampleCount=(std::clamp)(settings.sampleCount,4u,16u);return settings;}
inline float ScreenSpaceReflectionStepDistance3D(uint32_t step,const ScreenSpaceReflectionSettings3D& source={}){const auto settings=NormalizeScreenSpaceReflectionSettings3D(source);if(!settings.enabled||settings.intensity<=0.f||step>=settings.sampleCount)return 0.f;const float fraction=float(step+1)/float(settings.sampleCount),minimum=(std::min)(.18f,settings.maximumDistance*.02f);return minimum+(settings.maximumDistance-minimum)*fraction*fraction;}
inline float ScreenSpaceReflectionConfidence3D(float depthGap,float viewportEdgeFade,float roughness,const ScreenSpaceReflectionSettings3D& source={}){const auto settings=NormalizeScreenSpaceReflectionSettings3D(source);if(!settings.enabled||settings.intensity<=0.f||!std::isfinite(depthGap)||!std::isfinite(viewportEdgeFade)||!std::isfinite(roughness)||depthGap<0.f||depthGap>settings.thickness||roughness>=settings.maximumRoughness)return 0.f;const float depth=1.f-depthGap/settings.thickness,edge=Clamp(viewportEdgeFade,0.f,1.f),smooth=1.f-Clamp(roughness/settings.maximumRoughness,0.f,1.f);return Clamp(depth*edge*smooth*settings.intensity,0.f,1.f);}
// 僅篩選局部反射的接收材質，不改全域環境或既有探針／DrawList 的資料布局。
enum class LocalReflectionMaterialMask3D:uint8_t {None=0,Surface=1,Water=2,Foliage=4,Terrain=8,All=15};
constexpr bool ValidLocalReflectionMaterialMask3D(LocalReflectionMaterialMask3D mask){return (uint8_t(mask)&~uint8_t(LocalReflectionMaterialMask3D::All))==0;}
constexpr bool LocalReflectionMaterialAllowed3D(LocalReflectionMaterialMask3D mask,MaterialClass3D material){return ValidLocalReflectionMaterialMask3D(mask)&&uint8_t(material)<=uint8_t(MaterialClass3D::Terrain)&&(uint8_t(mask)&(1u<<uint8_t(material)))!=0;}
struct LocalReflectionProbe3D {EnvironmentLightingHandle3D environment{};Vec3 minimum{-1,-1,-1},maximum{1,1,1},capturePosition{};int32_t priority{};};
inline bool ValidLocalReflectionProbe3D(const LocalReflectionProbe3D& probe){return probe.environment&&std::isfinite(probe.minimum.x)&&std::isfinite(probe.minimum.y)&&std::isfinite(probe.minimum.z)&&std::isfinite(probe.maximum.x)&&std::isfinite(probe.maximum.y)&&std::isfinite(probe.maximum.z)&&std::isfinite(probe.capturePosition.x)&&std::isfinite(probe.capturePosition.y)&&std::isfinite(probe.capturePosition.z)&&probe.minimum.x<probe.maximum.x&&probe.minimum.y<probe.maximum.y&&probe.minimum.z<probe.maximum.z;}
inline size_t SelectLocalReflectionProbe3D(Vec3 point,std::span<const LocalReflectionProbe3D> probes){if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return SIZE_MAX;size_t selected=SIZE_MAX;const auto better=[](const LocalReflectionProbe3D& left,const LocalReflectionProbe3D& right){if(left.priority!=right.priority)return left.priority>right.priority;const Vec3 ls=left.maximum-left.minimum,rs=right.maximum-right.minimum;const double lv=double(ls.x)*ls.y*ls.z,rv=double(rs.x)*rs.y*rs.z;if(lv!=rv)return lv<rv;if(left.environment.value!=right.environment.value)return left.environment.value<right.environment.value;for(const auto pair:{std::pair{left.minimum.x,right.minimum.x},std::pair{left.minimum.y,right.minimum.y},std::pair{left.minimum.z,right.minimum.z},std::pair{left.maximum.x,right.maximum.x},std::pair{left.maximum.y,right.maximum.y},std::pair{left.maximum.z,right.maximum.z}})if(pair.first!=pair.second)return pair.first<pair.second;return false;};for(size_t index=0;index<probes.size();++index){const auto& probe=probes[index];if(!ValidLocalReflectionProbe3D(probe)||point.x<probe.minimum.x||point.y<probe.minimum.y||point.z<probe.minimum.z||point.x>probe.maximum.x||point.y>probe.maximum.y||point.z>probe.maximum.z)continue;if(selected==SIZE_MAX||better(probe,probes[selected]))selected=index;}return selected;}
inline size_t SelectLocalReflectionProbeForMaterial3D(Vec3 point,std::span<const LocalReflectionProbe3D> probes,std::span<const LocalReflectionMaterialMask3D> masks,MaterialClass3D material){
    if(masks.size()!=probes.size())return SIZE_MAX;
    size_t selected=SIZE_MAX;
    for(size_t i=0;i<probes.size();++i){
        if(!LocalReflectionMaterialAllowed3D(masks[i],material)||SelectLocalReflectionProbe3D(point,probes.subspan(i,1))==SIZE_MAX)continue;
        if(selected==SIZE_MAX){selected=i;continue;}
        const std::array pair{probes[selected],probes[i]};
        if(SelectLocalReflectionProbe3D(point,pair)==1)selected=i;
    }
    return selected;
}
inline Vec3 BoxProjectedReflectionDirection3D(Vec3 worldPosition,Vec3 reflectionDirection,const LocalReflectionProbe3D& probe){if(!ValidLocalReflectionProbe3D(probe)||!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y)||!std::isfinite(worldPosition.z)||!std::isfinite(reflectionDirection.x)||!std::isfinite(reflectionDirection.y)||!std::isfinite(reflectionDirection.z))return{};reflectionDirection=Normalize(reflectionDirection);if(Length(reflectionDirection)<.000001f)return{};float hitDistance=(std::numeric_limits<float>::max)();for(size_t axis=0;axis<3;++axis){const float direction=axis==0?reflectionDirection.x:axis==1?reflectionDirection.y:reflectionDirection.z,position=axis==0?worldPosition.x:axis==1?worldPosition.y:worldPosition.z,bound=direction>=0.f?(axis==0?probe.maximum.x:axis==1?probe.maximum.y:probe.maximum.z):(axis==0?probe.minimum.x:axis==1?probe.minimum.y:probe.minimum.z);if(std::abs(direction)>.000001f){const float distance=(bound-position)/direction;if(distance>=0.f)hitDistance=(std::min)(hitDistance,distance);}}if(!std::isfinite(hitDistance)||hitDistance==(std::numeric_limits<float>::max)())return reflectionDirection;const Vec3 corrected=Normalize(worldPosition+reflectionDirection*hitDistance-probe.capturePosition);return Length(corrected)>.000001f?corrected:reflectionDirection;}
struct ReflectionProbeCaptureHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}friend constexpr bool operator==(ReflectionProbeCaptureHandle3D,ReflectionProbeCaptureHandle3D)=default;};
enum class ReflectionProbeCaptureStage3D:uint8_t {Dirty,Pending,ReadyRawUnfiltered,ReadyFiltered,BudgetRejected};
struct ReflectionProbeCaptureSettings3D {Vec3 position{};uint32_t resolution{128};float nearPlane{.1f},farPlane{200.f};};
inline bool ValidReflectionProbeCaptureSettings3D(const ReflectionProbeCaptureSettings3D& value){return std::isfinite(value.position.x)&&std::isfinite(value.position.y)&&std::isfinite(value.position.z)&&std::isfinite(value.nearPlane)&&std::isfinite(value.farPlane)&&value.nearPlane>=.01f&&value.farPlane>value.nearPlane&&value.resolution>=32&&value.resolution<=512&&(value.resolution&(value.resolution-1))==0;}
struct ReflectionProbeCaptureStatus3D {ReflectionProbeCaptureStage3D stage{ReflectionProbeCaptureStage3D::Dirty};uint32_t resolution{},specularMipCount{};size_t residentBytes{};uint64_t generation{},pendingFence{};EnvironmentLightingHandle3D environment{};bool requiresPrefilter{true};uint64_t originGeneration{},pendingOriginGeneration{};bool requeueAfterFence{};};
enum class ReflectionProbeCaptureFace3D:uint8_t {PositiveX,NegativeX,PositiveY,NegativeY,PositiveZ,NegativeZ};
struct ReflectionProbeCaptureReadback3D {uint32_t resolution{},specularMipCount{};uint64_t generation{};std::array<uint64_t,6> faceHashes{},irradianceFaceHashes{};std::vector<uint64_t> specularMipFaceHashes;std::array<double,6> irradianceFaceEnergy{};std::vector<double> specularMipFaceEnergy;};
// 有界近水平視差；不改世界頂點或碰撞，所有不合法要求回到關閉狀態。
struct TerrainParallaxSettings3D {
    bool enabled{false};
    float depthMeters{},fadeStart{12.f},fadeEnd{18.f},minimumNormalY{.96f},minimumViewCosine{.15f},maximumUvOffset{.08f};
    friend constexpr bool operator==(const TerrainParallaxSettings3D&,const TerrainParallaxSettings3D&)=default;
};
inline bool ValidTerrainParallaxSettings3D(const TerrainParallaxSettings3D& s){
    for(float v:{s.depthMeters,s.fadeStart,s.fadeEnd,s.minimumNormalY,s.minimumViewCosine,s.maximumUvOffset})if(!std::isfinite(v))return false;
    return s.depthMeters>=0.f&&s.depthMeters<=.02f&&s.fadeStart>=0.f&&s.fadeStart<=100.f&&s.fadeEnd>s.fadeStart&&s.fadeEnd<=120.f&&s.minimumNormalY>=.9f&&s.minimumNormalY<=1.f&&s.minimumViewCosine>=.05f&&s.minimumViewCosine<=.8f&&s.maximumUvOffset>=.001f&&s.maximumUvOffset<=.1f&&(!s.enabled||s.depthMeters>0.f);
}
inline TerrainParallaxSettings3D NormalizeTerrainParallaxSettings3D(TerrainParallaxSettings3D s){return ValidTerrainParallaxSettings3D(s)&&s.enabled?s:TerrainParallaxSettings3D{};}
inline std::uint64_t FingerprintTerrainParallaxSettings3D(TerrainParallaxSettings3D s){
    s=NormalizeTerrainParallaxSettings3D(s);if(!s.enabled)return 0;
    std::uint64_t hash=14695981039346656037ull;
    for(float value:{s.depthMeters,s.fadeStart,s.fadeEnd,s.minimumNormalY,s.minimumViewCosine,s.maximumUvOffset})for(unsigned b=0;b<4;++b){hash^=(std::bit_cast<std::uint32_t>(value)>>(8*b))&255u;hash*=1099511628211ull;}
    return hash;
}
struct TerrainDistanceMaterialSettings3D {
    bool enabled{true};
    float detailFadeStart{48.f},detailFadeEnd{192.f},macroVariationScale{.05f},macroVariationStrength{.24f},normalDetailStrength{1.f},farRoughnessBoost{.12f};
    bool decorrelationEnabled{true};
    float warpScale{.012f},warpStrength{2.4f},grassLayerScale{.92f},soilLayerScale{1.21f},rockLayerScale{.68f},chromaVariationStrength{.075f};
    bool mesoPartitionEnabled{false};
    bool rockVariationEnabled{false};
    friend constexpr bool operator==(const TerrainDistanceMaterialSettings3D&,const TerrainDistanceMaterialSettings3D&)=default;
};
inline TerrainDistanceMaterialSettings3D NormalizeTerrainDistanceMaterialSettings3D(TerrainDistanceMaterialSettings3D settings){settings.detailFadeStart=Clamp(std::isfinite(settings.detailFadeStart)?settings.detailFadeStart:48.f,0.f,100000.f);settings.detailFadeEnd=Clamp(std::isfinite(settings.detailFadeEnd)?settings.detailFadeEnd:192.f,settings.detailFadeStart+.01f,100001.f);settings.macroVariationScale=Clamp(std::isfinite(settings.macroVariationScale)?settings.macroVariationScale:.05f,.00001f,4.f);settings.macroVariationStrength=Clamp(std::isfinite(settings.macroVariationStrength)?settings.macroVariationStrength:.24f,0.f,.5f);settings.normalDetailStrength=Clamp(std::isfinite(settings.normalDetailStrength)?settings.normalDetailStrength:1.f,0.f,2.f);settings.farRoughnessBoost=Clamp(std::isfinite(settings.farRoughnessBoost)?settings.farRoughnessBoost:.12f,0.f,.5f);settings.warpScale=Clamp(std::isfinite(settings.warpScale)?settings.warpScale:.012f,.0001f,.05f);settings.warpStrength=Clamp(std::isfinite(settings.warpStrength)?settings.warpStrength:2.4f,0.f,8.f);settings.grassLayerScale=Clamp(std::isfinite(settings.grassLayerScale)?settings.grassLayerScale:.92f,.5f,1.75f);settings.soilLayerScale=Clamp(std::isfinite(settings.soilLayerScale)?settings.soilLayerScale:1.21f,.5f,1.75f);settings.rockLayerScale=Clamp(std::isfinite(settings.rockLayerScale)?settings.rockLayerScale:.68f,.5f,1.75f);settings.chromaVariationStrength=Clamp(std::isfinite(settings.chromaVariationStrength)?settings.chromaVariationStrength:.075f,0.f,.2f);return settings;}
// 沿用既有 root constant：0 關閉、1 原路徑、2 啟用獨立岩層取樣變化。
inline float TerrainDistanceMaterialMode3D(const TerrainDistanceMaterialSettings3D& settings){return settings.enabled?(settings.rockVariationEnabled&&settings.decorrelationEnabled?2.f:1.f):0.f;}
inline std::uint32_t PackTerrainDecorrelationSettings3D(const TerrainDistanceMaterialSettings3D& source={}){
    const auto settings=NormalizeTerrainDistanceMaterialSettings3D(source);if(!settings.enabled)return 0;
    const auto encode=[](float value,float minimum,float maximum){return std::uint32_t(std::lround(Clamp((value-minimum)/(maximum-minimum),0.f,1.f)*31.f));};
    std::uint32_t packed=settings.mesoPartitionEnabled?(1u<<31):0u;
    if(settings.decorrelationEnabled)packed|=encode(settings.warpScale,.0001f,.05f)|(encode(settings.warpStrength,0.f,8.f)<<5)|(encode(settings.grassLayerScale,.5f,1.75f)<<10)|(encode(settings.soilLayerScale,.5f,1.75f)<<15)|(encode(settings.rockLayerScale,.5f,1.75f)<<20)|(encode(settings.chromaVariationStrength,0.f,.2f)<<25)|(1u<<30);
    return packed;
}
inline TerrainDistanceMaterialSettings3D EffectiveTerrainDecorrelationSettings3D(const TerrainDistanceMaterialSettings3D& source={}){
    auto settings=NormalizeTerrainDistanceMaterialSettings3D(source);const std::uint32_t packed=PackTerrainDecorrelationSettings3D(settings);settings.decorrelationEnabled=(packed&(1u<<30))!=0;settings.mesoPartitionEnabled=(packed&(1u<<31))!=0;if(!settings.decorrelationEnabled)return settings;const auto decode=[&](unsigned shift,float minimum,float maximum){return minimum+(maximum-minimum)*(float((packed>>shift)&31u)/31.f);};settings.warpScale=decode(0,.0001f,.05f);settings.warpStrength=decode(5,0.f,8.f);settings.grassLayerScale=decode(10,.5f,1.75f);settings.soilLayerScale=decode(15,.5f,1.75f);settings.rockLayerScale=decode(20,.5f,1.75f);settings.chromaVariationStrength=decode(25,0.f,.2f);return settings;
}
inline std::uint64_t FingerprintTerrainDistanceMaterialSettings3D(const TerrainDistanceMaterialSettings3D& source={}){
    const auto settings=NormalizeTerrainDistanceMaterialSettings3D(source);std::uint64_t hash=1469598103934665603ull;const auto mix=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};mix(std::uint32_t(TerrainDistanceMaterialMode3D(settings)));if(settings.enabled){for(float value:{settings.detailFadeStart,settings.detailFadeEnd,settings.macroVariationScale,settings.macroVariationStrength,settings.normalDetailStrength,settings.farRoughnessBoost})mix(std::bit_cast<std::uint32_t>(value==0.f?0.f:value));mix(PackTerrainDecorrelationSettings3D(settings));}return hash;
}
inline Vec3 TerrainDecorrelatedWorldPosition3D(Vec3 worldPosition,const TerrainDistanceMaterialSettings3D& source={}){
    const auto settings=EffectiveTerrainDecorrelationSettings3D(source);if(!settings.enabled||!settings.decorrelationEnabled||settings.warpStrength<=0.f)return worldPosition;if(!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y)||!std::isfinite(worldPosition.z))return{};
    const Vec3 phase{(worldPosition.x*1.731f+worldPosition.z*.917f)*settings.warpScale,(worldPosition.y*1.423f+worldPosition.x*.611f)*settings.warpScale,(worldPosition.z*1.277f-worldPosition.y*.743f)*settings.warpScale};
    const Vec3 broad{std::sin(phase.x),std::sin(phase.y+1.7f),std::sin(phase.z-2.1f)},detail{std::sin(phase.y*2.17f+2.3f),std::sin(phase.z*2.17f-1.1f),std::sin(phase.x*2.17f+.7f)};return worldPosition+(broad*.72f+detail*.28f)*settings.warpStrength;
}
inline Vec3 TerrainLayerSamplePosition3D(Vec3 worldPosition,std::uint32_t layer,const TerrainDistanceMaterialSettings3D& source={}){const auto settings=EffectiveTerrainDecorrelationSettings3D(source);const Vec3 warped=TerrainDecorrelatedWorldPosition3D(worldPosition,settings);const float scale=layer==0?settings.grassLayerScale:layer==1?settings.soilLayerScale:settings.rockLayerScale;return settings.enabled&&settings.decorrelationEnabled?warped*scale:worldPosition;}
inline Vec3 TerrainGrassSecondarySamplePosition3D(Vec3 primaryPosition)noexcept{
    if(!std::isfinite(primaryPosition.x)||!std::isfinite(primaryPosition.y)||!std::isfinite(primaryPosition.z))return primaryPosition;
    constexpr float cosine=.829037573f,sine=.559192903f,scale=1.173f;
    return{(primaryPosition.x*cosine-primaryPosition.z*sine)*scale+31.73f,primaryPosition.y*scale-11.29f,(primaryPosition.x*sine+primaryPosition.z*cosine)*scale+47.91f};
}
inline Vec3 TerrainChromaVariation3D(Vec3 albedo,float noiseSample,const TerrainDistanceMaterialSettings3D& source={}){const auto settings=EffectiveTerrainDecorrelationSettings3D(source);if(!settings.enabled||!settings.decorrelationEnabled||settings.chromaVariationStrength<=0.f||!std::isfinite(albedo.x)||!std::isfinite(albedo.y)||!std::isfinite(albedo.z)||!std::isfinite(noiseSample))return albedo;const float amount=(Clamp(noiseSample,0.f,1.f)*2.f-1.f)*settings.chromaVariationStrength;const Vec3 shifted{(std::max)(0.f,albedo.x*(1.f+amount*.92f)),(std::max)(0.f,albedo.y*(1.f-amount*.28f)),(std::max)(0.f,albedo.z*(1.f+amount*.54f))};const auto luma=[](Vec3 value){return value.x*.2126f+value.y*.7152f+value.z*.0722f;};const float before=luma(albedo),after=luma(shifted);return before>0.f&&after>1e-6f?shifted*(before/after):albedo;}
struct TerrainBiomeWeights3D {float grass{},soil{},rock{};friend constexpr bool operator==(const TerrainBiomeWeights3D&,const TerrainBiomeWeights3D&)=default;};
enum class TerrainSecondaryDomainLayer3D:uint8_t {Forest=0,Soil=1,Rock=2,None=3};
struct TerrainSecondaryDomainSelection3D {TerrainSecondaryDomainLayer3D layer{TerrainSecondaryDomainLayer3D::None};float activation{};friend constexpr bool operator==(const TerrainSecondaryDomainSelection3D&,const TerrainSecondaryDomainSelection3D&)=default;};
inline TerrainSecondaryDomainSelection3D SelectTerrainSecondaryDomain3D(TerrainBiomeWeights3D weights,float authoredPathWeight,float authoredWetSoilWeight)noexcept{
    if(!std::isfinite(weights.grass)||!std::isfinite(weights.soil)||!std::isfinite(weights.rock)||!std::isfinite(authoredPathWeight)||!std::isfinite(authoredWetSoilWeight))return{};
    weights.grass=Clamp(weights.grass,0.f,1.f);weights.soil=Clamp(weights.soil,0.f,1.f);weights.rock=Clamp(weights.rock,0.f,1.f);authoredPathWeight=Clamp(authoredPathWeight,0.f,1.f);authoredWetSoilWeight=Clamp(authoredWetSoilWeight,0.f,1.f);
    const auto smooth=[](float value,float minimum,float maximum){const float t=Clamp((value-minimum)/(maximum-minimum),0.f,1.f);return t*t*(3.f-2.f*t);};
    const float authoredWeight=(std::max)(authoredPathWeight,authoredWetSoilWeight),authoredActivation=smooth(authoredWeight,.20f,.55f),neutral=1.f-smooth(authoredWeight,.05f,.20f),forestActivation=smooth(weights.grass,.45f,.72f)*neutral,soilActivation=smooth(weights.soil,.45f,.70f)*neutral,dominance=authoredPathWeight-authoredWetSoilWeight;
    if(dominance>.04f&&authoredActivation>0.f)return{TerrainSecondaryDomainLayer3D::Rock,authoredActivation};
    if(dominance<-.04f&&authoredActivation>0.f)return{TerrainSecondaryDomainLayer3D::Soil,authoredActivation};
    if(weights.soil>weights.grass+.08f&&soilActivation>0.f)return{TerrainSecondaryDomainLayer3D::Soil,soilActivation};
    if(forestActivation>0.f)return{TerrainSecondaryDomainLayer3D::Forest,forestActivation};
    return{};
}
inline float TerrainMesoPartitionSignal3D(Vec2 worldPosition){if(!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y))return .5f;const auto fraction=[](float value){return value-std::floor(value);};const auto hash=[&](Vec2 coordinate){coordinate={fraction(coordinate.x*123.34f),fraction(coordinate.y*456.21f)};const float value=coordinate.x*(coordinate.x+45.32f)+coordinate.y*(coordinate.y+45.32f);coordinate={coordinate.x+value,coordinate.y+value};return fraction(coordinate.x*coordinate.y);};const auto noise=[&](Vec2 coordinate){const Vec2 cell{std::floor(coordinate.x),std::floor(coordinate.y)},part{fraction(coordinate.x),fraction(coordinate.y)},smooth{part.x*part.x*(3.f-2.f*part.x),part.y*part.y*(3.f-2.f*part.y)};const float lower=hash(cell)+(hash({cell.x+1,cell.y})-hash(cell))*smooth.x,upper=hash({cell.x,cell.y+1})+(hash({cell.x+1,cell.y+1})-hash({cell.x,cell.y+1}))*smooth.x;return lower+(upper-lower)*smooth.y;};const Vec2 rotated{worldPosition.x*.8f-worldPosition.y*.6f,worldPosition.x*.6f+worldPosition.y*.8f};const float middle=noise(worldPosition*.14f+Vec2{1.f,.7f}),secondary=noise(rotated*.38f+Vec2{13.7f,-5.9f}),broad=noise(worldPosition*.021f),combined=middle*.62f+secondary*.30f+broad*.08f;return Clamp(.5f+(combined-.5f)*1.45f,0.f,1.f);}
inline float TerrainGrassDualDomainBlend3D(Vec2 worldPosition){if(!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y))return 0.f;const float signal=TerrainMesoPartitionSignal3D(worldPosition*.19f+Vec2{37.11f,-19.73f}),smooth=signal*signal*(3.f-2.f*signal);return .28f+.34f*smooth;}
inline Vec3 TerrainForestFloorSpectralVariation3D(float mesoSignal,float macroSignal,float grassWeight,float soilWeight,float authoredCalibration){if(!std::isfinite(mesoSignal)||!std::isfinite(macroSignal)||!std::isfinite(grassWeight)||!std::isfinite(soilWeight)||!std::isfinite(authoredCalibration))return{1,1,1};const float habitat=Clamp(mesoSignal*.68f+macroSignal*.32f,0.f,1.f),shaped=habitat*habitat*(3.f-2.f*habitat),naturalFloor=Clamp(grassWeight+soilWeight,0.f,1.f)*(1.f-Clamp(authoredCalibration,0.f,1.f));const Vec3 litter{1.12f,.96f,.78f},moss{.78f,1.12f,.84f},target=litter+(moss-litter)*shaped,spectral=Vec3{1,1,1}+(target-Vec3{1,1,1})*(naturalFloor*.62f);const float luma=spectral.x*.2126f+spectral.y*.7152f+spectral.z*.0722f;return luma>.000001f?spectral*(1.f/luma):Vec3{1,1,1};}
inline Vec3 BlendTerrainGrassAlbedo3D(Vec3 primary,Vec3 secondary,float blend)noexcept{if(!std::isfinite(primary.x)||!std::isfinite(primary.y)||!std::isfinite(primary.z)||!std::isfinite(secondary.x)||!std::isfinite(secondary.y)||!std::isfinite(secondary.z)||!std::isfinite(blend))return primary;blend=Clamp(blend,0.f,1.f);const Vec3 mixed=primary+(secondary-primary)*blend;const auto luma=[](Vec3 value){return value.x*.2126f+value.y*.7152f+value.z*.0722f;};const float target=luma(primary)+(luma(secondary)-luma(primary))*blend,actual=luma(mixed);return target>0.f&&actual>1e-6f?mixed*(target/actual):mixed;}
inline float TerrainWetShoreBlend3D(float authoredWeight,float breakupSignal)noexcept{
    if(!std::isfinite(authoredWeight)||!std::isfinite(breakupSignal))return 0.f;
    const float weight=Clamp(authoredWeight,0.f,1.f),breakup=Clamp(breakupSignal,0.f,1.f);
    // 只在插值中的濕岸邊緣偏移權重；純乾地與純濕土仍精確保持 0/1。
    const float edge=Clamp(weight+(breakup-.5f)*.24f*(4.f*weight*(1.f-weight)),0.f,1.f);
    return edge*edge*(3.f-2.f*edge);
}
inline TerrainBiomeWeights3D ApplyTerrainMesoPartition3D(TerrainBiomeWeights3D source,Vec2 worldPosition,const TerrainDistanceMaterialSettings3D& settings={}){if(!settings.enabled||!settings.mesoPartitionEnabled)return source;if(!std::isfinite(source.grass)||!std::isfinite(source.soil)||!std::isfinite(source.rock))return{};const float rock=Clamp(source.rock,0.f,1.f),nonRock=1.f-rock;if(nonRock==0.f)return{0,0,rock};const float originalSoil=Clamp(source.soil/nonRock,0.f,1.f),signal=TerrainMesoPartitionSignal3D(worldPosition),inverse=1.f-signal,t=Clamp((inverse-.43f)/(.70f-.43f),0.f,1.f),targetSoil=t*t*(3.f-2.f*t),soilFraction=originalSoil+(targetSoil-originalSoil)*.62f,soil=nonRock*soilFraction;return{nonRock-soil,soil,rock};}
inline float TerrainDetailWeight3D(float cameraDistance,const TerrainDistanceMaterialSettings3D& source={}){const TerrainDistanceMaterialSettings3D settings=NormalizeTerrainDistanceMaterialSettings3D(source);if(!settings.enabled)return 1.f;if(!std::isfinite(cameraDistance))return 0.f;const float value=Clamp((cameraDistance-settings.detailFadeStart)/(settings.detailFadeEnd-settings.detailFadeStart),0.f,1.f);return 1.f-value*value*(3.f-2.f*value);}
inline Vec3 TerrainLowSkyDistanceLighting3D(Vec3 lighting,float detailWeight,float skyIntensity)noexcept{
    if(!std::isfinite(lighting.x)||!std::isfinite(lighting.y)||!std::isfinite(lighting.z)||!std::isfinite(detailWeight)||!std::isfinite(skyIntensity))return lighting;
    if(skyIntensity>=.5f||detailWeight>=1.f)return lighting;
    const float lowSkyLinear=Clamp((.5f-Clamp(skyIntensity,0.f,16.f))/.22f,0.f,1.f),lowSky=lowSkyLinear*lowSkyLinear*(3.f-2.f*lowSkyLinear),amount=Clamp((1.f-Clamp(detailWeight,0.f,1.f))*lowSky*4.f,0.f,1.f);
    const float luma=lighting.x*.2126f+lighting.y*.7152f+lighting.z*.0722f;const Vec3 neutralized=lighting+(Vec3{luma,luma,luma}-lighting)*.30f,distantNight=neutralized*.68f;return lighting+(distantNight-lighting)*amount;
}
inline unsigned TerrainMaterialTextureSamples3D(float cameraDistance,bool hasTextures=true,const TerrainDistanceMaterialSettings3D& source={},bool grassDualDomain=false){if(!hasTextures)return 0;return TerrainDetailWeight3D(cameraDistance,source)>.001f?(grassDualDomain?15u:9u):3u;}
inline float TerrainMacroVariation3D(Vec2 worldPosition,const TerrainDistanceMaterialSettings3D& source={}){const TerrainDistanceMaterialSettings3D settings=NormalizeTerrainDistanceMaterialSettings3D(source);if(!settings.enabled||settings.macroVariationStrength<=0||!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y))return 1.f;const float value=std::sin(worldPosition.x*settings.macroVariationScale*1.731f+worldPosition.y*settings.macroVariationScale*.917f)*.5f+.5f;return 1.f-settings.macroVariationStrength+2.f*settings.macroVariationStrength*value;}
inline Vec3 TerrainTriplanarWeights3D(Vec3 normal){if(!std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z))return{0,1,0};const Vec3 powered{std::pow(std::abs(normal.x),4.f),std::pow(std::abs(normal.y),4.f),std::pow(std::abs(normal.z),4.f)};const float total=powered.x+powered.y+powered.z;return total>.000001f?powered*(1.f/total):Vec3{0,1,0};}
inline float TerrainMicroVariation3D(float noiseSample,float cameraDistance,const TerrainDistanceMaterialSettings3D& source={}){if(!std::isfinite(noiseSample))return 1.f;const float detail=TerrainDetailWeight3D(cameraDistance,source);return 1.f+(Clamp(noiseSample,0.f,1.f)*.16f-.08f)*detail;}
enum class TerrainSurfaceProjection3D:uint8_t {WorldSpaceTriplanar};
struct TerrainSurfaceShaderRoute3D {TerrainSurfaceProjection3D projection{TerrainSurfaceProjection3D::WorldSpaceTriplanar};MaterialSurfaceMapEncoding3D surfaceEncoding{MaterialSurfaceMapEncoding3D::PackedOrm};bool baseColorSrgb{true},normalDirectX{true},directionalLightShaftsAtmosphereOnly{true},screenSpaceReflectionsWaterOnly{true};friend constexpr bool operator==(const TerrainSurfaceShaderRoute3D&,const TerrainSurfaceShaderRoute3D&)=default;};
inline constexpr TerrainSurfaceShaderRoute3D TerrainSurfaceShaderRoute{};
inline float FoliageWrapDiffuse3D(float normalLightCosine,float wrap=.35f){if(!std::isfinite(normalLightCosine)||!std::isfinite(wrap))return 0.f;wrap=Clamp(wrap,0.f,1.f);return Clamp((normalLightCosine+wrap)/(1.f+wrap),0.f,1.f);}
inline float FoliageTransmission3D(float normalLightCosine,float viewLightCosine){if(!std::isfinite(normalLightCosine)||!std::isfinite(viewLightCosine))return 0.f;const float backLight=Clamp(-normalLightCosine,0.f,1.f),forwardScatter=Clamp(viewLightCosine,0.f,1.f);return Clamp(backLight*backLight*(.45f+.55f*forwardScatter),0.f,1.f);}
inline float FoliagePhysicalTransmission3D(float normalLightCosine,float viewLightCosine){if(!std::isfinite(normalLightCosine)||!std::isfinite(viewLightCosine))return 0.f;const float backLight=Clamp(-normalLightCosine,0.f,1.f),forwardScatter=Clamp(-viewLightCosine,0.f,1.f);return Clamp(backLight*backLight*(.45f+.55f*forwardScatter),0.f,1.f);}
inline Vec3 FoliageTwoSidedShadingNormal3D(Vec3 normal,Vec3 viewDirection,bool frontFace){if(!std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z)||!std::isfinite(viewDirection.x)||!std::isfinite(viewDirection.y)||!std::isfinite(viewDirection.z)||Length(normal)<.0001f||Length(viewDirection)<.0001f)return{0,1,0};normal=Normalize(normal);viewDirection=Normalize(viewDirection);const Vec3 oriented=frontFace?normal:normal*-1.f;const float rawFacing=Dot(oriented,viewDirection);const Vec3 tangent=oriented-viewDirection*rawFacing;const Vec3 hemisphere=rawFacing<0.f?Normalize(tangent+viewDirection*.02f):oriented;const float facing=Clamp(Dot(hemisphere,viewDirection),0.f,1.f),t=Clamp((facing-.08f)/.24f,0.f,1.f),smooth=t*t*(3.f-2.f*t),grazing=(1.f-smooth)*.12f,back=frontFace?0.f:.18f,convergence=(std::max)(grazing,back);return Normalize(hemisphere*(1.f-convergence)+Normalize(hemisphere+viewDirection*.18f)*convergence);}
inline float FoliageIndirectOcclusion3D(float combinedOcclusion,bool foliage){if(!foliage)return combinedOcclusion;if(!std::isfinite(combinedOcclusion))return 0.f;const float value=Clamp(combinedOcclusion,0.f,1.f);return value*.82f+std::sqrt(value)*.18f;}
inline Vec3 FoliageNeutralizedTransmissionColor3D(Vec3 color,float transmissionWeight){if(!std::isfinite(color.x)||!std::isfinite(color.y)||!std::isfinite(color.z)||!std::isfinite(transmissionWeight))return {};color={(std::max)(0.f,color.x),(std::max)(0.f,color.y),(std::max)(0.f,color.z)};const float luminance=color.x*.2126f+color.y*.7152f+color.z*.0722f,weight=.08f*Clamp(transmissionWeight,0.f,1.f);return color+(Vec3{luminance,luminance,luminance}-color)*weight;}
struct SkinSubsurfaceSettings3D {bool enabled{true};float strength{.62f},wrap{.3f},curvatureScale{1.35f},transmission{.18f};friend constexpr bool operator==(const SkinSubsurfaceSettings3D&,const SkinSubsurfaceSettings3D&)=default;};
inline SkinSubsurfaceSettings3D NormalizeSkinSubsurfaceSettings3D(SkinSubsurfaceSettings3D settings){settings.strength=Clamp(std::isfinite(settings.strength)?settings.strength:.62f,0.f,2.f);settings.wrap=Clamp(std::isfinite(settings.wrap)?settings.wrap:.3f,.02f,.75f);settings.curvatureScale=Clamp(std::isfinite(settings.curvatureScale)?settings.curvatureScale:1.35f,0.f,8.f);settings.transmission=Clamp(std::isfinite(settings.transmission)?settings.transmission:.18f,0.f,1.f);return settings;}
inline Vec3 EvaluateSkinSubsurfaceDiffusion3D(float normalLightCosine,float viewAgainstLightCosine,float curvature,const SkinSubsurfaceSettings3D& source={}){
    // RGB 使用不同包覆半徑近似皮膚擴散輪廓；只回傳超出 Lambert 的能量，便於 shader 守恆限幅。
    const auto settings=NormalizeSkinSubsurfaceSettings3D(source);if(!settings.enabled||settings.strength<=0.f||!std::isfinite(normalLightCosine)||!std::isfinite(viewAgainstLightCosine)||!std::isfinite(curvature))return{};const float direct=Clamp(normalLightCosine,0.f,1.f),curve=Clamp(curvature*settings.curvatureScale,0.f,1.f),back=Clamp(-normalLightCosine,0.f,1.f);const auto wrapped=[&](float radius){return Clamp((normalLightCosine+radius)/(1.f+radius),0.f,1.f);};const float red=(std::max)(0.f,wrapped(settings.wrap)-direct),green=(std::max)(0.f,wrapped(settings.wrap*.62f)-direct),blue=(std::max)(0.f,wrapped(settings.wrap*.34f)-direct),forward=std::pow(Clamp(viewAgainstLightCosine,0.f,1.f),4.f),transmission=back*back*forward*settings.transmission,curveWeight=.35f+.65f*curve;return{Clamp((red*curveWeight+transmission)*settings.strength,0.f,.3f),Clamp((green*curveWeight+transmission*.42f)*settings.strength,0.f,.3f),Clamp((blue*curveWeight+transmission*.16f)*settings.strength,0.f,.3f)};
}
inline float ProceduralCloudDensity3D(float noiseSample,float coverage=.54f,float softness=.18f){if(!std::isfinite(noiseSample)||!std::isfinite(coverage)||!std::isfinite(softness)||softness<=0.f)return 0.f;const float value=Clamp((noiseSample-coverage)/softness,0.f,1.f);return value*value*(3.f-2.f*value);}
inline float CloudHorizonFade3D(float rayHeight){if(!std::isfinite(rayHeight))return 0.f;const float value=Clamp((rayHeight-.025f)/.18f,0.f,1.f);return value*value*(3.f-2.f*value);}
struct VolumetricCloudSettings3D {bool enabled{true};float baseAltitude{1200.f},thickness{1800.f},coverage{.52f},density{1.15f},erosion{.24f},windSpeed{18.f};uint32_t sampleCount{16};bool shadowsEnabled{true};float shadowStrength{.72f};float maximumDistance{12000.f};std::uint32_t baseNoiseSeed{0x9e3779b9u},detailNoiseSeed{0x243f6a88u};float baseNoiseRotation{.37f},detailNoiseRotation{-1.11f};friend constexpr bool operator==(const VolumetricCloudSettings3D&,const VolumetricCloudSettings3D&)=default;};
inline VolumetricCloudSettings3D NormalizeVolumetricCloudSettings3D(VolumetricCloudSettings3D settings){settings.baseAltitude=Clamp(std::isfinite(settings.baseAltitude)?settings.baseAltitude:1200.f,-10000.f,50000.f);settings.thickness=Clamp(std::isfinite(settings.thickness)?settings.thickness:1800.f,10.f,20000.f);settings.coverage=Clamp(std::isfinite(settings.coverage)?settings.coverage:.52f,.05f,.95f);settings.density=Clamp(std::isfinite(settings.density)?settings.density:1.15f,0.f,4.f);settings.erosion=Clamp(std::isfinite(settings.erosion)?settings.erosion:.24f,0.f,.8f);settings.windSpeed=Clamp(std::isfinite(settings.windSpeed)?settings.windSpeed:18.f,-200.f,200.f);settings.sampleCount=(std::clamp)(settings.sampleCount,4u,32u);settings.shadowStrength=Clamp(std::isfinite(settings.shadowStrength)?settings.shadowStrength:.72f,0.f,1.f);settings.maximumDistance=Clamp(std::isfinite(settings.maximumDistance)?settings.maximumDistance:12000.f,64.f,50000.f);settings.baseNoiseRotation=Clamp(std::isfinite(settings.baseNoiseRotation)?settings.baseNoiseRotation:.37f,-6.2831853f,6.2831853f);settings.detailNoiseRotation=Clamp(std::isfinite(settings.detailNoiseRotation)?settings.detailNoiseRotation:-1.11f,-6.2831853f,6.2831853f);if(settings.baseNoiseSeed==settings.detailNoiseSeed)settings.detailNoiseSeed^=0x85ebca6bu;return settings;}
inline float VolumetricCloudDensity3D(float baseNoise,float erosionNoise,float normalizedHeight,const VolumetricCloudSettings3D& source={}){const auto settings=NormalizeVolumetricCloudSettings3D(source);if(!settings.enabled||settings.density<=0.f||!std::isfinite(baseNoise)||!std::isfinite(erosionNoise)||!std::isfinite(normalizedHeight))return 0.f;const float height=Clamp(normalizedHeight,0.f,1.f),profile=Clamp(height/.16f,0.f,1.f)*Clamp((1.f-height)/.28f,0.f,1.f),shape=baseNoise-settings.erosion*erosionNoise;return ProceduralCloudDensity3D(shape,settings.coverage,.18f)*profile*settings.density;}
inline float VolumetricCloudTransmittance3D(float integratedDensity){return std::isfinite(integratedDensity)&&integratedDensity>0.f?std::exp(-(std::min)(integratedDensity,80.f)):1.f;}
inline float VolumetricCloudRayLength3D(float cameraHeight,float rayHeight,const VolumetricCloudSettings3D& source={}){const auto settings=NormalizeVolumetricCloudSettings3D(source);if(!settings.enabled||settings.density<=0.f||!std::isfinite(cameraHeight)||!std::isfinite(rayHeight)||std::abs(rayHeight)<=.001f)return 0.f;const float first=(settings.baseAltitude-cameraHeight)/rayHeight,second=(settings.baseAltitude+settings.thickness-cameraHeight)/rayHeight,entry=(std::max)(0.f,(std::min)(first,second)),exit=(std::min)(settings.maximumDistance,(std::max)(first,second));return std::isfinite(entry)&&std::isfinite(exit)&&exit>entry?exit-entry:0.f;}
inline float VolumetricCloudGrazingFade3D(float rayHeight){if(!std::isfinite(rayHeight))return 0.f;const float value=Clamp((std::abs(rayHeight)-.004f)/.028f,0.f,1.f);return value*value*(3.f-2.f*value);}
inline float VolumetricCloudPathCapFade3D(float layerEntry,float maximumDistance,float cloudThickness){if(!std::isfinite(layerEntry)||!std::isfinite(maximumDistance)||!std::isfinite(cloudThickness)||maximumDistance<=0.f||cloudThickness<=0.f)return 0.f;const float width=Clamp(cloudThickness*.12f,32.f,256.f),value=Clamp((maximumDistance-layerEntry)/width,0.f,1.f);return value*value*(3.f-2.f*value);}
inline float BeerLambertTransmittance3D(float extinction,float distance){if(!std::isfinite(extinction)||!std::isfinite(distance)||extinction<=0.f||distance<=0.f)return 1.f;return std::exp(-(std::min)(extinction*distance,80.f));}
inline float EnergyConservingSingleScattering3D(float scattering,float extinction,float distance){if(!std::isfinite(scattering)||!std::isfinite(extinction)||!std::isfinite(distance)||scattering<=0.f||extinction<=0.f||distance<=0.f)return 0.f;scattering=(std::min)(scattering,extinction);return scattering/extinction*(1.f-BeerLambertTransmittance3D(extinction,distance));}
inline Vec3 VolumetricCloudShadowSamplePosition3D(Vec3 worldPosition,Vec3 sunDirection,const VolumetricCloudSettings3D& source={}){const auto settings=NormalizeVolumetricCloudSettings3D(source);if(!settings.enabled||!settings.shadowsEnabled||settings.shadowStrength<=0.f||!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y)||!std::isfinite(worldPosition.z)||!std::isfinite(sunDirection.x)||!std::isfinite(sunDirection.y)||!std::isfinite(sunDirection.z)||sunDirection.y<=.02f)return worldPosition;const float targetHeight=settings.baseAltitude+settings.thickness*.5f,distance=(targetHeight-worldPosition.y)/sunDirection.y;if(!std::isfinite(distance)||distance<=0.f)return worldPosition;return worldPosition+sunDirection*distance;}
inline float VolumetricCloudShadowTransmittance3D(float baseNoise,float erosionNoise,const VolumetricCloudSettings3D& source={}){const auto settings=NormalizeVolumetricCloudSettings3D(source);if(!settings.enabled||!settings.shadowsEnabled||settings.shadowStrength<=0.f)return 1.f;const float density=VolumetricCloudDensity3D(baseNoise,erosionNoise,.5f,settings);return 1.f-settings.shadowStrength+settings.shadowStrength*VolumetricCloudTransmittance3D(density*1.35f);}
struct BloomSettings3D {bool enabled{true};float threshold{1.f},softKnee{.5f},intensity{.12f},radiusPixels{2.5f};bool depthAware{true};float depthThreshold{.02f},depthSoftness{.02f};};
inline BloomSettings3D NormalizeBloomSettings3D(BloomSettings3D settings){settings.threshold=Clamp(std::isfinite(settings.threshold)?settings.threshold:1.f,0.f,32.f);settings.softKnee=Clamp(std::isfinite(settings.softKnee)?settings.softKnee:.5f,0.f,1.f);settings.intensity=Clamp(std::isfinite(settings.intensity)?settings.intensity:.12f,0.f,4.f);settings.radiusPixels=Clamp(std::isfinite(settings.radiusPixels)?settings.radiusPixels:2.5f,.5f,16.f);settings.depthThreshold=Clamp(std::isfinite(settings.depthThreshold)?settings.depthThreshold:.02f,0.f,1.f);settings.depthSoftness=Clamp(std::isfinite(settings.depthSoftness)?settings.depthSoftness:.02f,0.f,1.f);return settings;}
inline float BloomDepthSemanticWeight3D(float centerDepth,float sampleDepth,float localDepthDerivative,const BloomSettings3D& source={},bool centerValid=true,bool sampleValid=true){const BloomSettings3D settings=NormalizeBloomSettings3D(source);if(!settings.depthAware||!centerValid||!sampleValid||!std::isfinite(centerDepth)||!std::isfinite(sampleDepth)||!std::isfinite(localDepthDerivative)||centerDepth<0.f||centerDepth>1.f||sampleDepth<0.f||sampleDepth>1.f)return 1.f;const bool centerSky=centerDepth==1.f,sampleSky=sampleDepth==1.f;if(centerSky!=sampleSky)return 0.f;if(centerSky)return 1.f;const float scale=(std::max)(1.f-(std::min)(centerDepth,sampleDepth),1e-4f),relativeDifference=std::abs(sampleDepth-centerDepth)/scale,derivative=std::abs(localDepthDerivative)/scale,edge=settings.depthThreshold+derivative*settings.radiusPixels,width=(std::max)(settings.depthSoftness,1e-5f);if(relativeDifference<=edge)return 1.f;if(relativeDifference>=edge+width)return 0.f;const float t=(relativeDifference-edge)/width;return 1.f-t*t*(3.f-2.f*t);}
inline float BloomSoftThreshold3D(float luminance,const BloomSettings3D& source={}){const BloomSettings3D settings=NormalizeBloomSettings3D(source);if(!settings.enabled||!std::isfinite(luminance))return 0.f;const float knee=(std::max)(settings.threshold*settings.softKnee,.0001f),soft=Clamp((luminance-settings.threshold+knee)/(2.f*knee),0.f,1.f);return(std::max)(luminance-settings.threshold,0.f)+soft*soft*knee;}
inline float DirectionalShadowReceiverBias3D(float normalLightCosine,float slopeScale=.0015f,float minimumBias=.00035f,float maximumBias=.006f){if(!std::isfinite(normalLightCosine)||!std::isfinite(slopeScale)||!std::isfinite(minimumBias)||!std::isfinite(maximumBias))return(std::max)(0.f,minimumBias);const float cosine=Clamp(normalLightCosine,0.f,1.f),slope=std::sqrt((std::max)(0.f,1.f-cosine*cosine))/(std::max)(.2f,cosine);return Clamp((std::max)(0.f,minimumBias)+(std::max)(0.f,slopeScale)*slope,(std::max)(0.f,minimumBias),(std::max)(minimumBias,maximumBias));}
inline Vec3 TerrainLayerWeights3D(float slope,float normalizedHeight,float macroNoise){if(!std::isfinite(slope)||!std::isfinite(normalizedHeight)||!std::isfinite(macroNoise))return{1,0,0};slope=Clamp(slope,0.f,1.f);normalizedHeight=Clamp(normalizedHeight,0.f,1.f);macroNoise=Clamp(macroNoise,0.f,1.f);const auto smooth=[](float edge0,float edge1,float value){value=Clamp((value-edge0)/(edge1-edge0),0.f,1.f);return value*value*(3.f-2.f*value);};const float rock=smooth(.38f,.78f,slope+(std::max)(0.f,normalizedHeight-.62f)*.35f),soil=(1.f-rock)*smooth(.18f,.62f,slope+(1.f-macroNoise)*.18f);return{(std::max)(0.f,1.f-rock-soil),soil,rock};}
struct PointLight3D {Vec3 position{};float radius{8.f};Color color{1.f,.72f,.38f,1.f};float intensity{1.f};};
inline constexpr std::size_t MaximumForwardPlusPointLights3D=256;
inline constexpr std::size_t TypicalForwardPlusPointLights3D=64;
inline constexpr std::uint32_t ForwardPlusTileSize3D=16;
inline constexpr std::uint32_t MaximumForwardPlusLightsPerTile3D=64;
struct PointLightGpu3D {Vec3 position{};float radius{};Vec3 color{};float intensity{};};
static_assert(sizeof(PointLightGpu3D)==32);
struct ForwardPlusTileGridEntry3D {std::uint32_t offset{};std::uint32_t count{};std::uint32_t overflow{};std::uint32_t reserved{};};
static_assert(sizeof(ForwardPlusTileGridEntry3D)==16);
[[nodiscard]] constexpr std::uint32_t ForwardPlusTileCount3D(std::uint32_t extent)noexcept{return(extent+ForwardPlusTileSize3D-1u)/ForwardPlusTileSize3D;}
[[nodiscard]] constexpr std::size_t ForwardPlusIndexCapacity3D(std::uint32_t width,std::uint32_t height)noexcept{return std::size_t(ForwardPlusTileCount3D(width))*ForwardPlusTileCount3D(height)*MaximumForwardPlusLightsPerTile3D;}
struct CanonicalPointLights3D {std::array<PointLightGpu3D,MaximumForwardPlusPointLights3D> lights{};std::uint32_t count{};std::uint32_t rejected{};std::uint64_t fingerprint{1469598103934665603ull};};
[[nodiscard]] inline bool ValidPointLight3D(const PointLight3D& light)noexcept{return std::isfinite(light.position.x)&&std::isfinite(light.position.y)&&std::isfinite(light.position.z)&&std::isfinite(light.radius)&&light.radius>0.f&&std::isfinite(light.color.r)&&std::isfinite(light.color.g)&&std::isfinite(light.color.b)&&std::isfinite(light.color.a)&&std::isfinite(light.intensity)&&light.intensity>0.f;}
[[nodiscard]] inline bool PointLightCanonicalLess3D(const PointLight3D& a,const PointLight3D& b)noexcept{const auto ordered=[](float value){const std::uint32_t bits=std::bit_cast<std::uint32_t>(value);return bits&0x80000000u?~bits:bits^0x80000000u;};const float av[]{a.position.x,a.position.y,a.position.z,a.radius,a.color.r,a.color.g,a.color.b,a.intensity},bv[]{b.position.x,b.position.y,b.position.z,b.radius,b.color.r,b.color.g,b.color.b,b.intensity};for(std::size_t index=0;index<8;++index){const auto ak=ordered(av[index]),bk=ordered(bv[index]);if(ak!=bk)return ak<bk;}return false;}
[[nodiscard]] inline CanonicalPointLights3D CanonicalizePointLights3D(std::span<const PointLight3D> source){
    CanonicalPointLights3D result{};std::vector<PointLightGpu3D> valid;valid.reserve(source.size());for(const auto& light:source){if(!ValidPointLight3D(light)){++result.rejected;continue;}valid.push_back({{light.position.x,light.position.y,light.position.z},light.radius,{light.color.r,light.color.g,light.color.b},light.intensity});}
    const auto orderedFloat=[](float value){const std::uint32_t bits=std::bit_cast<std::uint32_t>(value);return bits&0x80000000u?~bits:bits^0x80000000u;};
    const auto less=[&](const PointLightGpu3D& a,const PointLightGpu3D& b){const float av[]={a.position.x,a.position.y,a.position.z,a.radius,a.color.x,a.color.y,a.color.z,a.intensity},bv[]={b.position.x,b.position.y,b.position.z,b.radius,b.color.x,b.color.y,b.color.z,b.intensity};for(std::size_t i=0;i<8;++i){const auto ak=orderedFloat(av[i]),bk=orderedFloat(bv[i]);if(ak!=bk)return ak<bk;}return false;};
    std::sort(valid.begin(),valid.end(),less);result.count=std::uint32_t((std::min)(valid.size(),MaximumForwardPlusPointLights3D));result.rejected+=std::uint32_t(valid.size()-result.count);std::copy_n(valid.begin(),result.count,result.lights.begin());
    for(std::uint32_t index=0;index<result.count;++index)for(const std::byte value:std::as_bytes(std::span{&result.lights[index],std::size_t{1}}))result.fingerprint=(result.fingerprint^std::to_integer<std::uint8_t>(value))*1099511628211ull;result.fingerprint=(result.fingerprint^result.count)*1099511628211ull;return result;
}
inline float PointLightInfluence(const PointLight3D& light,Vec3 reference){const Vec3 delta=light.position-reference;return light.intensity*light.radius*light.radius/(1.f+Dot(delta,delta));}
template<std::size_t Capacity>struct PointLightSelection3D {std::array<PointLight3D,Capacity> lights{};std::array<float,Capacity> influences{};std::size_t count{},candidates{};};
template<std::size_t Capacity>PointLightSelection3D<Capacity> SelectInfluentialPointLights(std::span<const PointLight3D> lights,Vec3 reference){
    static_assert(Capacity>0);PointLightSelection3D<Capacity> result{};result.candidates=lights.size();
    for(const PointLight3D& light:lights){const float influence=PointLightInfluence(light,reference);if(!std::isfinite(influence)||influence<=0.f)continue;std::size_t destination=result.count;while(destination>0&&influence>result.influences[destination-1])--destination;if(destination>=Capacity)continue;const std::size_t newCount=(std::min)(Capacity,result.count+1);for(std::size_t index=newCount-1;index>destination;--index){result.lights[index]=result.lights[index-1];result.influences[index]=result.influences[index-1];}result.lights[destination]=light;result.influences[destination]=influence;result.count=newCount;}
    return result;
}
struct AerialPerspectiveSettings3D {
    bool enabled{true};
    float extinction{.0024f},scattering{.00165f},heightFalloff{.025f},baseHeight{};
    float nearDistance{24.f},nearFadeDistance{24.f},maximumDistance{420.f},anisotropy{.18f};
    Color skyRadiance{.12f,.17f,.24f,1.f};
    float sunRadianceScale{.055f};
    friend constexpr bool operator==(const AerialPerspectiveSettings3D&,const AerialPerspectiveSettings3D&)=default;
};
inline AerialPerspectiveSettings3D NormalizeAerialPerspectiveSettings3D(AerialPerspectiveSettings3D settings){
    settings.extinction=Clamp(std::isfinite(settings.extinction)?settings.extinction:.0024f,0.f,1.f);
    settings.scattering=Clamp(std::isfinite(settings.scattering)?settings.scattering:.00165f,0.f,settings.extinction);
    settings.heightFalloff=Clamp(std::isfinite(settings.heightFalloff)?settings.heightFalloff:.025f,0.f,4.f);
    settings.baseHeight=std::isfinite(settings.baseHeight)?settings.baseHeight:0.f;
    settings.nearDistance=Clamp(std::isfinite(settings.nearDistance)?settings.nearDistance:24.f,0.f,100000.f);
    settings.nearFadeDistance=Clamp(std::isfinite(settings.nearFadeDistance)?settings.nearFadeDistance:24.f,.001f,100000.f);
    settings.maximumDistance=Clamp(std::isfinite(settings.maximumDistance)?settings.maximumDistance:420.f,settings.nearDistance+.001f,1000000.f);
    settings.anisotropy=Clamp(std::isfinite(settings.anisotropy)?settings.anisotropy:.18f,-.8f,.85f);
    settings.skyRadiance.r=Clamp(std::isfinite(settings.skyRadiance.r)?settings.skyRadiance.r:.12f,0.f,100.f);
    settings.skyRadiance.g=Clamp(std::isfinite(settings.skyRadiance.g)?settings.skyRadiance.g:.17f,0.f,100.f);
    settings.skyRadiance.b=Clamp(std::isfinite(settings.skyRadiance.b)?settings.skyRadiance.b:.24f,0.f,100.f);
    settings.skyRadiance.a=1.f;
    settings.sunRadianceScale=Clamp(std::isfinite(settings.sunRadianceScale)?settings.sunRadianceScale:.055f,0.f,1.f);
    return settings;
}
struct AerialPerspectiveEvaluation3D {float transmittance{1.f},scatteredFraction{},opticalDepth{},effectiveDistance{};[[nodiscard]] bool FiniteAndConservative()const noexcept{return std::isfinite(transmittance)&&std::isfinite(scatteredFraction)&&std::isfinite(opticalDepth)&&std::isfinite(effectiveDistance)&&transmittance>=0.f&&transmittance<=1.f&&scatteredFraction>=0.f&&scatteredFraction<=1.f-transmittance+1e-6f&&opticalDepth>=0.f&&effectiveDistance>=0.f;}};
inline AerialPerspectiveEvaluation3D EvaluateAerialPerspective3D(const AerialPerspectiveSettings3D& source,Vec3 cameraPosition,Vec3 worldPosition){
    // 近場以平滑權重排除；高度密度沿實際視線段積分，再由 Beer-Lambert 取得守恆透射與單次散射比例。
    const auto settings=NormalizeAerialPerspectiveSettings3D(source);AerialPerspectiveEvaluation3D result{};
    if(!settings.enabled||settings.extinction<=0.f||!std::isfinite(cameraPosition.x)||!std::isfinite(cameraPosition.y)||!std::isfinite(cameraPosition.z)||!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y)||!std::isfinite(worldPosition.z))return result;
    const Vec3 ray=worldPosition-cameraPosition;const float rayLength=Length(ray);if(!std::isfinite(rayLength)||rayLength<=settings.nearDistance)return result;
    const float available=(std::min)(rayLength,settings.maximumDistance)-settings.nearDistance,onset=Clamp(available/settings.nearFadeDistance,0.f,1.f),smoothOnset=onset*onset*(3.f-2.f*onset);result.effectiveDistance=available*smoothOnset;if(available<=0.f||smoothOnset<=0.f)return result;
    const Vec3 direction=ray*(1.f/rayLength);const float startHeight=cameraPosition.y+direction.y*settings.nearDistance;
    const float startDensity=std::exp(Clamp(-settings.heightFalloff*(startHeight-settings.baseHeight),-20.f,20.f));
    const float heightRate=settings.heightFalloff*direction.y;
    float densityIntegral{};if(std::abs(heightRate)<1e-5f)densityIntegral=startDensity*available;else densityIntegral=startDensity*(-std::expm1(Clamp(-heightRate*available,-20.f,20.f)))/heightRate;densityIntegral*=smoothOnset;
    if(!std::isfinite(densityIntegral)||densityIntegral<=0.f)return AerialPerspectiveEvaluation3D{};
    result.opticalDepth=(std::min)(settings.extinction*densityIntegral,80.f);result.transmittance=std::exp(-result.opticalDepth);result.scatteredFraction=settings.scattering/settings.extinction*(1.f-result.transmittance);return result;
}
struct Atmosphere3D {Color fogColor{.35f,.48f,.52f,1.f};Vec3 cameraPosition{};float fogStart{45.f},fogEnd{140.f};bool enabled{};float heightFogDensity{},heightFogFalloff{.12f},heightFogBaseHeight{},heightFogMaximumOpacity{.92f};AerialPerspectiveSettings3D aerialPerspective{};};
inline Atmosphere3D NormalizeAtmosphere3D(Atmosphere3D atmosphere){
    atmosphere.fogStart=std::isfinite(atmosphere.fogStart)?atmosphere.fogStart:45.f;atmosphere.fogEnd=(std::max)(atmosphere.fogStart+.001f,std::isfinite(atmosphere.fogEnd)?atmosphere.fogEnd:140.f);
    atmosphere.heightFogDensity=Clamp(std::isfinite(atmosphere.heightFogDensity)?atmosphere.heightFogDensity:0.f,0.f,1.f);atmosphere.heightFogFalloff=Clamp(std::isfinite(atmosphere.heightFogFalloff)?atmosphere.heightFogFalloff:.12f,0.f,4.f);atmosphere.heightFogBaseHeight=std::isfinite(atmosphere.heightFogBaseHeight)?atmosphere.heightFogBaseHeight:0.f;atmosphere.heightFogMaximumOpacity=Clamp(std::isfinite(atmosphere.heightFogMaximumOpacity)?atmosphere.heightFogMaximumOpacity:.92f,0.f,.98f);atmosphere.aerialPerspective=NormalizeAerialPerspectiveSettings3D(atmosphere.aerialPerspective);return atmosphere;
}
inline float EvaluateHeightFogAmount3D(const Atmosphere3D& source,Vec3 worldPosition){
    // 使用線段兩端密度的梯形積分，讓 CPU 參考與即時 shader 保持一致且不需額外資源。
    const Atmosphere3D atmosphere=NormalizeAtmosphere3D(source);if(!atmosphere.enabled||atmosphere.heightFogDensity<=0.f||!std::isfinite(worldPosition.x)||!std::isfinite(worldPosition.y)||!std::isfinite(worldPosition.z)||!std::isfinite(atmosphere.cameraPosition.x)||!std::isfinite(atmosphere.cameraPosition.y)||!std::isfinite(atmosphere.cameraPosition.z))return 0.f;
    const Vec3 delta=worldPosition-atmosphere.cameraPosition;const float distance=Length(delta);if(!std::isfinite(distance)||distance<=0.f)return 0.f;
    const float cameraExponent=Clamp(-atmosphere.heightFogFalloff*(atmosphere.cameraPosition.y-atmosphere.heightFogBaseHeight),-20.f,20.f),fragmentExponent=Clamp(-atmosphere.heightFogFalloff*(worldPosition.y-atmosphere.heightFogBaseHeight),-20.f,20.f);const float opticalDepth=distance*atmosphere.heightFogDensity*.5f*(std::exp(cameraExponent)+std::exp(fragmentExponent));return Clamp(1.f-std::exp(-(std::min)(opticalDepth,80.f)),0.f,atmosphere.heightFogMaximumOpacity);
}
struct DirectionalLightShaftSettings3D {bool enabled{true};float intensity{.65f},anisotropy{.72f},maximumDistance{320.f};uint32_t sampleCount{4};float shadowStrength{.72f};friend constexpr bool operator==(const DirectionalLightShaftSettings3D&,const DirectionalLightShaftSettings3D&)=default;};
inline DirectionalLightShaftSettings3D NormalizeDirectionalLightShaftSettings3D(DirectionalLightShaftSettings3D settings){settings.intensity=Clamp(std::isfinite(settings.intensity)?settings.intensity:.65f,0.f,1.5f);settings.anisotropy=Clamp(std::isfinite(settings.anisotropy)?settings.anisotropy:.72f,-.8f,.85f);settings.maximumDistance=Clamp(std::isfinite(settings.maximumDistance)?settings.maximumDistance:320.f,1.f,5000.f);settings.sampleCount=(std::clamp)(settings.sampleCount,2u,16u);settings.shadowStrength=Clamp(std::isfinite(settings.shadowStrength)?settings.shadowStrength:.72f,0.f,1.f);return settings;}
inline float EvaluateDirectionalLightShaftScattering3D(float opticalDepth,float shadowVisibility,float cloudTransmittance,float viewSunCosine,const DirectionalLightShaftSettings3D& source={}){
    // CPU 參考以相同的 Mie 相位與 Beer-Lambert 消光驗證 GPU 光柱輸出，輸入無效時關閉效果。
    const auto settings=NormalizeDirectionalLightShaftSettings3D(source);if(!settings.enabled||settings.intensity<=0.f||!std::isfinite(opticalDepth)||!std::isfinite(shadowVisibility)||!std::isfinite(cloudTransmittance)||!std::isfinite(viewSunCosine)||opticalDepth<=0.f)return 0.f;const float scattered=Clamp(1.f-std::exp(-(std::min)(opticalDepth,80.f)),0.f,1.f)*Clamp(shadowVisibility,0.f,1.f)*Clamp(cloudTransmittance,0.f,1.f)*MiePhase3D(viewSunCosine,settings.anisotropy)*settings.intensity;return Clamp(scattered,0.f,3.f);
}
struct AmbientOcclusionSettings3D {bool enabled{true};float radius{1.35f},bias{.035f},intensity{1.05f};};
struct RendererQualitySettings3D {bool directionalShadows{true},ambientOcclusion{true},multisampling{true},postProcessAntiAliasing{true};friend constexpr bool operator==(const RendererQualitySettings3D&,const RendererQualitySettings3D&)=default;};
enum class GpuInstanceSubmissionMode3D:uint8_t {Aggregate,MultiGroup};
struct GpuInstanceCullingSettings3D {bool enabled{};float maximumDistance{};GpuInstanceSubmissionMode3D submissionMode{GpuInstanceSubmissionMode3D::MultiGroup};friend constexpr bool operator==(const GpuInstanceCullingSettings3D&,const GpuInstanceCullingSettings3D&)=default;};
inline GpuInstanceCullingSettings3D NormalizeGpuInstanceCullingSettings3D(GpuInstanceCullingSettings3D settings){settings.maximumDistance=std::isfinite(settings.maximumDistance)&&settings.maximumDistance>0.f?settings.maximumDistance:0.f;return settings;}
struct GpuHlodSelectionResult3D {
    std::uint64_t requestSerial{},dataContentHash{},residentProxyBytes{};
    std::vector<HlodSelection3D> selections{};
    std::vector<std::uint8_t> stableModes{};
    bool gpuExecuted{},complete{},sceneEcsDrawListIntegrated{};
};
enum class HzbOcclusionQuality3D:uint8_t {Off,Conservative,Aggressive};
struct HzbOcclusionSettings3D {HzbOcclusionQuality3D quality{HzbOcclusionQuality3D::Conservative};uint32_t minimumCandidates{2048},maximumOccluderDraws{256},maximumOccluderTriangles{200000};float minimumOccluderPixels{64.f},depthBias{.0005f};friend constexpr bool operator==(const HzbOcclusionSettings3D&,const HzbOcclusionSettings3D&)=default;};
inline HzbOcclusionSettings3D NormalizeHzbOcclusionSettings3D(HzbOcclusionSettings3D settings){settings.minimumCandidates=(std::clamp)(settings.minimumCandidates,1024u,1u<<24);settings.maximumOccluderDraws=(std::clamp)(settings.maximumOccluderDraws,1u,4096u);settings.maximumOccluderTriangles=(std::clamp)(settings.maximumOccluderTriangles,1u,1u<<26);settings.minimumOccluderPixels=Clamp(std::isfinite(settings.minimumOccluderPixels)?settings.minimumOccluderPixels:64.f,1.f,4096.f);settings.depthBias=Clamp(std::isfinite(settings.depthBias)?settings.depthBias:.0005f,0.f,.05f);return settings;}
struct VelocityCapture3D {unsigned width{},height{};std::vector<Vec2> pixels{};bool historyValid{};};
inline AmbientOcclusionSettings3D NormalizeAmbientOcclusionSettings3D(AmbientOcclusionSettings3D settings){settings.radius=Clamp(std::isfinite(settings.radius)?settings.radius:1.35f,.1f,8.f);settings.bias=Clamp(std::isfinite(settings.bias)?settings.bias:.035f,0.f,.5f);settings.intensity=Clamp(std::isfinite(settings.intensity)?settings.intensity:1.05f,0.f,4.f);return settings;}
inline constexpr size_t AmbientOcclusionResidentBytes3D(unsigned width,unsigned height){return size_t((width+1)/2)*((height+1)/2)*4;}
struct DrawListStats2D {
    size_t inputTriangles{},submittedTriangles{},culledTriangles{},peakSubmittedTriangles{};
    size_t textGlyphs{},textSourcePixels{},textSpans{};
    size_t textContainmentTests{},textContainedSpans{},textContainmentBypassedTriangles{},textCulledSpans{},textCullingBypassedTriangles{};
    size_t primitiveCullTests{},primitiveCulled{},primitiveCulledTriangles{};
    size_t primitiveContained{},primitiveContainedTriangles{};
    size_t ellipseSegments{},ellipseOutlineSegments{},ellipseTrigonometricEvaluations{},ellipseTrigonometricEvaluationsSaved{};
    size_t ellipseStepCacheHits{},ellipseStepCacheMisses{};
    size_t sharedVertexTransforms{},sharedVertexTransformsSaved{};
    size_t primitiveColorPacks{},primitiveColorPacksSaved{};
    size_t geometryBatches{},geometryCulledBatches{},geometryVertices{},geometryTransformsSaved{};
    size_t additiveTriangles{},additiveSpans{};
    size_t radialLightBatches{},radialLightCandidates{},radialLightsSubmitted{},radialLightsCulled{},radialLightsBudgetDropped{};
    uint64_t totalInputTriangles{},totalCulledTriangles{};
    uint64_t totalPrimitiveCulledTriangles{};
    uint64_t totalPrimitiveContainedTriangles{};
    uint64_t totalEllipseSegments{},totalEllipseOutlineSegments{},totalEllipseTrigonometricEvaluationsSaved{};
    uint64_t totalEllipseStepCacheHits{},totalEllipseStepCacheMisses{};
    uint64_t totalSharedVertexTransforms{},totalSharedVertexTransformsSaved{};
    uint64_t totalPrimitiveColorPacks{},totalPrimitiveColorPacksSaved{};
    uint64_t totalTextSourcePixels{},totalTextSpans{};
    uint64_t totalTextContainmentTests{},totalTextContainedSpans{},totalTextContainmentBypassedTriangles{},totalTextCulledSpans{},totalTextCullingBypassedTriangles{};
    uint64_t totalAdditiveTriangles{},totalAdditiveSpans{};
    uint64_t totalRadialLightBatches{},totalRadialLightCandidates{},totalRadialLightsSubmitted{},totalRadialLightsCulled{},totalRadialLightsBudgetDropped{};
};
struct SystemGlyphCacheStats2D {
    size_t residentGlyphs{},residentBytes{},peakResidentGlyphs{},peakResidentBytes{};
    uint64_t hits{},misses{},evictions{},releasedBytes{},trims{};
};
struct TransientMemoryStats {
    size_t trackedCapacityBytes{};
    size_t reclaimableCapacityBytes{};
    size_t lastReleasedBytes{};
    uint64_t trimCount{};
};
inline constexpr uint32_t EnhancedWorldRootDescriptorTableDwords3D=8;
inline constexpr uint32_t EnhancedWorldRootSrvDwords3D=2;
inline constexpr uint32_t EnhancedWorldRootConstantDwords3D=2+4+12+8+8+4+4+12;
inline constexpr uint32_t EnhancedWorldRootDwords3D=EnhancedWorldRootDescriptorTableDwords3D+EnhancedWorldRootSrvDwords3D+EnhancedWorldRootConstantDwords3D;
static_assert(EnhancedWorldRootDwords3D==64,"Enhanced world root signature must remain at the D3D12 64 DWORD limit");
enum class BloomSemanticSource3D : uint8_t { Unavailable, NativeDepth, HdrAlphaCarrier, TemporalDepthHistory, DlssDepthReplay, DlssUnavailable };
struct BloomSemanticCapability3D {bool hdrAlphaShaders{},opaquePipelines{},transparentDualSourcePipeline{},maskedPipeline{},maskedMsaaPipeline{},atmospherePipeline{},postProcessPipeline{};[[nodiscard]] constexpr bool Complete()const noexcept{return hdrAlphaShaders&&opaquePipelines&&transparentDualSourcePipeline&&maskedPipeline&&maskedMsaaPipeline&&atmospherePipeline&&postProcessPipeline;}};
struct BloomSemanticReceipt3D {BloomSemanticSource3D requested{BloomSemanticSource3D::Unavailable},actual{BloomSemanticSource3D::Unavailable};BloomSemanticCapability3D capability{};bool requestedDepthAware{},available{},failOpen{},dualSourceBlendAvailable{},transparentOrderingPreserved{},nativeDepthTransitioned{},currentTemporalDepthHistory{},carrierProduced{},postProcessCarrierPipeline{};uint32_t transparentLayerCount{};uint64_t transparentOrderHash{};bool transparentCarrierPipeline{},atmosphereCarrierPipeline{};uint32_t semanticRenderWidth{},semanticRenderHeight{},semanticOutputWidth{},semanticOutputHeight{},semanticFrameIndex{};uint64_t semanticResourceGeneration{},semanticResizeGeneration{};bool dlssReplayCurrentFrame{},dlssReplaySampleable{},dlssReplayTransitioned{};};
// 視窗狀態不是完整遮蔽判定；flip swap chain 的 S_OK 也不能證明未被遮蔽。
struct WindowPresentationState3D {
    uint32_t showCommand{},cloaked{},cloakedHresult{};
    bool valid{},visible{},iconic{},foreground{},placementValid{},cloakedValid{};
};
struct PresentObservation3D {
    uint64_t serial{},qpcBegin{},qpcEnd{},qpcFrequency{},threadCpuCycles{};
    uint32_t hresult{},syncInterval{},flags{},swapEffect{},swapFlags{},bufferCount{},width{},height{};
    WindowPresentationState3D before{},after{};
    bool observed{},clockValid{},threadCpuCyclesValid{},descriptorValid{},captureRequested{};
};
struct RendererStats {
    uint64_t lastTerrainParallaxMaterialBinds{},lastTerrainParallaxMaterialSemanticIdentity{};
    uint32_t lastTerrainParallaxHeightLayerMask{};
    bool terrainParallaxEnabled{};
    TerrainParallaxSettings3D terrainParallaxAppliedSettings{};
    std::array<float,8> screenOcclusionWindowBoundValues{},lastCapturedOcclusionWindow{};
    std::array<float,16> lastCapturedViewProjection{},lastCapturedUnjitteredViewProjection{};
    Vec2 lastCapturedTemporalJitter{};
    std::uint64_t lastCapturedRenderSerial{};
    unsigned outputWidth{},outputHeight{};
    uint32_t adapterLuidLow{};
    int32_t adapterLuidHigh{};
    bool adapterLuidValid{};
    CanvasViewport canvasViewport{};
    uint64_t resizeCount{};
    bool gpuValidationRequested{},gpuValidationAvailable{},gpuValidationExecuted{},gpuValidationPassed{},debugLayerEnabled{};
    uint64_t debugErrorCount{},debugWarningCount{};uint32_t debugFirstErrorId{};std::array<uint32_t,16> debugErrorIds{};std::array<std::string,16> debugErrorDescriptions{};size_t debugErrorIdCount{};
    CanonicalDrawFingerprintReceipt3D lastCanonicalDrawFingerprint{};
    DeterminismIntegrityReceipt3D lastDeterminismIntegrity{};
    std::array<uint32_t,32> debugWarningIds{};std::array<uint64_t,32> debugWarningOccurrences{};std::array<std::string,32> debugWarningDescriptions{};size_t debugWarningIdCount{};
    RendererQualitySettings3D requestedQuality{},effectiveQuality{};
    GpuInstanceCullingSettings3D gpuInstanceCulling{};
    HzbOcclusionSettings3D hzbOcclusion{};
    bool gpuInstanceCullingResourcesResident{};
    size_t gpuInstanceCullingResidentBytes{},lastGpuInstanceCullingCpuGroups{},lastGpuInstanceCullingStaticOpaqueColorGroups{},lastGpuInstanceCullingColorDirectGroups{},lastGpuInstanceCullingShadowDirectGroups{},lastGpuInstanceCullingAmbientOcclusionDirectGroups{},lastGpuInstanceCullingVelocityDirectGroups{},lastGpuInstanceCullingProbeDirectGroups{},lastGpuInstanceCullingCandidates{},lastGpuInstanceCullingVisible{},lastGpuInstanceCullingRejected{};
    size_t lastGpuInstanceSubmissionBuckets{},lastGpuInstanceSubmissionCommands{},lastGpuInstanceSubmissionCalls{},lastGpuInstanceSubmissionBindingSets{},gpuInstanceCommandTemplateBytes{};
    uint64_t lastGpuInstanceSubmissionCpuNanoseconds{};
    bool lastStaticMeshSubmissionWarmHit{},lastStaticMeshSubmissionCpuRebuild{},lastStaticMeshSubmissionCacheEligible{};size_t lastStaticMeshSubmissionCopiedBytes{},lastStaticMeshSubmissionInstanceCopiedBytes{},lastStaticMeshSubmissionGroupCopiedBytes{},lastStaticMeshSubmissionCommandCopiedBytes{},lastStaticMeshSubmissionPreparationIterations{};
    struct HlodCapacityReceipt3D {size_t consideredClusters{},acceptedClusters{},sourceFallbackClusters{},proxyFallbackClusters{},rejectedClusters{},droppedInstances{};uint64_t decisionHash{1469598103934665603ull};friend constexpr bool operator==(const HlodCapacityReceipt3D&,const HlodCapacityReceipt3D&)=default;} lastHlodCapacityReceipt{};
    uint64_t lastRenderPreparationCpuNanoseconds{},lastRenderCommandRecordingCpuNanoseconds{},lastRenderCommandRecordingCpuCycles{},lastRenderQueueExecuteCpuNanoseconds{},lastRenderFenceWaitCpuNanoseconds{},lastRenderPresentCpuNanoseconds{},lastRenderThreadCpuCycles{},performanceTelemetryMutationAllocations{};bool lastRenderThreadCpuQuerySucceeded{};
    PresentObservation3D lastPresentObservation{};
    // 僅量測最終 Bloom 合成區段，包含語意深度狀態轉換與統計更新。
    uint64_t lastBloomPostProcessCpuCycles{},lastBloomPostProcessCpuNanoseconds{};
    std::vector<uint32_t> lastGpuInstanceCullingGroupVisible{};
    std::vector<uint64_t> lastGpuInstanceCullingGroupIdentityHashes{};
    std::vector<std::vector<uint64_t>> lastGpuInstanceCullingGroupIdentities{};
    size_t gpuInstanceCullingValidationReadbackBytes{};
    uint64_t gpuInstanceCullingDispatches{},gpuInstanceIndirectDraws{},gpuInstanceDirectFallbackDraws{};
    uint64_t gpuHlodSelectionRequests{},gpuHlodSelectionDispatches{},gpuHlodSelectionCompletions{};size_t gpuHlodSelectionCandidateCount{},gpuHlodSelectionResidentBytes{};bool gpuHlodSelectionPending{};
    bool lastHzbOcclusionActive{};size_t hzbOcclusionResidentBytes{},lastHzbOccluderDraws{},lastHzbOccluderTriangles{},lastHzbCandidatesTested{},lastHzbOccluded{};uint64_t hzbDepthPrepasses{},hzbBuildDispatches{},lastHzbPrepassGpuNanoseconds{},lastHzbBuildGpuNanoseconds{},lastHzbCullGpuNanoseconds{},lastHzbColorGpuNanoseconds{};
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    struct HlodHzbIdentityReceipt3D {
        uint64_t submissionRevision{},readbackSerial{};size_t tested{},totalVisible{},hzbOccluded{};bool hzbActive{},complete{};
        std::vector<RenderObjectIdentity3D> candidates{},visible{};
    } lastHlodHzbIdentityReceipt{};
#endif
    uint64_t lastHzbSelectionCpuNanoseconds{},lastHzbPrepassRecordingCpuNanoseconds{},lastHzbBuildRecordingCpuNanoseconds{},lastHzbCullRecordingCpuNanoseconds{};
    uint64_t qualitySwitches{},qualitySwitchFlushes{},qualityResourceReleases{};
    bool linearWorldBlending{};
    size_t worldSrgbRenderTargetViews{};
    bool linearHdrSceneComposition{};
    size_t hdrSceneColorBytes{};
    uint64_t hdrSceneColorAllocations{};
    uint64_t hdrSceneColorReleases{};
    size_t nativeDepthBufferBytes{};
    uint64_t nativeDepthBufferAllocations{};
    uint64_t nativeDepthBufferReleases{};
    bool ambientOcclusionEnabled{};
    unsigned ambientOcclusionWidth{},ambientOcclusionHeight{};
    size_t ambientOcclusionBytes{};
    uint64_t ambientOcclusionAllocations{};
    uint64_t ambientOcclusionReleases{};
    uint64_t ambientOcclusionDepthPrepasses{};
    uint64_t ambientOcclusionEvaluationPasses{};
    uint64_t ambientOcclusionBlurPasses{};
    bool depthAwareWaterEnabled{};
    uint64_t depthAwareWaterPasses{};
    bool waterSingleCompositeEnabled{};
    uint64_t waterSingleCompositePasses{},waterSingleCompositeCopies{},waterSingleCompositeDraws{};
    bool waterAerialSingleCompositeOwnershipEnabled{};
    uint64_t waterAerialSingleCompositePasses{};
    uint32_t waterAerialOwnershipMutation{};
    bool screenSpaceWaterRefractionEnabled{};
    size_t waterOpaqueSceneColorBytes{};
    uint64_t waterOpaqueSceneColorAllocations{},waterOpaqueSceneColorReleases{},waterOpaqueSceneColorCopies{},screenSpaceWaterRefractionPasses{};
    bool screenSpaceReflectionsEnabled{};unsigned screenSpaceReflectionSampleCount{};size_t screenSpaceReflectionResidentBytes{};uint64_t screenSpaceReflectionPasses{},screenSpaceReflectionRaySteps{};
    uint64_t hdrPostProcessPasses{};
    bool bloomEnabled{},bloomDepthAwareEnabled{};uint64_t bloomPasses{},bloomTextureSamples{};size_t bloomResidentBytes{};
    BloomSemanticCapability3D bloomSemanticCapability{};BloomSemanticReceipt3D bloomSemanticReceipt{};
    bool volumetricCloudsEnabled{},volumetricCloudShadowsEnabled{};unsigned volumetricCloudSampleCount{};size_t volumetricCloudResidentBytes{},volumetricCloudShadowResidentBytes{};uint64_t volumetricCloudConfiguredFrames{},volumetricCloudShadowConfiguredFrames{};
    bool volumetricAtmosphereConfigured{},volumetricAtmosphereDispatched{},volumetricAtmosphereDiagnosticsAvailable{},volumetricAtmosphereDiagnosticsEnabled{},volumetricAtmosphereMutationAvailable{};
    VolumetricAtmospherePhase3D volumetricAtmospherePhase{VolumetricAtmospherePhase3D::Day};
    unsigned volumetricAtmosphereWidth{},volumetricAtmosphereHeight{};
    size_t volumetricAtmosphereResidentBytes{};
    uint64_t volumetricAtmosphereConfiguredFrames{},volumetricAtmosphereDispatches{},volumetricAtmosphereCompositePasses{},volumetricAtmosphereThreadGroups{},volumetricAtmosphereResourceCreates{},volumetricAtmosphereResourceReleases{},volumetricAtmosphereTemporalAcceptedPixels{},volumetricAtmosphereTemporalRejectedPixels{},volumetricAtmosphereTemporalInvalidPixels{},volumetricAtmosphereTemporalDiagnosticFrames{},lastVolumetricAtmosphereGpuNanoseconds{},volumetricAtmosphereAppliedFingerprint{};
    uint64_t volumetricAtmosphereSequenceDispatches{},volumetricAtmosphereSequenceReuses{},volumetricAtmosphereSequenceReuseRejects{},volumetricAtmosphereSequenceFingerprint{},volumetricAtmosphereSequenceOutputIdentity{},volumetricAtmosphereSequenceFence{};AtmosphereSequenceReuseReject3D volumetricAtmosphereLastSequenceReuseReject{AtmosphereSequenceReuseReject3D::None};bool volumetricAtmosphereSequenceReused{};
    uint64_t temporalSequenceDispatches{},temporalSequenceReuses{},temporalSequenceReuseRejects{},temporalSequenceFingerprint{},temporalSequenceOutputIdentity{},temporalSequenceFence{};uint32_t temporalHistoryResourceFormat{},temporalSequenceOutputFormat{};AtmosphereSequenceReuseReject3D temporalLastSequenceReuseReject{AtmosphereSequenceReuseReject3D::None};bool temporalSequenceReused{};
    unsigned volumetricAtmosphereAppliedCloudSamples{},volumetricAtmosphereAppliedShaftSamples{},volumetricAtmosphereAppliedShadowSamples{};float volumetricAtmosphereAppliedCloudMaximumDistance{},volumetricAtmosphereAppliedShaftMaximumDistance{},volumetricAtmosphereAppliedCloudShadowStrength{},volumetricAtmosphereAppliedShaftShadowStrength{};size_t volumetricAtmosphereBlueNoiseBytes{};
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    uint32_t volumetricAtmosphereAppliedTestMutation{};uint64_t volumetricAtmosphereAppliedFixturePatternHash{};unsigned volumetricAtmosphereAppliedFixtureWidth{},volumetricAtmosphereAppliedFixtureHeight{};bool volumetricAtmosphereAppliedResetHistory{},volumetricAtmosphereAppliedHistoryValid{};uint32_t volumetricAtmosphereAppliedTemporalSampleIndex{};
#endif
    bool directionalLightShaftsEnabled{};unsigned directionalLightShaftSampleCount{};size_t directionalLightShaftResidentBytes{};uint64_t directionalLightShaftConfiguredFrames{};
    bool skinSubsurfaceEnabled{};size_t skinSubsurfaceResidentBytes{};uint64_t skinSubsurfaceConfiguredFrames{};
    bool postProcessAntiAliasingEnabled{};
    uint64_t postProcessAntiAliasingPasses{},postProcessAntiAliasingTextureSamples{};
    bool temporalAntiAliasingEnabled{},temporalColorHistoryValid{},temporalDepthHistoryValid{},temporalJitterEnabled{},temporalHistoryUsed{};
    Vec2 currentTemporalJitterPixels{},previousTemporalJitterPixels{};
    unsigned temporalJitterSequenceIndex{};
    size_t temporalHistoryResidentBytes{};
    uint64_t temporalHistoryAllocations{},temporalHistoryReleases{},temporalResolvePasses{},temporalJitteredFrames{},temporalJitterResets{};
    size_t temporalDepthHistoryResidentBytes{};
    uint64_t temporalDepthHistoryAllocations{},temporalDepthHistoryReleases{},temporalDepthHistoryCopies{};
    bool multisampleSupported{},multisampleEnabled{};
    unsigned multisampleCount{1},multisampleQuality{};
    unsigned directionalShadowResolution{},directionalShadowCascadeCapacity{};
    float volumetricAtmosphereResolutionScale{.5f};
    size_t multisampleResidentBytes{};
    uint64_t multisampleAllocations{},multisampleReleases{},multisampleResolves{};
    size_t lastMultisampleMaskedDrawCalls{};
    uint64_t multisampleWaterDepthPrepasses{};
    size_t velocityResidentBytes{};
    uint64_t velocityAllocations{},velocityReleases{},velocityPasses{},velocityReadbackRequests{},velocityReadbackCompletions{},velocityReadbackRejections{},temporalHistoryResets{};
    bool temporalHistoryValid{};
    size_t sceneConstantBufferBytes{};
    size_t rootSignatureDwords{};
    float lastExposureCompensationEv{};
    TimeOfDayExposureReceipt3D timeOfDayExposureReceipt{};
    EnvironmentLightingBlendReceipt3D environmentLightingBlendReceipt{};
    size_t lastSubmittedVertices{};
    size_t lastSubmittedWorldVertices{};
    size_t lastSubmittedWorldIndices{};
    size_t lastUploadBytes{};
    size_t peakSubmittedVertices{};
    size_t peakSubmittedWorldVertices{};
    size_t peakSubmittedWorldIndices{};
    size_t totalDroppedVertices{};
    size_t totalDroppedWorldVertices{};
    size_t totalDroppedWorldIndices{};
    size_t lastSubmittedMeshInstances{};
    size_t lastSubmittedOverlayInstances{};
    size_t lastSubmittedShadowOnlyInstances{};
    size_t lastSubmittedTexturedMeshInstances{};
    size_t lastSubmittedNormalScaleOverrides{};
    size_t lastEffectiveNormalScaledMeshInstances{};
    size_t lastSubmittedTexturedSkinnedInstances{};
    size_t lastSubmittedSkinShadedInstances{};
    size_t lastSubmittedHairShadedInstances{};
    size_t lastSubmittedEyeShadedInstances{};
    size_t lastSubmittedClothShadedInstances{};
    size_t lastSubmittedMaskedSkinnedInstances{};
    size_t lastSubmittedDoubleSidedSkinnedInstances{};
    size_t lastSubmittedMaskedMeshInstances{};
    size_t lastSubmittedDoubleSidedMeshInstances{};
    size_t lastMaskedSkinnedShadowDrawCalls{};
    size_t lastMaskedSkinnedAmbientOcclusionDrawCalls{};
    size_t lastSkinnedMaterialDescriptorBinds{};
    uint64_t totalSkinnedMaterialDescriptorBinds{};
    uint64_t missingSkinnedTextureBindings{};
    uint64_t rejectedSkinnedAlphaBindings{};
    uint64_t rejectedStaticAlphaBindings{};
    size_t lastMaskedStaticShadowDrawCalls{};
    size_t lastMaskedStaticAmbientOcclusionDrawCalls{};
    size_t lastMaskedStaticVelocityDrawCalls{};
    size_t lastMaskedSkinnedVelocityDrawCalls{};
    size_t lastMaskedStaticSingleDepthDrawCalls{};
    size_t lastMaskedSkinnedSingleDepthDrawCalls{};
    size_t lastSubmittedFoliageInstances{};
    size_t lastSubmittedTerrainInstances{};
    size_t lastDirectionalShadowCasters{};
    size_t lastDirectionalShadowDrawCalls{};
    std::array<size_t,MaximumDirectionalShadowCascades3D> lastDirectionalShadowCascadeInstances{};
    std::array<size_t,MaximumDirectionalShadowCascades3D> lastDirectionalShadowCascadeDrawCalls{};
    size_t lastDirectionalShadowSharedInstances{};
    size_t lastDirectionalShadowInstancePassesSaved{};
    size_t lastDirectionalShadowAutomaticMasks{};
    size_t lastDirectionalShadowManualMasks{};
    std::array<size_t,MaximumDirectionalShadowCascades3D> lastDirectionalShadowLightSpaceRejected{};
    size_t lastDirectionalShadowMaskFallbacks{};
    size_t directionalShadowMapBytes{};
    uint64_t directionalShadowMapAllocations{},directionalShadowResourceGeneration{};
    uint64_t directionalShadowMapReleases{};
    uint64_t directionalShadowDescriptorSwitches{};
    uint64_t directionalShadowInitializationFlushes{};
    size_t lastDirectionalShadowCascades{};
    float directionalShadowCascadeSplit{32.f};
    std::array<float,MaximumDirectionalShadowCascades3D> directionalShadowCascadeSplits{};
    std::array<uint64_t,MaximumDirectionalShadowCascades3D> lastDirectionalShadowCascadeRendered{},lastDirectionalShadowCascadeReused{};uint64_t directionalShadowCascadeCacheInvalidations{};
    std::array<uint64_t,MaximumDirectionalShadowCascades3D> lastDirectionalShadowCascadeFingerprints{};
    std::array<uint32_t,MaximumDirectionalShadowCascades3D> lastDirectionalShadowCascadeInvalidationReasons{};
    DirectionalShadowFrameProvenance3D lastDirectionalShadowProvenance{};
    bool contactShadowsEnabled{},contactShadowTemporalFiltering{};uint32_t contactShadowMaximumSteps{};float contactShadowMaximumDistance{},contactShadowThickness{},contactShadowStrength{};uint64_t contactShadowFrames{},contactShadowFallbackFrames{},contactShadowEvaluationPasses{};
    uint64_t contactShadowTemporalDispatches{},contactShadowTemporalHistoryEligibleFrames{},contactShadowTemporalHistoryIneligibleFrames{},contactShadowTemporalHistoryResets{};
    size_t contactShadowTemporalResidentBytes{};uint64_t contactShadowTemporalAllocations{},contactShadowTemporalReleases{};
    bool forwardPlusEnabled{};uint32_t forwardPlusLightCount{},forwardPlusTileCountX{},forwardPlusTileCountY{};uint64_t forwardPlusDispatches{},forwardPlusAllocations{},forwardPlusReleases{},forwardPlusFingerprint{};size_t forwardPlusResidentBytes{};
    float directionalShadowTransitionWidth{6.f};
    size_t directionalShadowBasePcfSamples{12};
    size_t directionalShadowTransitionPcfSamples{24};
    bool softDirectionalShadowsEnabled{};
    size_t softDirectionalShadowBlockerSamples{};
    float softDirectionalShadowBlockerSearchRadiusTexels{};
    float softDirectionalShadowLightSourceRadiusTexels{};
    float softDirectionalShadowMaximumPenumbraRadiusTexels{};
    uint64_t successfulFrameCaptures{};
    uint64_t rejectedFrameCaptures{};
    // 最近正式擷取的 CPU 區段，以 submissionSerial 連結；不重用舊擷取假稱目前幀。
    struct FrameCapturePipelineCost3D {
        uint64_t submissionSerial{},fenceWaitNanoseconds{},mapNanoseconds{},analysisNanoseconds{},unmapNanoseconds{},totalNanoseconds{};
        FrameCaptureCost3D png{};
        bool complete{};
    } lastFrameCapturePipelineCost{};
    double lastCapturedAverageLuminance{};
    double lastCapturedLuminanceVariance{};
    double lastCapturedDarkPixelFraction{};
    double lastCapturedBrightPixelFraction{};
    size_t residentInstanceGroups{};
    size_t peakResidentInstanceGroups{};
    size_t residentInstanceGroupCapacityBytes{};
    size_t peakInstanceGroupCapacityBytes{};
    uint64_t instanceGroupCreations{};
    uint64_t instanceGroupEvictions{};
    uint64_t instanceGroupReleasedBytes{};
    uint64_t invalidMeshBoundsRejections{};
    size_t lastSubmittedPointLights{};
    size_t lastPointLightCandidates{};
    size_t peakSubmittedMeshInstances{};
    size_t totalDroppedMeshInstances{};
    size_t totalDroppedPointLights{};
    size_t lastInputTriangles2D{};
    size_t lastCulledTriangles2D{};
    size_t lastAdditiveTriangles2D{};
    size_t lastAdditiveDrawCalls2D{};
    uint64_t totalCulledTriangles2D{};
    uint64_t totalAdditiveTriangles2D{};
    size_t residentMeshes{};
    size_t residentMeshReferences{};
    size_t residentMeshBytes{};
    size_t residentMeshResources{};
    size_t peakResidentMeshResources{};
    uint64_t meshResourceAllocations{};
    size_t meshMemoryBudgetBytes{};
    uint64_t meshCacheHits{};
    uint64_t meshBudgetRejections{};
    size_t meshUploadStagingBytes{};
    size_t peakMeshUploadStagingBytes{};
    uint64_t meshUploads{};
    uint64_t meshUploadBatches{};
    uint64_t completedMeshUploadBatches{};
    uint64_t meshUploadContextCreations{};
    uint64_t meshUploadContextReuses{};
    size_t residentMeshUploadContexts{};
    size_t peakResidentMeshUploadContexts{};
    size_t idleMeshUploadContexts{};
    uint64_t meshUploadContextReleases{};
    uint64_t meshUploadPressureWaits{};
    uint64_t meshUploadPressureWaitNanoseconds{};
    size_t meshUploadStagingResources{};
    size_t peakMeshUploadStagingResources{};
    uint64_t meshUploadBufferAllocations{};
    size_t residentMaterialTextureSets{};
    size_t residentMaterialTextureReferences{};
    size_t residentTextureBytes{};
    size_t residentTextureAllocationBytes{};
    size_t peakResidentTextureAllocationBytes{};
    size_t residentMaterialTextureHeaps{};
    uint64_t materialTextureHeapAllocations{};
    size_t materialTextureMemoryBudgetBytes{};
    size_t peakMaterialTextureCpuStagingBytes{};
    size_t peakMaterialTextureCompressionStagingBytes{};
    size_t materialTextureUploadStagingBytes{};
    size_t peakMaterialTextureUploadStagingBytes{};
    uint64_t materialTextureUploads{};
    uint64_t materialTextureUploadBatches{};
    uint64_t completedMaterialTextureUploadBatches{};
    uint64_t materialTextureUploadContextCreations{};
    uint64_t materialTextureUploadContextReuses{};
    size_t residentMaterialTextureUploadContexts{};
    size_t peakResidentMaterialTextureUploadContexts{};
    size_t idleMaterialTextureUploadContexts{};
    uint64_t materialTextureUploadContextReleases{};
    uint64_t materialTextureUploadPressureWaits{};
    uint64_t materialTextureUploadPressureWaitNanoseconds{};
    size_t materialTextureUploadStagingResources{};
    size_t peakMaterialTextureUploadStagingResources{};
    uint64_t materialTextureUploadBufferAllocations{};
    uint64_t materialTextureCacheHits{};
    uint64_t materialTextureBudgetRejections{};
    size_t residentEnvironmentLightingSets{};
    size_t residentEnvironmentLightingBytes{};
    size_t peakResidentEnvironmentLightingBytes{};
    size_t pendingEnvironmentLightingReleaseBytes{};
    size_t environmentLightingUploadStagingBytes{};
    size_t peakEnvironmentLightingUploadStagingBytes{};
    size_t residentEnvironmentLightingDescriptors{};
    size_t environmentLightingFallbackDescriptors{};
    uint64_t environmentLightingUploads{};
    uint64_t environmentLightingResourceAllocations{};
    uint64_t environmentLightingResourceReleases{};
    uint64_t environmentLightingDescriptorSwitches{};
    uint64_t environmentLightingInvalidRejections{};
    uint64_t environmentLightingAtomicRollbacks{};
    uint64_t environmentLightingTrims{};
    bool environmentLightingActive{};
    bool localReflectionProbesEnabled{};size_t localReflectionProbeCount{},localReflectionProbeBufferBytes{};uint64_t localReflectionProbeSelections{},localReflectionProbeDraws{},localReflectionProbeBufferAllocations{},localReflectionProbeBufferReleases{};
    size_t reflectionProbeCaptureCount{},reflectionProbeCaptureResidentBytes{},reflectionProbeCaptureFilteredBytes{},reflectionProbeCaptureBudgetBytes{};uint64_t reflectionProbeCaptureRequests{},reflectionProbeCaptureRefreshes{},reflectionProbeCaptureFacesRendered{},reflectionProbeCaptureDraws{},reflectionProbeCaptureRecursionSuppressions{},reflectionProbeCaptureAllocations{},reflectionProbeCaptureReleases{},reflectionProbeCaptureBudgetRejections{},reflectionProbeCaptureReadbacks{},reflectionProbeCaptureDestroyWaits{},reflectionProbeCapturePrefilterPasses{},reflectionProbeCaptureIrradiancePasses{};
    uint64_t totalMaterialTextureDecodedBytes{};
    uint64_t totalMaterialTextureCompressedBytes{};
    uint64_t materialTexturePreflightRejectedBytes{};
    uint64_t materialDerivedDataCacheHits{};
    uint64_t materialDerivedDataCacheMisses{};
    uint64_t materialDerivedDataCacheInvalidations{};
    uint64_t materialDerivedDataCacheWrites{};
    uint64_t materialDerivedDataCacheReadBytes{};
    uint64_t materialDerivedDataCacheWrittenBytes{};
    size_t materialDerivedDataCacheResidentBytes{};
    size_t peakMaterialDerivedDataCacheResidentBytes{};
    uint64_t materialDerivedDataCacheEvictions{};
    uint64_t materialDerivedDataCacheReleasedBytes{};
    uint64_t materialDerivedDataCacheTrims{};
    size_t pendingGpuReleaseBytes{};
    size_t peakPendingGpuReleaseBytes{};
    uint64_t deferredGpuReleaseBatches{};
    uint64_t completedGpuReleaseBatches{};
    size_t dlssResidentBytes{};
    uint64_t dlssResourceAllocations{};
    uint64_t dlssResourceReleases{};
    bool dlssResourcesResident{};
    bool dlssModuleRequested{};
    bool dlssFrameworkLoaded{};
    size_t frameUploadResidentBytes{};
    size_t peakFrameUploadResidentBytes{};
    uint64_t frameUploadBufferAllocations{};
    uint64_t frameUploadBufferReleases{};
    uint64_t frameUploadGrowths{};
    uint64_t frameUploadTrims{};
    size_t directionalShadowPackedResidentBytes{};
    size_t lastDirectionalShadowPackedBytes{};
    size_t lastDirectionalShadowPackedInstances{};
    size_t lastDirectionalShadowPackedGroups{};
    uint64_t lastDirectionalShadowPackingCpuNanoseconds{};
    size_t lastDirectionalShadowMappedSourceViolations{};
    uint64_t directionalShadowPackedBufferAllocations{};
    uint64_t directionalShadowPackedBufferReleases{};
    uint64_t directionalShadowPackedBufferGrowths{};
    std::vector<uint8_t> lastPackedShadowCascadeMasks;
    uint64_t lastPackedShadowCascadeMaskHash{};
    size_t packedShadowCascadeMaskCapacityBytes{};
    uint32_t lastDrawCalls{};
    bool lastBackfaceCulling{};
    uint64_t submittedFrames{};
    uint64_t resourceWaits{};
    uint64_t validationWaits{};
    uint64_t lastResourceWaitNanoseconds{};
    uint64_t totalResourceWaitNanoseconds{};
    uint64_t peakResourceWaitNanoseconds{};
    uint64_t lastValidationWaitNanoseconds{};
    uint64_t totalValidationWaitNanoseconds{};
    uint64_t peakValidationWaitNanoseconds{};
    uint64_t gpuTimingSamples{};
    // 此收據於 timestamp serial 簽發時一次寫入，讓遙測可驗證提交槽位與實際 Bloom 模式屬於同一幀。
    struct SubmissionTelemetryReceipt3D {uint64_t serial{};uint32_t frameSlot{};bool bloomEnabled{},bloomDepthAwareEnabled{};friend constexpr bool operator==(const SubmissionTelemetryReceipt3D&,const SubmissionTelemetryReceipt3D&)=default;} lastSubmissionTelemetryReceipt{};
    // 完成收據保留原始 GPU timestamp，避免 benchmark 只能反推已換算的奈秒。
    struct CompletedGpuTimingReceipt3D {uint64_t serial{},beginTick{},endTick{},frequency{},nanoseconds{},bloomResourceIdentity{};[[nodiscard]] bool Complete()const noexcept{return serial&&endTick>=beginTick&&frequency&&nanoseconds&&bloomResourceIdentity;}} lastCompletedGpuTimingReceipt{};
    uint64_t completedGpuFrameSerial{};
    // 分段計時只隨同一完成幀發佈；沒有執行的 pass 不沿用上一幀數值。
    struct CompletedGpuPassTimingReceipt3D {
        uint64_t serial{}, beginTick{}, endTick{}, frequency{}, nanoseconds{};
        bool recorded{}, valid{};
        [[nodiscard]] static CompletedGpuPassTimingReceipt3D FromCompleted(
            uint64_t serial, uint64_t totalBegin, uint64_t totalEnd,
            uint64_t begin, uint64_t end, uint64_t frequency, bool recorded) noexcept {
            CompletedGpuPassTimingReceipt3D result{serial, begin, end, frequency, 0, recorded, false};
            if (!serial || !frequency || totalEnd < totalBegin || begin < totalBegin || end < begin || end > totalEnd) return result;
            result.valid = true;
            if (recorded) {
                const long double value = static_cast<long double>(end - begin) * 1000000000.0L / frequency;
                if (value >= static_cast<long double>((std::numeric_limits<uint64_t>::max)())) { result.valid = false; return result; }
                result.nanoseconds = static_cast<uint64_t>(value);
            }
            return result;
        }
    };
    CompletedGpuPassTimingReceipt3D lastShadowRenderingGpuTiming{}, lastProductionCaptureCopyGpuTiming{};
    // 同一完成幀的原始 GPU 時間戳，依命令錄製順序保存各階段邊界。
    struct CompletedGpuSceneTiming3D {
        uint64_t serial{};
        // 陰影起／訖、深度與 AO、不透明場景、體積霧、透明與後製、截圖起／訖。
        std::array<uint64_t,8> boundaries{};
    } lastCompletedGpuSceneTiming{};
    uint64_t lastGpuFrameNanoseconds{};
    uint64_t totalGpuFrameNanoseconds{};
    uint64_t peakGpuFrameNanoseconds{};
    size_t residentSkinnedMeshes{},residentSkinnedMeshReferences{},residentSkinnedMeshBytes{};
    size_t lastSubmittedSkinnedInstances{},lastUploadedSkinJoints{},skinPaletteResidentBytes{},peakSkinPaletteResidentBytes{};
    uint64_t skinnedMeshUploads{},skinnedMeshCacheHits{},skinnedMeshReleases{},skinPaletteUploads{},rejectedSkinnedInstances{},rejectedSkinPalettes{};
    size_t lastSubmittedSkinnedGroups{};
    uint64_t rejectedSkinnedGroups{},rejectedSkinnedGroupInstances{};
    uint64_t rejectedSkinnedCompatibilityBindings{},rejectedMissingSkeletonCompatibilityBindings{};
};
struct DevelopmentUltraRendererBaseline3D {
    bool shadowResident{},volumetricAtmosphereResident{};
    unsigned shadowResolution{},shadowCascadeCapacity{},volumetricAtmosphereWidth{},volumetricAtmosphereHeight{};
    float volumetricAtmosphereResolutionScale{.5f};
    size_t shadowResidentBytes{},volumetricAtmosphereResidentBytes{};
    uint64_t shadowAllocations{},shadowReleases{},shadowDescriptorSwitches{},shadowResourceGeneration{},volumetricAtmosphereResourceCreates{},volumetricAtmosphereResourceReleases{};
};
class DrawList {
public:
    static constexpr size_t MaximumSkinJointsPerFrame=4096,MaximumSkinPalettesPerFrame=64,MaximumSkinnedMeshInstancesPerFrame=512,MaximumLocalReflectionProbes=32;
    static constexpr size_t InitialVertexCapacity=32768;
    static constexpr size_t InitialWorldIndexCapacity=InitialVertexCapacity*3/2;
    static constexpr size_t InitialMeshInstanceCapacity=1024;
    static constexpr size_t InitialPointLightCapacity=16;
    static constexpr size_t InitialAdditiveSpanCapacity=32;
    static constexpr size_t MaxRadialLightsPerBatch=64;
    static constexpr size_t Minimal2DVertexCapacity=256;
    static constexpr size_t MaxSystemGlyphCacheEntries=512;
    static constexpr size_t MaxSystemGlyphCacheBytes=2*1024*1024;
    static constexpr size_t RetainedSystemGlyphsAfterTrim=64;
    explicit DrawList(DrawListCapacityProfile capacityProfile=DrawListCapacityProfile::Frame):capacityProfile_(capacityProfile){if(capacityProfile_==DrawListCapacityProfile::Frame){vertices.reserve(InitialVertexCapacity);worldVertices.reserve(InitialVertexCapacity);worldIndices.reserve(InitialWorldIndexCapacity);meshInstances.reserve(InitialMeshInstanceCapacity);pointLights_.reserve(InitialPointLightCapacity);additiveSpans_.reserve(InitialAdditiveSpanCapacity);}else vertices.reserve(Minimal2DVertexCapacity);}
    void Clear(){AbortPreparedWorldOriginShift3D();vertices.clear();worldVertices.clear();worldIndices.clear();meshInstances.clear();staticMeshSubmission_={};staticMeshSubmissionGeneration_=0;skinnedMeshInstances.clear();skinPaletteJoints.clear();localReflectionProbes_.clear();resourceLeases_.clear();resourceLeaseKeys_.clear();skinPaletteCount_=0;nextSkinnedSubmissionGroupId_=1;++skinPaletteGeneration_;if(!skinPaletteGeneration_)++skinPaletteGeneration_;pointLights_.clear();additiveSpans_.clear();blendMode_=BlendMode2D::Alpha;transform_={};transformCosine_=1.f;transformSine_=0.f;transformIdentity_=true;clipEnabled_=false;viewProjection_=Matrix4::Identity();screenOcclusionWindow_={};cameraCut3D_=false;performanceTelemetryEnabled3D_=false;presentObservationEnabled3D_=false;unsynchronizedDiagnosticPresent3D_=false;determinismIntegrityMode3D_=DeterminismIntegrityMode3D::Runtime;atmosphere_={};stats_.inputTriangles=stats_.submittedTriangles=stats_.culledTriangles=stats_.textGlyphs=stats_.textSourcePixels=stats_.textSpans=stats_.textContainmentTests=stats_.textContainedSpans=stats_.textContainmentBypassedTriangles=stats_.textCulledSpans=stats_.textCullingBypassedTriangles=stats_.primitiveCullTests=stats_.primitiveCulled=stats_.primitiveCulledTriangles=stats_.primitiveContained=stats_.primitiveContainedTriangles=stats_.ellipseSegments=stats_.ellipseOutlineSegments=stats_.ellipseTrigonometricEvaluations=stats_.ellipseTrigonometricEvaluationsSaved=stats_.ellipseStepCacheHits=stats_.ellipseStepCacheMisses=stats_.sharedVertexTransforms=stats_.sharedVertexTransformsSaved=stats_.primitiveColorPacks=stats_.primitiveColorPacksSaved=stats_.geometryBatches=stats_.geometryCulledBatches=stats_.geometryVertices=stats_.geometryTransformsSaved=stats_.additiveTriangles=stats_.additiveSpans=stats_.radialLightBatches=stats_.radialLightCandidates=stats_.radialLightsSubmitted=stats_.radialLightsCulled=stats_.radialLightsBudgetDropped=0;}
    void SetTranslation(Vec2 translation){transform_.translation=translation;UpdateTransformIdentity2D();}
    Vec2 Translation()const{return transform_.translation;}
    void SetTransform2D(Transform2D transform){transform_=transform;transformCosine_=std::cos(transform.rotation);transformSine_=std::sin(transform.rotation);UpdateTransformIdentity2D();}
    Transform2D CurrentTransform2D()const{return transform_;}
    void ResetTransform2D(){SetTransform2D({});}
    void SetClipRect(Vec2 minimum,Vec2 maximum){clipMinimum_={(std::min)(minimum.x,maximum.x),(std::min)(minimum.y,maximum.y)};clipMaximum_={(std::max)(minimum.x,maximum.x),(std::max)(minimum.y,maximum.y)};clipEnabled_=true;}
    void DisableClip(){clipEnabled_=false;}
    bool ClipEnabled()const{return clipEnabled_;}
    DrawListStats2D Stats2D()const{return stats_;}
    void ResetStats2D(){stats_={};}
    void TrimTransientMemory();
    [[nodiscard]] StaticMeshSubmission3D FreezeMeshInstances3D()const{return StaticMeshSubmission3D{meshInstances};}
    void SetStaticMeshSubmission3D(StaticMeshSubmission3D submission){meshInstances.clear();staticMeshSubmission_=std::move(submission);staticMeshSubmissionGeneration_=skinPaletteGeneration_;}
    void ClearStaticMeshSubmission3D(){staticMeshSubmission_={};staticMeshSubmissionGeneration_=0;}
    [[nodiscard]] std::span<const MeshInstance3D> EffectiveMeshInstances3D()const{return meshInstances.empty()&&staticMeshSubmission_&&staticMeshSubmissionGeneration_==skinPaletteGeneration_?staticMeshSubmission_.Instances():std::span<const MeshInstance3D>{meshInstances};}
    [[nodiscard]] uint64_t StaticMeshSubmissionRevision3D()const{return meshInstances.empty()&&staticMeshSubmission_&&staticMeshSubmissionGeneration_==skinPaletteGeneration_?staticMeshSubmission_.ContentRevision():0;}
    [[nodiscard]] uint64_t StaticMeshSubmissionLayoutRevision3D()const{return meshInstances.empty()&&staticMeshSubmission_&&staticMeshSubmissionGeneration_==skinPaletteGeneration_?staticMeshSubmission_.LayoutRevision():0;}
    [[nodiscard]] bool StaticMeshSubmissionCacheEligible3D()const{return meshInstances.empty()&&staticMeshSubmission_&&staticMeshSubmissionGeneration_==skinPaletteGeneration_&&staticMeshSubmission_.CacheEligible();}
    [[nodiscard]] size_t StaticMeshSubmissionRetainedBytes3D()const{return staticMeshSubmission_.RetainedBytes();}
    [[nodiscard]] TransientMemoryStats MemoryStats()const;
    [[nodiscard]] static SystemGlyphCacheStats2D SystemGlyphCacheStats();
    static size_t TrimSystemGlyphCache(size_t retainedGlyphs=RetainedSystemGlyphsAfterTrim);
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void WorldTriangle(Vec3 a,Vec3 b,Vec3 c,Color color);
    bool MeshInstance(MeshHandle3D mesh,Matrix4 transform,Color tint={1,1,1,1},Surface3D surface={},MeshRenderLayer3D layer=MeshRenderLayer3D::Opaque,MaterialTextureSetHandle3D textures={},float textureScale=.25f,bool useUv=false,MaterialClass3D materialClass=MaterialClass3D::Surface,float cameraDepth=std::numeric_limits<float>::quiet_NaN(),bool castsShadow=true,float lodCoverage=1.f,uint32_t lodSeed=0,bool lodInvert=false,uint8_t shadowCascadeMask=DirectionalShadowAllCascades3D,ShadowCascadeSelection3D shadowCascadeSelection=ShadowCascadeSelection3D::Automatic,Matrix4 previousTransform=Matrix4::Identity(),bool motionValid=false,float normalScale=1.f,MaterialAlphaMode3D alphaMode=MaterialAlphaMode3D::Opaque,float alphaCutoff=.5f,bool doubleSided=false,RenderObjectIdentity3D identity={},float occlusionOpacity=1.f){materialClass=NormalizeMaterialClass3D(materialClass);alphaMode=NormalizeMaterialAlphaMode3D(alphaMode);if(!mesh||alphaMode==MaterialAlphaMode3D::Blend||!std::isfinite(alphaCutoff)||!ValidOcclusionOpacity3D(occlusionOpacity)||(occlusionOpacity<1.f&&!identity)||(identity.id&&!identity.generation)||(alphaMode==MaterialAlphaMode3D::Mask&&(!textures||!useUv||(layer!=MeshRenderLayer3D::Opaque&&layer!=MeshRenderLayer3D::ShadowOnly))))return false;if(materialClass==MaterialClass3D::Water)layer=MeshRenderLayer3D::Transparent;if(!std::isfinite(cameraDepth))cameraDepth=TransformPoint(viewProjection_,TransformPoint(transform,{})).z;lodCoverage=Clamp(std::isfinite(lodCoverage)?lodCoverage:1.f,0.f,1.f);normalScale=Clamp(std::isfinite(normalScale)?normalScale:1.f,-16.f,15.96875f);lodSeed=(lodSeed&7u)|(lodInvert?8u:0u);shadowCascadeMask=castsShadow?(shadowCascadeMask&DirectionalShadowAllCascades3D):0u;if(!motionValid)previousTransform=transform;meshInstances.push_back({mesh,transform,CreateNormalMatrix(transform),PackColorRgba8(tint),PackSurface3D(surface),layer,textures,(std::max)(.001f,textureScale),normalScale,useUv,materialClass,cameraDepth,castsShadow&&shadowCascadeMask!=0,lodCoverage,lodSeed,shadowCascadeMask,shadowCascadeSelection,previousTransform,motionValid,alphaMode,Clamp(alphaCutoff,0.f,1.f),doubleSided,identity,occlusionOpacity});return true;}
    void RetainAssetLease(AssetLease3D lease){if(!lease)return;const AssetLeaseState3D* key=lease.get();const auto found=std::lower_bound(resourceLeaseKeys_.begin(),resourceLeaseKeys_.end(),key,std::less<const AssetLeaseState3D*>{});if(found!=resourceLeaseKeys_.end()&&*found==key)return;resourceLeaseKeys_.insert(found,key);resourceLeases_.push_back(std::move(lease));}
    [[nodiscard]] std::size_t AssetLeaseCount()const noexcept{return resourceLeases_.size();}
    SkinPaletteRange3D AddSkinPalette(std::span<const SkinPaletteJoint3D> joints,SkeletonCompatibilityId3D skeletonCompatibilityId={}){if(joints.empty()||skinPaletteCount_>=MaximumSkinPalettesPerFrame||joints.size()>MaximumSkinJointsPerFrame-skinPaletteJoints.size())return{};for(const SkinPaletteJoint3D& joint:joints){for(const auto& row:joint.skin.values)for(float value:row)if(!std::isfinite(value))return{};for(const Vec4& row:joint.normalRows)if(!std::isfinite(row.x)||!std::isfinite(row.y)||!std::isfinite(row.z)||!std::isfinite(row.w))return{};}const SkinPaletteRange3D range{uint32_t(skinPaletteJoints.size()),uint32_t(joints.size()),skinPaletteGeneration_,skeletonCompatibilityId};skinPaletteJoints.insert(skinPaletteJoints.end(),joints.begin(),joints.end());++skinPaletteCount_;return range;}
    struct SkinPaletteCheckpoint3D {std::size_t jointCount{},skinnedInstanceCount{};std::uint32_t paletteCount{},generation{};};
    SkinPaletteCheckpoint3D SkinPaletteCheckpoint()const noexcept{return{skinPaletteJoints.size(),skinnedMeshInstances.size(),skinPaletteCount_,skinPaletteGeneration_};}
    bool RestoreSkinPaletteCheckpoint(SkinPaletteCheckpoint3D checkpoint)noexcept{if(checkpoint.generation!=skinPaletteGeneration_||checkpoint.skinnedInstanceCount!=skinnedMeshInstances.size()||checkpoint.jointCount>skinPaletteJoints.size()||checkpoint.paletteCount>skinPaletteCount_)return false;skinPaletteJoints.resize(checkpoint.jointCount);skinPaletteCount_=checkpoint.paletteCount;return true;}
    uint32_t AllocateSkinnedSubmissionGroupId3D()noexcept{if(!nextSkinnedSubmissionGroupId_)nextSkinnedSubmissionGroupId_=1;return nextSkinnedSubmissionGroupId_++;}
    uint64_t AllocateSkinnedSubmissionNonce3D()noexcept{uint64_t value=nextSkinnedSubmissionNonce_.load(std::memory_order_relaxed);while(value&&value!=UINT64_MAX){if(nextSkinnedSubmissionNonce_.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;}return 0;}
    bool ContainsSkinPalette(SkinPaletteRange3D palette)const noexcept{return palette&&palette.generation==skinPaletteGeneration_&&palette.firstJoint<=skinPaletteJoints.size()&&palette.jointCount<=skinPaletteJoints.size()-palette.firstJoint;}
    uint32_t Generation3D()const noexcept{return skinPaletteGeneration_;}
    bool SkinnedMeshInstance(SkinnedMeshHandle3D mesh,SkinPaletteRange3D palette,Matrix4 transform,Color tint={1,1,1,1},Surface3D surface={},MaterialTextureSetHandle3D textures={},float textureScale=.25f,bool useUv=false,MaterialClass3D materialClass=MaterialClass3D::Surface,bool castsShadow=true,uint8_t shadowCascadeMask=DirectionalShadowAllCascades3D,uint32_t firstIndex=0,uint32_t indexCount=0,CharacterShadingModel3D shadingModel=CharacterShadingModel3D::Default,MaterialAlphaMode3D alphaMode=MaterialAlphaMode3D::Opaque,float alphaCutoff=.5f,bool doubleSided=false,SkinPaletteRange3D previousPalette={},Matrix4 previousTransform=Matrix4::Identity(),bool motionValid=false,float normalScale=1.f,float baseTextureScale=0.f,float detailTextureScale=0.f,uint32_t submissionGroupId=0,RenderObjectIdentity3D identity={},float occlusionOpacity=1.f){materialClass=NormalizeMaterialClass3D(materialClass);shadingModel=NormalizeCharacterShadingModel3D(shadingModel);alphaMode=NormalizeMaterialAlphaMode3D(alphaMode);const bool validPrevious=!motionValid||(previousPalette&&previousPalette.generation==skinPaletteGeneration_&&previousPalette.firstJoint<=skinPaletteJoints.size()&&previousPalette.jointCount<=skinPaletteJoints.size()-previousPalette.firstJoint);if(baseTextureScale<=0)baseTextureScale=textureScale;if(detailTextureScale<=0)detailTextureScale=textureScale;const bool validScales=std::isfinite(textureScale)&&textureScale>0&&std::isfinite(baseTextureScale)&&baseTextureScale>0&&std::isfinite(detailTextureScale)&&detailTextureScale>0;if(!ValidOcclusionOpacity3D(occlusionOpacity)||(occlusionOpacity<1.f&&!identity)||(identity.id&&!identity.generation)||!mesh||!palette||palette.generation!=skinPaletteGeneration_||palette.firstJoint>skinPaletteJoints.size()||palette.jointCount>skinPaletteJoints.size()-palette.firstJoint||!validPrevious||skinnedMeshInstances.size()>=MaximumSkinnedMeshInstancesPerFrame||firstIndex%3||indexCount%3||materialClass==MaterialClass3D::Water||materialClass==MaterialClass3D::Terrain||alphaMode==MaterialAlphaMode3D::Blend||!std::isfinite(alphaCutoff)||!std::isfinite(normalScale)||!validScales||(alphaMode==MaterialAlphaMode3D::Mask&&(!textures||!useUv)))return false;shadowCascadeMask=castsShadow?(shadowCascadeMask&DirectionalShadowAllCascades3D):0;if(!motionValid){previousPalette=palette;previousTransform=transform;}skinnedMeshInstances.push_back({mesh,palette,previousPalette,transform,previousTransform,CreateNormalMatrix(transform),PackColorRgba8(tint),PackSurface3D(surface),textures,(std::max)(.001f,textureScale),Clamp(normalScale,-16.f,15.96875f),(std::max)(.001f,baseTextureScale),(std::max)(.001f,detailTextureScale),firstIndex,indexCount,useUv,materialClass,shadingModel,alphaMode,Clamp(alphaCutoff,0.f,1.f),doubleSided,castsShadow&&shadowCascadeMask!=0,motionValid,shadowCascadeMask,submissionGroupId});skinnedMeshInstances.back().identity=identity;skinnedMeshInstances.back().occlusionOpacity=occlusionOpacity;return true;}
    ObjectOcclusionOpacityUpdate3D SetObjectOcclusionOpacity3D(RenderObjectIdentity3D identity,float opacity){ObjectOcclusionOpacityUpdate3D result{};if(!identity||!ValidOcclusionOpacity3D(opacity))return result;for(const MeshInstance3D& instance:meshInstances)if(instance.identity.id==identity.id&&instance.identity.generation!=identity.generation)++result.generationConflicts;for(const SkinnedMeshInstance3D& instance:skinnedMeshInstances)if(instance.identity.id==identity.id&&instance.identity.generation!=identity.generation)++result.generationConflicts;if(result.generationConflicts)return result;for(MeshInstance3D& instance:meshInstances)if(instance.identity==identity){instance.occlusionOpacity=opacity;++result.matchedStatic;}for(SkinnedMeshInstance3D& instance:skinnedMeshInstances)if(instance.identity==identity){instance.occlusionOpacity=opacity;++result.matchedSkinned;}result.matchedMultipart=result.matchedStatic+result.matchedSkinned>1?result.matchedStatic+result.matchedSkinned:0;result.missing=result.matchedStatic+result.matchedSkinned==0;return result;}
    void SetViewProjection(Matrix4 matrix){viewProjection_=matrix;}
    const Matrix4& ViewProjection()const{return viewProjection_;}
    void SetCameraCut3D(bool cut=true){cameraCut3D_=cut;}
    bool CameraCut3D()const{return cameraCut3D_;}
    void SetPerformanceTelemetryEnabled3D(bool enabled){performanceTelemetryEnabled3D_=enabled;}
    bool PerformanceTelemetryEnabled3D()const{return performanceTelemetryEnabled3D_;}
    void SetPresentObservationEnabled3D(bool enabled){presentObservationEnabled3D_=enabled;if(!enabled)unsynchronizedDiagnosticPresent3D_=false;}
    bool PresentObservationEnabled3D()const{return presentObservationEnabled3D_;}
    bool SetUnsynchronizedDiagnosticPresent3D(bool enabled)noexcept{if(enabled&&!presentObservationEnabled3D_)return false;unsynchronizedDiagnosticPresent3D_=enabled;return true;}
    bool UnsynchronizedDiagnosticPresent3D()const noexcept{return unsynchronizedDiagnosticPresent3D_;}
    bool SetDeterminismIntegrityMode3D(DeterminismIntegrityMode3D mode)noexcept{if(mode!=DeterminismIntegrityMode3D::Runtime&&mode!=DeterminismIntegrityMode3D::SealedCapture)return false;determinismIntegrityMode3D_=mode;return true;}
    [[nodiscard]] DeterminismIntegrityMode3D DeterminismIntegrityMode()const noexcept{return determinismIntegrityMode3D_;}
    void SetOriginGeneration3D(uint64_t generation){originGeneration3D_=generation;}
    uint64_t OriginGeneration3D()const{return originGeneration3D_;}
    // 原點切換會失效跨幀歷史，並將所有渲染環境座標移入新的本地空間。
    bool PrepareWorldOriginShift3D(Vec3 delta,uint64_t generation){
        const auto finiteMatrix=[](const Matrix4& matrix){for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;return true;};
        const auto finitePoint=[](Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);};
        if(!generation||generation==originGeneration3D_||!finitePoint(delta))return false;
        const Matrix4 translation=CreateTranslationMatrix(delta*-1.f);
        if(!finitePoint(atmosphere_.cameraPosition-delta)||!std::isfinite(atmosphere_.heightFogBaseHeight-delta.y)||!std::isfinite(atmosphere_.aerialPerspective.baseHeight-delta.y)||!std::isfinite(volumetricClouds_.baseAltitude-delta.y))return false;
        for(const auto& probe:localReflectionProbes_)if(!finitePoint(probe.minimum-delta)||!finitePoint(probe.maximum-delta)||!finitePoint(probe.capturePosition-delta))return false;
        for(const auto& instance:meshInstances)if(!finiteMatrix(translation*instance.transform))return false;
        for(const auto& instance:skinnedMeshInstances)if(!finiteMatrix(translation*instance.transform))return false;
        StaticMeshSubmission3D prepared{};
        if(meshInstances.empty()&&staticMeshSubmission_)try{
            std::vector<MeshInstance3D> shifted(staticMeshSubmission_.Instances().begin(),staticMeshSubmission_.Instances().end());
            for(auto& instance:shifted){instance.transform=translation*instance.transform;if(!finiteMatrix(instance.transform))return false;instance.normalTransform=CreateNormalMatrix(instance.transform);if(!finiteMatrix(instance.normalTransform))return false;instance.previousTransform=instance.transform;instance.motionValid=false;}
            prepared=StaticMeshSubmission3D(shifted);
        }catch(...){return false;}
        preparedStaticMeshSubmission3D_=std::move(prepared);preparedOriginGeneration3D_=generation;preparedOriginDelta3D_=delta;return true;
    }
    bool CommitPreparedWorldOriginShift3D()noexcept{if(!preparedOriginGeneration3D_)return false;const Vec3 delta=preparedOriginDelta3D_;const Matrix4 translation=CreateTranslationMatrix(delta*-1.f);if(preparedStaticMeshSubmission3D_){staticMeshSubmission_=std::move(preparedStaticMeshSubmission3D_);staticMeshSubmissionGeneration_=skinPaletteGeneration_;}for(auto& instance:meshInstances){instance.transform=translation*instance.transform;instance.normalTransform=CreateNormalMatrix(instance.transform);instance.previousTransform=instance.transform;instance.motionValid=false;}for(auto& instance:skinnedMeshInstances){instance.transform=translation*instance.transform;instance.normalTransform=CreateNormalMatrix(instance.transform);instance.previousTransform=instance.transform;instance.previousPalette=instance.palette;instance.motionValid=false;}originGeneration3D_=preparedOriginGeneration3D_;preparedOriginGeneration3D_=0;preparedOriginDelta3D_={};cameraCut3D_=true;atmosphere_.cameraPosition=atmosphere_.cameraPosition-delta;atmosphere_.heightFogBaseHeight-=delta.y;atmosphere_.aerialPerspective.baseHeight-=delta.y;volumetricClouds_.baseAltitude-=delta.y;for(auto& probe:localReflectionProbes_){probe.minimum=probe.minimum-delta;probe.maximum=probe.maximum-delta;probe.capturePosition=probe.capturePosition-delta;}return true;}
    void AbortPreparedWorldOriginShift3D()noexcept{preparedStaticMeshSubmission3D_={};preparedOriginGeneration3D_=0;preparedOriginDelta3D_={};}
    bool ApplyWorldOriginShift3D(Vec3 delta,uint64_t generation){return PrepareWorldOriginShift3D(delta,generation)&&CommitPreparedWorldOriginShift3D();}
    void SetDirectionalLight(DirectionalLight3D light){light.direction=Normalize(light.direction);if(Length(light.direction)<.0001f)light.direction={0,-1,0};light_=light;}
    const DirectionalLight3D& DirectionalLight()const{return light_;}
    void SetDirectionalShadowMapEnabled(bool enabled){directionalShadowMapEnabled_=enabled;}
    bool DirectionalShadowMapEnabled()const{return directionalShadowMapEnabled_;}
    void AddPointLight(PointLight3D light){if(!ValidPointLight3D(light))return;if(pointLights_.size()<MaximumForwardPlusPointLights3D){pointLights_.push_back(light);return;}const auto worst=std::max_element(pointLights_.begin(),pointLights_.end(),PointLightCanonicalLess3D);if(worst!=pointLights_.end()&&PointLightCanonicalLess3D(light,*worst))*worst=light;}
    const std::vector<PointLight3D>& PointLights()const{return pointLights_;}
    void SetAtmosphere(Atmosphere3D atmosphere){atmosphere_=NormalizeAtmosphere3D(atmosphere);}
    const Atmosphere3D& Atmosphere()const{return atmosphere_;}
    void SetEnvironmentTime(float seconds){environmentTime_=std::isfinite(seconds)?seconds:0.f;}
    float EnvironmentTime()const{return environmentTime_;}
    void SetVolumetricAtmospherePhase(VolumetricAtmospherePhase3D phase){volumetricAtmospherePhase_=std::uint32_t(phase)<=std::uint32_t(VolumetricAtmospherePhase3D::Night)?phase:VolumetricAtmospherePhase3D::Day;}
    VolumetricAtmospherePhase3D VolumetricAtmospherePhase()const{return volumetricAtmospherePhase_;}
    void SetExposureCompensation(float ev){exposureCompensationEv_=(std::clamp)(std::isfinite(ev)?ev:0.f,-10.f,10.f);}
    float ExposureCompensation()const{return exposureCompensationEv_;}
    void SetTimeOfDayExposure(TimeOfDayExposureSettings3D settings){timeOfDayExposure_=NormalizeTimeOfDayExposureSettings3D(settings);}
    const TimeOfDayExposureSettings3D& TimeOfDayExposure()const{return timeOfDayExposure_;}
    bool SetEnvironmentLightingBlend(EnvironmentLightingBlendSettings3D settings){EnvironmentLightingBlendSettings3D normalized{};if(!NormalizeEnvironmentLightingBlendSettings3D(settings,normalized))return false;environmentLightingBlend_=normalized;return true;}
    const EnvironmentLightingBlendSettings3D& EnvironmentLightingBlend()const{return environmentLightingBlend_;}
    bool SetPhaseRadianceProfile(PhaseRadianceProfile3D profile){PhaseRadianceProfile3D normalized{};if(!NormalizePhaseRadianceProfile3D(profile,normalized))return false;phaseRadianceProfile_=normalized;return true;}
    const PhaseRadianceProfile3D& PhaseRadianceProfile()const{return phaseRadianceProfile_;}
    void SetBloom(BloomSettings3D settings){bloom_=NormalizeBloomSettings3D(settings);}
    const BloomSettings3D& Bloom()const{return bloom_;}
    void SetWaterOptics(WaterOpticsSettings3D settings){waterOptics_=NormalizeWaterOpticsSettings3D(settings);}
    const WaterOpticsSettings3D& WaterOptics()const{return waterOptics_;}
    void SetWaterSurface(WaterSurfaceSettings3D settings){waterSurface_=NormalizeWaterSurfaceSettings3D(settings);}
    const WaterSurfaceSettings3D& WaterSurface()const{return waterSurface_;}
    WaterSurfaceReceipt3D WaterSurfaceReceipt()const{return{waterSurface_,WaterSurfaceFingerprint3D(waterSurface_),true,waterSurface_.enabled,waterSurface_.enabled};}
    void SetScreenSpaceReflections(ScreenSpaceReflectionSettings3D settings){screenSpaceReflections_=NormalizeScreenSpaceReflectionSettings3D(settings);}
    const ScreenSpaceReflectionSettings3D& ScreenSpaceReflections()const{return screenSpaceReflections_;}
    bool AddLocalReflectionProbe(LocalReflectionProbe3D probe){if(localReflectionProbes_.size()>=MaximumLocalReflectionProbes||!ValidLocalReflectionProbe3D(probe))return false;localReflectionProbes_.push_back(probe);return true;}
    bool RemoveLocalReflectionProbe(size_t index){if(index>=localReflectionProbes_.size())return false;localReflectionProbes_.erase(localReflectionProbes_.begin()+index);return true;}
    bool MoveLocalReflectionProbe(size_t from,size_t to){if(from>=localReflectionProbes_.size()||to>=localReflectionProbes_.size())return false;if(from==to)return true;const auto probe=localReflectionProbes_[from];localReflectionProbes_.erase(localReflectionProbes_.begin()+from);localReflectionProbes_.insert(localReflectionProbes_.begin()+to,probe);return true;}
    std::span<const LocalReflectionProbe3D> LocalReflectionProbes()const{return localReflectionProbes_;}
    void SetTerrainDistanceMaterial(TerrainDistanceMaterialSettings3D settings){terrainDistanceMaterial_=NormalizeTerrainDistanceMaterialSettings3D(settings);}
    const TerrainDistanceMaterialSettings3D& TerrainDistanceMaterial()const{return terrainDistanceMaterial_;}
    void SetTerrainParallax(TerrainParallaxSettings3D settings){terrainParallax_=NormalizeTerrainParallaxSettings3D(settings);}
    const TerrainParallaxSettings3D& TerrainParallax()const{return terrainParallax_;}
    // 明確的frame政策：將此frame所有部分occlusion opacity限制在主體窗口；預設關閉。
    void SetScreenOcclusionWindow(ScreenOcclusionWindow3D settings){screenOcclusionWindow_=NormalizeScreenOcclusionWindow3D(settings);}
    const ScreenOcclusionWindow3D& ScreenOcclusionWindow()const{return screenOcclusionWindow_;}
    void SetVolumetricClouds(VolumetricCloudSettings3D settings){volumetricClouds_=NormalizeVolumetricCloudSettings3D(settings);}
    const VolumetricCloudSettings3D& VolumetricClouds()const{return volumetricClouds_;}
    void SetDirectionalLightShafts(DirectionalLightShaftSettings3D settings){directionalLightShafts_=NormalizeDirectionalLightShaftSettings3D(settings);}
    const DirectionalLightShaftSettings3D& DirectionalLightShafts()const{return directionalLightShafts_;}
    void SetSkinSubsurface(SkinSubsurfaceSettings3D settings){skinSubsurface_=NormalizeSkinSubsurfaceSettings3D(settings);}
    const SkinSubsurfaceSettings3D& SkinSubsurface()const{return skinSubsurface_;}
    void SetAmbientOcclusion(AmbientOcclusionSettings3D settings){ambientOcclusion_=NormalizeAmbientOcclusionSettings3D(settings);}
    const AmbientOcclusionSettings3D& AmbientOcclusion()const{return ambientOcclusion_;}
    void SetSoftDirectionalShadows(SoftDirectionalShadowSettings3D settings){softDirectionalShadows_=NormalizeSoftDirectionalShadowSettings3D(settings);}
    const SoftDirectionalShadowSettings3D& SoftDirectionalShadows()const{return softDirectionalShadows_;}
    void SetDirectionalShadowCascades(DirectionalShadowCascadeSettings3D settings){directionalShadowCascades_=NormalizeDirectionalShadowCascadeSettings3D(settings);}
    const DirectionalShadowCascadeSettings3D& DirectionalShadowCascades()const{return directionalShadowCascades_;}
    void SetContactShadows(ContactShadowSettings3D settings){contactShadows_=NormalizeContactShadowSettings3D(settings);}
    const ContactShadowSettings3D& ContactShadows()const{return contactShadows_;}
    void SetBackfaceCulling(bool enabled){backfaceCulling_=enabled;}
    bool BackfaceCulling()const{return backfaceCulling_;}
    void SetBlendMode2D(BlendMode2D mode){blendMode_=mode;}
    BlendMode2D CurrentBlendMode2D()const{return blendMode_;}
    std::span<const DrawSpan2D> AdditiveSpans2D()const{return additiveSpans_;}
    void Quad(Vec2 center,Vec2 size,Color color);
    void GradientQuad(Vec2 center,Vec2 size,Color top,Color bottom);
    void Diamond(Vec2 center,float rx,float ry,Color color);
    void IsoPrism(Vec2 center,float rx,float ry,float height,Color top,Color left,Color right);
    void IsoFrustum(Vec2 center,float rx,float ry,float topScale,float height,Color top,Color lightSide,Color darkSide,int segments=14);
    void Circle(Vec2 center,float radius,Color color,int segments=18);
    void Ellipse(Vec2 center,float rx,float ry,Color color,int segments=24);
    void RadialGradientEllipse(Vec2 center,float rx,float ry,Color inner,Color outer,int segments=24);
    void RadialLights(std::span<const RadialLight2D> lights,size_t maximumLights=MaxRadialLightsPerBatch);
    void EllipseOutline(Vec2 center,float rx,float ry,float width,Color color,int segments=24);
    void ProjectedEllipseShadow(Vec2 base,float rx,float ry,Vec2 lightDirection,float height,float opacity=.4f,int segments=18);
    void Line(Vec2 a,Vec2 b,float width,Color color);
    void Geometry(const Geometry2D& geometry);
    void Text(Vec2 pos,std::string_view text,float scale,Color color);
    Vec2 MeasureText(std::string_view text,float scale)const;
    // 選用 Windows 系統字型；pixelHeight 為字型像素尺度，保留舊 Text 外觀。
    void SystemText(Vec2 pos,std::string_view text,float pixelHeight,Color color);
    Vec2 MeasureSystemText(std::string_view text,float pixelHeight)const;
    std::vector<Vertex> vertices;
    std::vector<Vertex3D> worldVertices;
    std::vector<uint32_t> worldIndices;
    std::vector<MeshInstance3D> meshInstances;
    std::vector<SkinnedMeshInstance3D> skinnedMeshInstances;
    std::vector<SkinPaletteJoint3D> skinPaletteJoints;
private:
    StaticMeshSubmission3D staticMeshSubmission_{};uint32_t staticMeshSubmissionGeneration_{};
    enum class PrimitiveClipResult:uint8_t {Culled,Contained,Partial};
    struct EllipseStepCacheEntry {int segments{};float cosine{},sine{};uint64_t lastUsed{};};
    struct EllipseStepResult {float cosine{},sine{};bool cached{};};
    void UpdateTransformIdentity2D(){transformIdentity_=transform_.translation.x==0.f&&transform_.translation.y==0.f&&transform_.scale.x==1.f&&transform_.scale.y==1.f&&transform_.rotation==0.f;}
    Vec2 ApplyTransform2D(Vec2 point)const{if(transformIdentity_)return point;const Vec2 local{(point.x-transform_.pivot.x)*transform_.scale.x,(point.y-transform_.pivot.y)*transform_.scale.y};return{local.x*transformCosine_-local.y*transformSine_+transform_.pivot.x+transform_.translation.x,local.x*transformSine_+local.y*transformCosine_+transform_.pivot.y+transform_.translation.y};}
    PrimitiveClipResult ClassifyTransformedBounds(Vec2 minimum,Vec2 maximum)const;
    void SubmitTransformedTriangle(Vec2 a,Vec2 b,Vec2 c,float alpha,uint32_t packedColor);
    void SubmitContainedTriangle(Vec2 a,Vec2 b,Vec2 c,uint32_t packedColor);
    void SubmitGradientTriangle(Vec2 a,uint32_t colorA,Vec2 b,uint32_t colorB,Vec2 c,uint32_t colorC,bool contained);
    void SubmitContainedTextQuad(Vec2 center,Vec2 size,Color color);
    void CullTextQuad();
    void RecordSharedVertexTransforms(size_t transforms,size_t saved);
    void RecordPrimitiveColorPacks(size_t packs,size_t saved);
    void RecordSubmittedVertices(size_t firstVertex,size_t vertexCount);
    EllipseStepResult EllipseStep(int segments);
    PrimitiveClipResult ClassifyTransformedPrimitive(std::span<const Vec2> points,float alpha,size_t triangles);
    PrimitiveClipResult ClassifyTessellatedPrimitive(Vec2 minimum,Vec2 maximum,Color color,size_t triangles);
    Transform2D transform_{};
    float transformCosine_{1.f},transformSine_{};
    bool transformIdentity_{true};
    Matrix4 viewProjection_{Matrix4::Identity()};
    DirectionalLight3D light_{};
    Atmosphere3D atmosphere_{};
    float environmentTime_{};
    VolumetricAtmospherePhase3D volumetricAtmospherePhase_{VolumetricAtmospherePhase3D::Day};
    float exposureCompensationEv_{};
    TimeOfDayExposureSettings3D timeOfDayExposure_{};
    EnvironmentLightingBlendSettings3D environmentLightingBlend_{};
    PhaseRadianceProfile3D phaseRadianceProfile_{};
    BloomSettings3D bloom_{};
    WaterOpticsSettings3D waterOptics_{};
    WaterSurfaceSettings3D waterSurface_{};
    ScreenSpaceReflectionSettings3D screenSpaceReflections_{};
    std::vector<LocalReflectionProbe3D> localReflectionProbes_{};
    TerrainDistanceMaterialSettings3D terrainDistanceMaterial_{};
    TerrainParallaxSettings3D terrainParallax_{};
    ScreenOcclusionWindow3D screenOcclusionWindow_{};
    VolumetricCloudSettings3D volumetricClouds_{};
    DirectionalLightShaftSettings3D directionalLightShafts_{};
    SkinSubsurfaceSettings3D skinSubsurface_{};
    AmbientOcclusionSettings3D ambientOcclusion_{};
    SoftDirectionalShadowSettings3D softDirectionalShadows_{};
    DirectionalShadowCascadeSettings3D directionalShadowCascades_{2,2048,.1f,180.f,.65f,.08f};
    ContactShadowSettings3D contactShadows_{};
    std::vector<PointLight3D> pointLights_;
    std::vector<DrawSpan2D> additiveSpans_;
    std::vector<AssetLease3D> resourceLeases_;
    std::vector<const AssetLeaseState3D*> resourceLeaseKeys_;
    Vec2 clipMinimum_{},clipMaximum_{};
    bool clipEnabled_{};
    DrawListStats2D stats_{};
    size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
    uint32_t skinPaletteGeneration_{1},skinPaletteCount_{},nextSkinnedSubmissionGroupId_{1};
    inline static std::atomic<uint64_t> nextSkinnedSubmissionNonce_{1};
    std::array<EllipseStepCacheEntry,4> ellipseStepCache_{};
    uint64_t ellipseStepCacheClock_{};
    bool backfaceCulling_{true};
    bool directionalShadowMapEnabled_{true};
    bool cameraCut3D_{},performanceTelemetryEnabled3D_{},presentObservationEnabled3D_{},unsynchronizedDiagnosticPresent3D_{};DeterminismIntegrityMode3D determinismIntegrityMode3D_{DeterminismIntegrityMode3D::Runtime};uint64_t originGeneration3D_{},preparedOriginGeneration3D_{};Vec3 preparedOriginDelta3D_{};StaticMeshSubmission3D preparedStaticMeshSubmission3D_{};
    BlendMode2D blendMode_{BlendMode2D::Alpha};
    DrawListCapacityProfile capacityProfile_{DrawListCapacityProfile::Frame};
};
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
struct D3D12RendererTestAccess3D;
#endif
class D3D12Renderer {
public:
    static constexpr unsigned BufferedFrames=3;
    static constexpr size_t MaxVertices=240000;
    static constexpr size_t MaxWorldVertices=240000;
    static constexpr size_t MaxWorldIndices=480000;
    static constexpr size_t MaxMeshInstances=MaximumMeshInstancesPerFrame3D;
    static constexpr uint64_t InstanceGroupRetentionFrames=300;
    static constexpr size_t MaxPointLights=MaximumForwardPlusPointLights3D;
    static constexpr size_t MaxMaterialTextureSets=64;
    static constexpr size_t InitialFrameVertices=DrawList::InitialVertexCapacity;
    static constexpr size_t InitialFrameWorldVertices=DrawList::InitialVertexCapacity;
    static constexpr size_t InitialFrameWorldIndices=DrawList::InitialWorldIndexCapacity;
    static constexpr size_t InitialFrameMeshInstances=DrawList::InitialMeshInstanceCapacity;
    static constexpr size_t DefaultMeshMemoryBudgetBytes=128*1024*1024;
    static constexpr size_t DefaultMaterialTextureMemoryBudgetBytes=256*1024*1024;
    static constexpr size_t DefaultMaterialDerivedDataCacheBudgetBytes=512*1024*1024;
    static constexpr size_t MaxMeshUploadBatchBytes=32*1024*1024;
    static constexpr size_t MaxMeshUploadBatchMeshes=256;
    static constexpr size_t MaxMeshUploadStagingBytes=64*1024*1024;
    static constexpr size_t MaxMaterialTextureUploadBatchBytes=32*1024*1024;
    static constexpr size_t MaxMaterialTextureUploadBatchTextures=48;
    static constexpr size_t MaxMaterialTextureUploadStagingBytes=96*1024*1024;
    static constexpr size_t MeshInstanceUploadBytes=sizeof(Matrix4)+sizeof(float)*13+sizeof(uint32_t)*6;
    static constexpr size_t ShadowInstanceUploadBytes=80;
    static constexpr size_t MotionInstanceUploadBytes=sizeof(Matrix4);
    static constexpr size_t DrawableVertexCount(size_t requested){const size_t limited=requested<MaxVertices?requested:MaxVertices;return limited-limited%3;}
    static constexpr size_t DrawableWorldVertexCount(size_t requested){return requested<MaxWorldVertices?requested:MaxWorldVertices;}
    static constexpr size_t NextFrameUploadCapacity(size_t current,size_t required,size_t maximum){size_t capacity=(std::max)(size_t{1},current);while(capacity<required&&capacity<maximum)capacity=capacity>maximum/2?maximum:capacity*2;return(std::min)(capacity,maximum);}
    static constexpr size_t Initial2DFrameUploadBytes=BufferedFrames*InitialFrameVertices*sizeof(Vertex);
    static constexpr size_t Initial3DFrameUploadBytes=BufferedFrames*(InitialFrameWorldVertices*sizeof(Vertex3D)+InitialFrameWorldIndices*sizeof(uint32_t)+InitialFrameMeshInstances*(MeshInstanceUploadBytes+ShadowInstanceUploadBytes));
    static constexpr size_t InitialMotionFrameUploadBytes=BufferedFrames*InitialFrameMeshInstances*MotionInstanceUploadBytes;
    static constexpr size_t InitialFrameUploadBytes=Initial2DFrameUploadBytes+Initial3DFrameUploadBytes;
    static size_t DrawableWorldIndexCount(const std::vector<uint32_t>& indices,size_t vertexCount);
    D3D12Renderer(); ~D3D12Renderer();
    void Initialize(HWND hwnd,unsigned width,unsigned height,unsigned logicalWidth=1280,unsigned logicalHeight=720,bool enableDLSS=false);
    void Resize(unsigned width,unsigned height);
    void Render(const DrawList& list,Color clear);
    bool CaptureNextFrame(const std::filesystem::path& path);
    bool RequestVelocityReadback3D();
    std::optional<VelocityCapture3D> TakeVelocityReadback3D();
    bool RequestObjectIdReadback3D(ObjectIdCaptureRequestMetadata3D metadata={});
    bool CancelObjectIdReadback3D()noexcept;
    std::optional<ObjectIdCapture3D> TakeObjectIdReadback3D();
    bool RequestDepthReadback3D(DepthCaptureRequestMetadata3D metadata={});
    bool CancelDepthReadback3D()noexcept;
    std::optional<DepthCapture3D> TakeDepthReadback3D();
    bool RequestLinearHdrReadback3D(LinearHdrCaptureRequestMetadata3D metadata={});
    bool CancelLinearHdrReadback3D()noexcept;
    std::optional<LinearHdrReadback3D> TakeLinearHdrReadback3D();
    std::optional<ProductionReadbackBundle3D> TakeProductionReadbackBundle3D();
    std::optional<ReadbackCaptureSequenceToken3D> BeginReadbackCaptureSequence3D(ReadbackCaptureSequenceDescriptor3D descriptor);
    std::optional<ReadbackCaptureSequenceToken3D> BeginReadbackCaptureSequence3D(uint32_t expectedPhaseCount){return BeginReadbackCaptureSequence3D(ReadbackCaptureSequenceDescriptor3D{expectedPhaseCount});}
    ReadbackCaptureSequenceResult3D EndReadbackCaptureSequence3D(ReadbackCaptureSequenceToken3D token);
    ReadbackCaptureSequenceGpuProvenance3D ReadbackCaptureSequenceGpuProvenance(ReadbackCaptureSequenceToken3D token)const;
    bool CancelReadbackCaptureSequence3D(ReadbackCaptureSequenceToken3D token)noexcept;
    MeshHandle3D CreateMesh(std::span<const Vertex3D> vertices,std::span<const uint32_t> indices);
    void DestroyMesh(MeshHandle3D mesh);
    SkinnedMeshHandle3D CreateSkinnedMesh(std::span<const SkinnedVertex3D> vertices,std::span<const uint32_t> indices,uint32_t jointCount,SkeletonCompatibilityId3D skeletonCompatibilityId={});
    void DestroySkinnedMesh(SkinnedMeshHandle3D mesh);
    void SetMeshMemoryBudget(size_t bytes);
    MaterialTextureSetHandle3D CreateMaterialTextureSet(const MaterialTextureSetDefinition3D& definition);
    [[nodiscard]] std::optional<size_t> EstimateMaterialTextureSetAllocationBytes3D(const MaterialTextureSetDefinition3D& definition)const;
    GpuUploadReceipt3D SubmitPendingUploads3D();
    [[nodiscard]] bool IsUploadComplete(GpuUploadReceipt3D receipt)const;
    // 呼叫端必須與其他 renderer mutation 串行；成功只排入 fence signal，不等待 GPU。
    [[nodiscard]] GpuRetirementReceipt3D CaptureRetirementFence3D();
    // 初始化完成後可跨執行緒輪詢，但 renderer 必須存活且不可同時 teardown/reinitialize。
    [[nodiscard]] bool IsRetirementComplete3D(GpuRetirementReceipt3D receipt)const;
    void DestroyMaterialTextureSet(MaterialTextureSetHandle3D textures);
    void SetMaterialTextureMemoryBudget(size_t bytes);
    void SetMaterialDerivedDataCacheDirectory(std::filesystem::path directory);
    void SetMaterialDerivedDataCacheBudget(size_t bytes);
    bool CreateEnvironmentLighting(const EnvironmentLightingStore3D& store,EnvironmentLightingHandle3D handle);
    void DestroyEnvironmentLighting(EnvironmentLightingHandle3D handle);
    bool SetEnvironmentLighting(EnvironmentLightingHandle3D handle);
    EnvironmentLightingHandle3D ActiveEnvironmentLighting()const;
    bool SetLocalReflectionProbeMaterialMask3D(EnvironmentLightingHandle3D environment,LocalReflectionMaterialMask3D mask);
    void TrimEnvironmentLighting();
    ReflectionProbeCaptureHandle3D CreateReflectionProbeCapture3D(ReflectionProbeCaptureSettings3D settings);
    void DestroyReflectionProbeCapture3D(ReflectionProbeCaptureHandle3D handle);
    bool MarkReflectionProbeCaptureDirty3D(ReflectionProbeCaptureHandle3D handle);
    bool RequestReflectionProbeCaptureReadback3D(ReflectionProbeCaptureHandle3D handle);
    std::optional<ReflectionProbeCaptureReadback3D> TakeReflectionProbeCaptureReadback3D(ReflectionProbeCaptureHandle3D handle);
    std::optional<ReflectionProbeCaptureStatus3D> ReflectionProbeCaptureStatus(ReflectionProbeCaptureHandle3D handle)const;
    EnvironmentLightingHandle3D ReflectionProbeCaptureEnvironment3D(ReflectionProbeCaptureHandle3D handle)const;
    void SetReflectionProbeCaptureBudget(size_t bytes);
    void TrimReflectionProbeCaptures3D();
    bool PrepareRebaseReflectionProbeCaptures3D(Vec3 delta,uint64_t originGeneration);
    void CommitPreparedReflectionProbeCaptureRebase3D()noexcept;
    void AbortPreparedReflectionProbeCaptureRebase3D()noexcept;
    bool RebaseReflectionProbeCaptures3D(Vec3 delta,uint64_t originGeneration);
    void TrimTransientMemory();
    [[nodiscard]] TransientMemoryStats MemoryStats()const;
    void WaitIdle();
    bool SupportsDLSS() const;
    bool DLSSModuleRequested()const;
    bool DLSSOn() const;
    void ToggleDLSS();
    void SetQualitySettings(RendererQualitySettings3D settings);
    RendererQualitySettings3D QualitySettings()const;
    void SetForwardPlusPointLightsEnabled(bool enabled);
    bool ForwardPlusPointLightsEnabled()const;
    bool ApplyDevelopmentUltraResourceProfile(unsigned shadowResolution,unsigned shadowCascadeCount,float volumetricResolutionScale);
    [[nodiscard]] DevelopmentUltraRendererBaseline3D CaptureDevelopmentUltraResourceBaseline()const;
    bool RestoreDevelopmentUltraResourceBaseline(const DevelopmentUltraRendererBaseline3D& baseline);
    float VolumetricAtmosphereResolutionScale()const;
    void SetGpuInstanceCullingSettings(GpuInstanceCullingSettings3D settings);
    GpuInstanceCullingSettings3D GpuInstanceCullingSettings()const;
    bool RequestGpuInstanceCullingValidationReadback3D();
    // 此非同步 seam 僅計算 HLOD 決策；呼叫端仍須明確把結果套用到 SceneECS 與 DrawList。
    bool RequestGpuHlodSelection3D(const HlodData3D& data,const HlodSelectionView3D& view,const HlodQualitySettings3D& settings,std::span<const std::uint8_t> stableModes={});
    std::optional<GpuHlodSelectionResult3D> TakeGpuHlodSelection3D();
    void SetHzbOcclusionSettings3D(HzbOcclusionSettings3D settings);
    HzbOcclusionSettings3D HzbOcclusionSettings()const;
    RendererStats GetStats()const;
    [[nodiscard]] RendererRuntimeEpoch3D RuntimeEpoch3D()const noexcept;
    [[nodiscard]] unsigned CurrentBufferedFrameIndex3D()const noexcept;
    SkinnedSubmissionResult3D QuerySkinnedSubmission(SkinnedSubmissionReceipt3D receipt)const;
    unsigned RenderWidth() const;
    unsigned RenderHeight() const;
private:
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    friend struct D3D12RendererTestAccess3D;
#endif
#if AEVRYND_SHADOW_MAP_DIAGNOSTICS
    friend struct D3D12RendererShadowDiagnosticAccess3D;
#endif
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
    friend struct D3D12RendererAtmosphereDiagnosticAccess3D;
#endif
    struct Impl; std::unique_ptr<Impl> impl_;
};
#if AEVRYND_SHADOW_MAP_DIAGNOSTICS
struct D3D12RendererShadowDiagnosticAccess3D {
    struct Capture3D {unsigned width{},height{},rowPitch{},cascade{};std::vector<float> pixels{};[[nodiscard]] bool Complete()const noexcept{return width&&height&&rowPitch>=width*sizeof(float)&&pixels.size()==size_t(width)*height;}};
    static std::optional<Capture3D> CaptureDirectionalShadow(D3D12Renderer& renderer,unsigned cascade);
};
#endif
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
struct D3D12RendererAtmosphereDiagnosticAccess3D {
    struct Capture3D {RenderResourceIdentity3D identity{};unsigned sourceRowPitch{};std::vector<std::byte> rgba16f{};std::array<std::byte,32> pixelSha256{};[[nodiscard]] bool Complete()const noexcept{return identity.Complete()&&identity.sampleCount==1&&sourceRowPitch>=identity.width*8&&rgba16f.size()==size_t(identity.width)*identity.height*8;}};
    static std::optional<Capture3D> CaptureVolumetricAtmosphereOutput(D3D12Renderer& renderer);
    static std::optional<Capture3D> CaptureAtmosphereSceneSnapshot(D3D12Renderer& renderer);
    static bool SetPreAtmosphereHdrCapture(D3D12Renderer& renderer,bool enabled);
    static bool SetPostAtmosphereHdrCapture(D3D12Renderer& renderer,bool enabled);
    static bool SetPostTransparentHdrCapture(D3D12Renderer& renderer,bool enabled);
    static bool SetSkipWaterTransparentDraw(D3D12Renderer& renderer,bool enabled);
    static bool SetSkipAllTransparentDraw(D3D12Renderer& renderer,bool enabled);
};
#endif
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
struct D3D12RendererTestAccess3D {
    enum class BloomCpuWindowOrder3D : uint8_t { Abba, Baab };
    enum class BloomSemanticMutation3D : uint8_t { Production, ForceUnavailable, ForceNonMaskedPipeline };
    struct BloomCpuBlockReceipt3D {
        bool depthAwareRequested{},depthAwareEffective{},semanticDepthRequired{},terminalDepthWrite{},queryThreadCyclesValid{},resetSucceeded{},closeSucceeded{},bundleAvailable{},bundlePsoMatched{},previousModeValid{},previousDepthAware{},modeChanged{};
        uint16_t startProcessorGroup{},endProcessorGroup{};
        uint8_t startLogicalProcessor{},endLogicalProcessor{};
        uint32_t rawCommandRecordings{},resourceBarrierApiCalls{},resourceBarrierRecords{},depthTransitionRecords{},timedQueryCount{};
        uint64_t firstCommandRecordingCycles{},firstCommandRecordingNanoseconds{},steadyCommandRecordingCycles{},steadyCommandRecordingNanoseconds{},commandRecordingCycles{},commandRecordingNanoseconds{},planCycles{},prepareCycles{},closeCycles{},recorderFingerprint{},bundleFingerprint{};
    };
    struct BloomCpuWindowReceipt3D {
        BloomCpuWindowOrder3D order{BloomCpuWindowOrder3D::Abba};
        std::vector<BloomCpuBlockReceipt3D> blocks{};
        uint64_t submissionSerialBefore{},submissionSerialAfter{},fenceValueBefore{},fenceValueAfter{},scheduleHash{};
        uint32_t blockSize{},blockCount{},supercycleCount{},baseSupercycleCount{},complementSupercycleCount{},totalCommandRecordings{},legacyCommandRecordings{},awareCommandRecordings{},modeTransitionCount{},sameModeCount{},legacyToAwareTransitions{},awareToLegacyTransitions{};
        bool directCommandLists{},sameProductionRecorder{},noSubmission{},microBlockInterleaved{},qtcBalanced{},processorStable{};
        [[nodiscard]] bool Complete()const noexcept{return directCommandLists&&sameProductionRecorder&&noSubmission&&submissionSerialBefore==submissionSerialAfter&&fenceValueBefore==fenceValueAfter;}
    };
    static std::optional<BloomCpuWindowReceipt3D> MeasureBloomPostProcessCpuWindow(D3D12Renderer& renderer,BloomCpuWindowOrder3D order,uint32_t rawCommandRecordings);
    static bool SetBloomSemanticMutation(D3D12Renderer& renderer,BloomSemanticMutation3D mutation);
    static void SetSkinnedAllocatorState(D3D12Renderer& renderer,uint32_t nextHandle,uint64_t nextIdentity);
    static void SetEnvironmentResourceFailureCheckpoint(D3D12Renderer& renderer,int checkpoint);
    static void SetFixedProbeClearMismatch(D3D12Renderer& renderer,bool enabled);
    static void RecreateDirectionalShadowResources(D3D12Renderer& renderer);
    struct LinearHdrCapture3D {unsigned width{},height{},rowPitch{};std::vector<Vec4> pixels{};std::array<std::byte,32> pixelSha256{};float exposureEv{};[[nodiscard]] bool Complete()const noexcept{return width&&height&&rowPitch>=width*8&&pixels.size()==size_t(width)*height&&std::isfinite(exposureEv);}};
    enum class ContactShadowCaptureStage3D { Current, History, Raw, Bilateral };
    struct ContactShadowCapture3D {unsigned width{},height{};std::vector<Vec2> pixels{};[[nodiscard]] bool Complete()const noexcept{return width&&height&&pixels.size()==size_t(width)*height;}};
    struct DirectionalShadowCapture3D {unsigned width{},height{},rowPitch{},cascade{};std::vector<float> pixels{};[[nodiscard]] bool Complete()const noexcept{return width&&height&&rowPitch>=width*sizeof(float)&&pixels.size()==size_t(width)*height;}};
    struct SemanticDepthCapture3D {unsigned width{},height{},rowPitch{};std::vector<float> pixels{};[[nodiscard]] bool Complete()const noexcept{return width&&height&&rowPitch>=width*sizeof(float)&&pixels.size()==size_t(width)*height;}};
    enum class BaseSkyMutation { Production, LegacyBlue, LegacyClipMatrix, SignedNegativeW, LegacyAbsW, HemisphereFlip };
    enum class CloudContributorRouting { Production, InlineOnly, PhysicalOnly, FullDouble };
    enum class WorldPixelMutation { Production, LegacyArmAlpha, TerrainHeightDisabled };
    enum class WaterAerialOwnershipMutation { Production, LegacyDouble, SnapshotOwned, LocalOnly, Transmittance, Inscattering };
    enum class VolumetricAtmosphereMutation { Production, LegacyDepth, Nearest, LegacyPlanarNoise, NoPathCap, AdditiveShaft, WholeSegmentPerStep, WrongWindReprojection, NightIncidentZero, UpscaleFixtureProduction, UpscaleFixtureNearest };
    static void SetTimeOfDayExposureApplyFailure(D3D12Renderer& renderer,bool enabled);
    static std::optional<LinearHdrCapture3D> CaptureLinearHdr(D3D12Renderer& renderer,float exposureEv);
    static std::optional<LinearHdrCapture3D> CaptureToneMapped(D3D12Renderer& renderer,float exposureEv);
    static std::optional<LinearHdrCapture3D> CapturePostProcessDiagnostic(D3D12Renderer& renderer,float exposureEv,bool toneMapped);
    static std::optional<ContactShadowCapture3D> CaptureContactShadow(D3D12Renderer& renderer,ContactShadowCaptureStage3D stage);
    static std::optional<DirectionalShadowCapture3D> CaptureDirectionalShadow(D3D12Renderer& renderer,unsigned cascade);
    static std::optional<SemanticDepthCapture3D> CaptureSemanticDepth(D3D12Renderer& renderer);
    static bool InvalidateContactShadowTemporalHistory(D3D12Renderer& renderer);
    static bool SetContactShadowTemporalForcedOff(D3D12Renderer& renderer,bool forcedOff);
    static bool SetBaseSkyMutation(D3D12Renderer& renderer,BaseSkyMutation mutation);
    static bool SetCloudContributorRouting(D3D12Renderer& renderer,CloudContributorRouting routing);
    static bool SetCloudContributorSkyProxy(D3D12Renderer& renderer,bool enabled);
    static bool SetWorldPixelMutation(D3D12Renderer& renderer,WorldPixelMutation mutation);
    static bool SetWaterAerialOwnershipMutation(D3D12Renderer& renderer,WaterAerialOwnershipMutation mutation,uint32_t waterOrdinal=0xffffffffu);
    static bool SetLegacyTemporalDepthRejection(D3D12Renderer& renderer,bool enabled);
    static bool SetLegacyTemporalFarSkyClassification(D3D12Renderer& renderer,bool enabled);
    static void SetVolumetricAtmosphereDiagnostics(D3D12Renderer& renderer,bool enabled);
    static bool SetVolumetricAtmosphereMutation(D3D12Renderer& renderer,VolumetricAtmosphereMutation mutation);
    static bool InvalidateActiveSequenceForDeviceTeardown(D3D12Renderer& renderer);
    static bool RecreateActiveSequenceDepth(D3D12Renderer& renderer);
    static bool RecreateActiveSequenceVelocity(D3D12Renderer& renderer);
};
#endif
}
