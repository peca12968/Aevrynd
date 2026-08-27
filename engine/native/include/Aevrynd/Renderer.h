#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <windows.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <limits>
#include <span>
#include <string_view>
#include <vector>
#include "Math.h"
#include "Viewport.h"

namespace Aevrynd {
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
struct MeshHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const MeshHandle3D&)const=default;};
struct MaterialTextureSetHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const MaterialTextureSetHandle3D&)const=default;};
struct MaterialTextureLayerDefinition3D {std::filesystem::path baseColorPath,roughnessPath,normalPath;};
struct MaterialTextureSetDefinition3D {std::filesystem::path baseColorPath,roughnessPath;unsigned maximumResolution{2048};std::filesystem::path normalPath;std::array<MaterialTextureLayerDefinition3D,2> additionalLayers{};};
struct Surface3D {float roughness{.72f},metallic{},emissive{},occlusion{1.f};};
constexpr uint32_t PackSurface3D(Surface3D surface){return PackColorRgba8({surface.roughness,surface.metallic,surface.emissive,surface.occlusion});}
enum class MaterialClass3D:uint8_t {Surface,Water,Foliage,Terrain};
static constexpr uint32_t MeshInstanceHasTexturesBit3D=1u,MeshInstanceUsesUvBit3D=2u,MeshInstanceMaterialClassShift3D=2u,MeshInstanceMaterialClassMask3D=3u<<MeshInstanceMaterialClassShift3D;
constexpr MaterialClass3D NormalizeMaterialClass3D(MaterialClass3D value){return value==MaterialClass3D::Water||value==MaterialClass3D::Foliage||value==MaterialClass3D::Terrain?value:MaterialClass3D::Surface;}
constexpr uint32_t PackMeshInstanceFlags3D(bool hasTextures,bool usesUv,MaterialClass3D materialClass){return(hasTextures?MeshInstanceHasTexturesBit3D:0u)|(hasTextures&&usesUv?MeshInstanceUsesUvBit3D:0u)|(uint32_t(NormalizeMaterialClass3D(materialClass))<<MeshInstanceMaterialClassShift3D);}
constexpr MaterialClass3D UnpackMeshInstanceMaterialClass3D(uint32_t value){return NormalizeMaterialClass3D(MaterialClass3D((value&MeshInstanceMaterialClassMask3D)>>MeshInstanceMaterialClassShift3D));}
enum class MeshRenderLayer3D:uint8_t {Opaque,Transparent,Overlay};
struct MeshInstance3D {MeshHandle3D mesh{};Matrix4 transform{Matrix4::Identity()};Matrix4 normalTransform{Matrix4::Identity()};uint32_t tint{0xffffffffu},surface{PackSurface3D({})};MeshRenderLayer3D layer{MeshRenderLayer3D::Opaque};MaterialTextureSetHandle3D textures{};float textureScale{.25f};bool useUv{};MaterialClass3D materialClass{MaterialClass3D::Surface};float cameraDepth{};bool castsShadow{true};};
constexpr bool TransparentBackToFront3D(const MeshInstance3D& left,const MeshInstance3D& right){return left.cameraDepth>right.cameraDepth;}
struct DirectionalLight3D {Vec3 direction{.4f,-1.f,.3f};float ambient{.28f};Color color{1.f,.96f,.86f,1.f};float intensity{.72f};};
constexpr uint32_t SelectDirectionalShadowCascade3D(float cameraDistance,float splitDistance=32.f){return std::isfinite(cameraDistance)&&cameraDistance>=splitDistance?1u:0u;}
inline float SnapDirectionalShadowCoordinate3D(float value,float texelWorldSize){return std::isfinite(value)&&std::isfinite(texelWorldSize)&&texelWorldSize>0.f?std::round(value/texelWorldSize)*texelWorldSize:value;}
inline float DirectionalShadowCascadeBlend3D(float cameraDistance,float splitDistance=32.f,float transitionWidth=6.f){if(!std::isfinite(cameraDistance)||!std::isfinite(splitDistance)||!std::isfinite(transitionWidth)||transitionWidth<=0.f)return cameraDistance>=splitDistance?1.f:0.f;const float value=Clamp((cameraDistance-(splitDistance-transitionWidth*.5f))/transitionWidth,0.f,1.f);return value*value*(3.f-2.f*value);}
inline float RayleighPhase3D(float cosine){cosine=Clamp(cosine,-1.f,1.f);return 3.f*(1.f+cosine*cosine)/(16.f*3.14159265359f);}
inline float MiePhase3D(float cosine,float anisotropy=.76f){cosine=Clamp(cosine,-1.f,1.f);anisotropy=Clamp(anisotropy,-.95f,.95f);const float squared=anisotropy*anisotropy,denominator=(std::max)(.0001f,1.f+squared-2.f*anisotropy*cosine);return 3.f*(1.f-squared)*(1.f+cosine*cosine)/(8.f*3.14159265359f*(2.f+squared)*std::pow(denominator,1.5f));}
inline float WaterFresnel3D(float viewNormalCosine,float reflectance=.02f){const float inverse=1.f-Clamp(viewNormalCosine,0.f,1.f),inverse2=inverse*inverse;return Clamp(reflectance+(1.f-reflectance)*inverse2*inverse2*inverse,0.f,1.f);}
inline float WaterTransmission3D(float absorption,float pathLength){if(!std::isfinite(absorption)||!std::isfinite(pathLength))return 0.f;return std::exp(-(std::max)(0.f,absorption)*(std::max)(0.f,pathLength));}
inline float FoliageWrapDiffuse3D(float normalLightCosine,float wrap=.35f){if(!std::isfinite(normalLightCosine)||!std::isfinite(wrap))return 0.f;wrap=Clamp(wrap,0.f,1.f);return Clamp((normalLightCosine+wrap)/(1.f+wrap),0.f,1.f);}
inline float FoliageTransmission3D(float normalLightCosine,float viewLightCosine){if(!std::isfinite(normalLightCosine)||!std::isfinite(viewLightCosine))return 0.f;const float backLight=Clamp(-normalLightCosine,0.f,1.f),forwardScatter=Clamp(viewLightCosine,0.f,1.f);return Clamp(backLight*backLight*(.45f+.55f*forwardScatter),0.f,1.f);}
inline float ProceduralCloudDensity3D(float noiseSample,float coverage=.54f,float softness=.18f){if(!std::isfinite(noiseSample)||!std::isfinite(coverage)||!std::isfinite(softness)||softness<=0.f)return 0.f;const float value=Clamp((noiseSample-coverage)/softness,0.f,1.f);return value*value*(3.f-2.f*value);}
inline float CloudHorizonFade3D(float rayHeight){if(!std::isfinite(rayHeight))return 0.f;const float value=Clamp((rayHeight-.025f)/.18f,0.f,1.f);return value*value*(3.f-2.f*value);}
inline float DirectionalShadowReceiverBias3D(float normalLightCosine,float slopeScale=.0015f,float minimumBias=.00035f,float maximumBias=.006f){if(!std::isfinite(normalLightCosine)||!std::isfinite(slopeScale)||!std::isfinite(minimumBias)||!std::isfinite(maximumBias))return(std::max)(0.f,minimumBias);const float cosine=Clamp(normalLightCosine,0.f,1.f),slope=std::sqrt((std::max)(0.f,1.f-cosine*cosine))/(std::max)(.2f,cosine);return Clamp((std::max)(0.f,minimumBias)+(std::max)(0.f,slopeScale)*slope,(std::max)(0.f,minimumBias),(std::max)(minimumBias,maximumBias));}
inline Vec3 TerrainLayerWeights3D(float slope,float normalizedHeight,float macroNoise){if(!std::isfinite(slope)||!std::isfinite(normalizedHeight)||!std::isfinite(macroNoise))return{1,0,0};slope=Clamp(slope,0.f,1.f);normalizedHeight=Clamp(normalizedHeight,0.f,1.f);macroNoise=Clamp(macroNoise,0.f,1.f);const auto smooth=[](float edge0,float edge1,float value){value=Clamp((value-edge0)/(edge1-edge0),0.f,1.f);return value*value*(3.f-2.f*value);};const float rock=smooth(.38f,.78f,slope+(std::max)(0.f,normalizedHeight-.62f)*.35f),soil=(1.f-rock)*smooth(.18f,.62f,slope+(1.f-macroNoise)*.18f);return{(std::max)(0.f,1.f-rock-soil),soil,rock};}
struct PointLight3D {Vec3 position{};float radius{8.f};Color color{1.f,.72f,.38f,1.f};float intensity{1.f};};
inline float PointLightInfluence(const PointLight3D& light,Vec3 reference){const Vec3 delta=light.position-reference;return light.intensity*light.radius*light.radius/(1.f+Dot(delta,delta));}
template<std::size_t Capacity>struct PointLightSelection3D {std::array<PointLight3D,Capacity> lights{};std::array<float,Capacity> influences{};std::size_t count{},candidates{};};
template<std::size_t Capacity>PointLightSelection3D<Capacity> SelectInfluentialPointLights(std::span<const PointLight3D> lights,Vec3 reference){
    static_assert(Capacity>0);PointLightSelection3D<Capacity> result{};result.candidates=lights.size();
    for(const PointLight3D& light:lights){const float influence=PointLightInfluence(light,reference);if(!std::isfinite(influence)||influence<=0.f)continue;std::size_t destination=result.count;while(destination>0&&influence>result.influences[destination-1])--destination;if(destination>=Capacity)continue;const std::size_t newCount=(std::min)(Capacity,result.count+1);for(std::size_t index=newCount-1;index>destination;--index){result.lights[index]=result.lights[index-1];result.influences[index]=result.influences[index-1];}result.lights[destination]=light;result.influences[destination]=influence;result.count=newCount;}
    return result;
}
struct Atmosphere3D {Color fogColor{.35f,.48f,.52f,1.f};Vec3 cameraPosition{};float fogStart{45.f},fogEnd{140.f};bool enabled{};};
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
struct RendererStats {
    unsigned outputWidth{},outputHeight{};
    CanvasViewport canvasViewport{};
    uint64_t resizeCount{};
    bool linearWorldBlending{};
    size_t worldSrgbRenderTargetViews{};
    bool linearHdrSceneComposition{};
    size_t hdrSceneColorBytes{};
    uint64_t hdrSceneColorAllocations{};
    uint64_t hdrSceneColorReleases{};
    size_t nativeDepthBufferBytes{};
    uint64_t nativeDepthBufferAllocations{};
    uint64_t nativeDepthBufferReleases{};
    uint64_t hdrPostProcessPasses{};
    size_t sceneConstantBufferBytes{};
    size_t rootSignatureDwords{};
    float lastExposureCompensationEv{};
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
    size_t lastSubmittedTexturedMeshInstances{};
    size_t lastSubmittedFoliageInstances{};
    size_t lastSubmittedTerrainInstances{};
    size_t lastDirectionalShadowCasters{};
    size_t lastDirectionalShadowDrawCalls{};
    size_t directionalShadowMapBytes{};
    uint64_t directionalShadowMapAllocations{};
    uint64_t directionalShadowMapReleases{};
    size_t lastDirectionalShadowCascades{};
    float directionalShadowCascadeSplit{32.f};
    float directionalShadowTransitionWidth{6.f};
    size_t directionalShadowBasePcfSamples{12};
    size_t directionalShadowTransitionPcfSamples{24};
    uint64_t successfulFrameCaptures{};
    uint64_t rejectedFrameCaptures{};
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
    uint64_t lastGpuFrameNanoseconds{};
    uint64_t totalGpuFrameNanoseconds{};
    uint64_t peakGpuFrameNanoseconds{};
};
class DrawList {
public:
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
    void Clear(){vertices.clear();worldVertices.clear();worldIndices.clear();meshInstances.clear();pointLights_.clear();additiveSpans_.clear();blendMode_=BlendMode2D::Alpha;transform_={};transformCosine_=1.f;transformSine_=0.f;transformIdentity_=true;clipEnabled_=false;viewProjection_=Matrix4::Identity();atmosphere_={};stats_.inputTriangles=stats_.submittedTriangles=stats_.culledTriangles=stats_.textGlyphs=stats_.textSourcePixels=stats_.textSpans=stats_.textContainmentTests=stats_.textContainedSpans=stats_.textContainmentBypassedTriangles=stats_.textCulledSpans=stats_.textCullingBypassedTriangles=stats_.primitiveCullTests=stats_.primitiveCulled=stats_.primitiveCulledTriangles=stats_.primitiveContained=stats_.primitiveContainedTriangles=stats_.ellipseSegments=stats_.ellipseOutlineSegments=stats_.ellipseTrigonometricEvaluations=stats_.ellipseTrigonometricEvaluationsSaved=stats_.ellipseStepCacheHits=stats_.ellipseStepCacheMisses=stats_.sharedVertexTransforms=stats_.sharedVertexTransformsSaved=stats_.primitiveColorPacks=stats_.primitiveColorPacksSaved=stats_.geometryBatches=stats_.geometryCulledBatches=stats_.geometryVertices=stats_.geometryTransformsSaved=stats_.additiveTriangles=stats_.additiveSpans=stats_.radialLightBatches=stats_.radialLightCandidates=stats_.radialLightsSubmitted=stats_.radialLightsCulled=stats_.radialLightsBudgetDropped=0;}
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
    [[nodiscard]] TransientMemoryStats MemoryStats()const;
    [[nodiscard]] static SystemGlyphCacheStats2D SystemGlyphCacheStats();
    static size_t TrimSystemGlyphCache(size_t retainedGlyphs=RetainedSystemGlyphsAfterTrim);
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void WorldTriangle(Vec3 a,Vec3 b,Vec3 c,Color color);
    void MeshInstance(MeshHandle3D mesh,Matrix4 transform,Color tint={1,1,1,1},Surface3D surface={},MeshRenderLayer3D layer=MeshRenderLayer3D::Opaque,MaterialTextureSetHandle3D textures={},float textureScale=.25f,bool useUv=false,MaterialClass3D materialClass=MaterialClass3D::Surface,float cameraDepth=std::numeric_limits<float>::quiet_NaN(),bool castsShadow=true){if(mesh){materialClass=NormalizeMaterialClass3D(materialClass);if(materialClass==MaterialClass3D::Water)layer=MeshRenderLayer3D::Transparent;if(!std::isfinite(cameraDepth))cameraDepth=TransformPoint(viewProjection_,TransformPoint(transform,{})).z;meshInstances.push_back({mesh,transform,CreateNormalMatrix(transform),PackColorRgba8(tint),PackSurface3D(surface),layer,textures,(std::max)(.001f,textureScale),useUv,materialClass,cameraDepth,castsShadow});}}
    void SetViewProjection(Matrix4 matrix){viewProjection_=matrix;}
    const Matrix4& ViewProjection()const{return viewProjection_;}
    void SetDirectionalLight(DirectionalLight3D light){light.direction=Normalize(light.direction);if(Length(light.direction)<.0001f)light.direction={0,-1,0};light_=light;}
    const DirectionalLight3D& DirectionalLight()const{return light_;}
    void SetDirectionalShadowMapEnabled(bool enabled){directionalShadowMapEnabled_=enabled;}
    bool DirectionalShadowMapEnabled()const{return directionalShadowMapEnabled_;}
    void AddPointLight(PointLight3D light){light.radius=(std::max)(.001f,light.radius);light.intensity=(std::max)(0.f,light.intensity);if(light.intensity>0.f)pointLights_.push_back(light);}
    const std::vector<PointLight3D>& PointLights()const{return pointLights_;}
    void SetAtmosphere(Atmosphere3D atmosphere){atmosphere.fogEnd=(std::max)(atmosphere.fogStart+.001f,atmosphere.fogEnd);atmosphere_=atmosphere;}
    const Atmosphere3D& Atmosphere()const{return atmosphere_;}
    void SetEnvironmentTime(float seconds){environmentTime_=std::isfinite(seconds)?seconds:0.f;}
    float EnvironmentTime()const{return environmentTime_;}
    void SetExposureCompensation(float ev){exposureCompensationEv_=(std::clamp)(std::isfinite(ev)?ev:0.f,-10.f,10.f);}
    float ExposureCompensation()const{return exposureCompensationEv_;}
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
    std::vector<Vertex> vertices;
    std::vector<Vertex3D> worldVertices;
    std::vector<uint32_t> worldIndices;
    std::vector<MeshInstance3D> meshInstances;
private:
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
    float exposureCompensationEv_{};
    std::vector<PointLight3D> pointLights_;
    std::vector<DrawSpan2D> additiveSpans_;
    Vec2 clipMinimum_{},clipMaximum_{};
    bool clipEnabled_{};
    DrawListStats2D stats_{};
    size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
    std::array<EllipseStepCacheEntry,4> ellipseStepCache_{};
    uint64_t ellipseStepCacheClock_{};
    bool backfaceCulling_{true};
    bool directionalShadowMapEnabled_{true};
    BlendMode2D blendMode_{BlendMode2D::Alpha};
    DrawListCapacityProfile capacityProfile_{DrawListCapacityProfile::Frame};
};
class D3D12Renderer {
public:
    static constexpr unsigned BufferedFrames=3;
    static constexpr size_t MaxVertices=240000;
    static constexpr size_t MaxWorldVertices=240000;
    static constexpr size_t MaxWorldIndices=480000;
    static constexpr size_t MaxMeshInstances=65536;
    static constexpr uint64_t InstanceGroupRetentionFrames=300;
    static constexpr size_t MaxPointLights=3;
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
    static constexpr size_t MeshInstanceUploadBytes=sizeof(Matrix4)+sizeof(float)*12+sizeof(uint32_t)*3;
    static constexpr size_t DrawableVertexCount(size_t requested){const size_t limited=requested<MaxVertices?requested:MaxVertices;return limited-limited%3;}
    static constexpr size_t DrawableWorldVertexCount(size_t requested){return requested<MaxWorldVertices?requested:MaxWorldVertices;}
    static constexpr size_t NextFrameUploadCapacity(size_t current,size_t required,size_t maximum){size_t capacity=(std::max)(size_t{1},current);while(capacity<required&&capacity<maximum)capacity=capacity>maximum/2?maximum:capacity*2;return(std::min)(capacity,maximum);}
    static constexpr size_t Initial2DFrameUploadBytes=BufferedFrames*InitialFrameVertices*sizeof(Vertex);
    static constexpr size_t Initial3DFrameUploadBytes=BufferedFrames*(InitialFrameWorldVertices*sizeof(Vertex3D)+InitialFrameWorldIndices*sizeof(uint32_t)+InitialFrameMeshInstances*MeshInstanceUploadBytes);
    static constexpr size_t InitialFrameUploadBytes=Initial2DFrameUploadBytes+Initial3DFrameUploadBytes;
    static size_t DrawableWorldIndexCount(const std::vector<uint32_t>& indices,size_t vertexCount);
    D3D12Renderer(); ~D3D12Renderer();
    void Initialize(HWND hwnd,unsigned width,unsigned height,unsigned logicalWidth=1280,unsigned logicalHeight=720,bool enableDLSS=false);
    void Resize(unsigned width,unsigned height);
    void Render(const DrawList& list,Color clear);
    bool CaptureNextFrame(const std::filesystem::path& path);
    MeshHandle3D CreateMesh(std::span<const Vertex3D> vertices,std::span<const uint32_t> indices);
    void DestroyMesh(MeshHandle3D mesh);
    void SetMeshMemoryBudget(size_t bytes);
    MaterialTextureSetHandle3D CreateMaterialTextureSet(const MaterialTextureSetDefinition3D& definition);
    void DestroyMaterialTextureSet(MaterialTextureSetHandle3D textures);
    void SetMaterialTextureMemoryBudget(size_t bytes);
    void SetMaterialDerivedDataCacheDirectory(std::filesystem::path directory);
    void SetMaterialDerivedDataCacheBudget(size_t bytes);
    void TrimTransientMemory();
    [[nodiscard]] TransientMemoryStats MemoryStats()const;
    void WaitIdle();
    bool SupportsDLSS() const;
    bool DLSSModuleRequested()const;
    bool DLSSOn() const;
    void ToggleDLSS();
    RendererStats GetStats()const;
    unsigned RenderWidth() const;
    unsigned RenderHeight() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
