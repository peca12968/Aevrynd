#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <windows.h>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#include "Math.h"
#include "Viewport.h"

namespace Aevrynd {
struct Vertex { float x,y; uint32_t color; };
struct Vertex3D {float x,y,z;uint32_t normal;uint32_t color;};
struct MeshHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const MeshHandle3D&)const=default;};
struct MaterialTextureSetHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const MaterialTextureSetHandle3D&)const=default;};
struct MaterialTextureSetDefinition3D {std::filesystem::path baseColorPath,roughnessPath;unsigned maximumResolution{2048};std::filesystem::path normalPath;};
struct Surface3D {float roughness{.72f},metallic{},emissive{},occlusion{1.f};};
constexpr uint32_t PackSurface3D(Surface3D surface){return PackColorRgba8({surface.roughness,surface.metallic,surface.emissive,surface.occlusion});}
enum class MeshRenderLayer3D:uint8_t {Opaque,Overlay};
struct MeshInstance3D {MeshHandle3D mesh{};Matrix4 transform{Matrix4::Identity()};Matrix4 normalTransform{Matrix4::Identity()};uint32_t tint{0xffffffffu},surface{PackSurface3D({})};MeshRenderLayer3D layer{MeshRenderLayer3D::Opaque};MaterialTextureSetHandle3D textures{};float textureScale{.25f};};
struct DirectionalLight3D {Vec3 direction{.4f,-1.f,.3f};float ambient{.28f};Color color{1.f,.96f,.86f,1.f};float intensity{.72f};};
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
    uint64_t totalInputTriangles{},totalCulledTriangles{};
    uint64_t totalTextSourcePixels{},totalTextSpans{};
};
struct TransientMemoryStats {
    size_t trackedCapacityBytes{};
    size_t lastReleasedBytes{};
    uint64_t trimCount{};
};
struct RendererStats {
    unsigned outputWidth{},outputHeight{};
    CanvasViewport canvasViewport{};
    uint64_t resizeCount{};
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
    size_t lastSubmittedPointLights{};
    size_t lastPointLightCandidates{};
    size_t peakSubmittedMeshInstances{};
    size_t totalDroppedMeshInstances{};
    size_t totalDroppedPointLights{};
    size_t lastInputTriangles2D{};
    size_t lastCulledTriangles2D{};
    uint64_t totalCulledTriangles2D{};
    size_t residentMeshes{};
    size_t residentMeshReferences{};
    size_t residentMeshBytes{};
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
    uint64_t meshUploadPressureWaits{};
    uint64_t meshUploadPressureWaitNanoseconds{};
    size_t residentMaterialTextureSets{};
    size_t residentMaterialTextureReferences{};
    size_t residentTextureBytes{};
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
    DrawList(){vertices.reserve(InitialVertexCapacity);worldVertices.reserve(InitialVertexCapacity);worldIndices.reserve(InitialWorldIndexCapacity);meshInstances.reserve(InitialMeshInstanceCapacity);pointLights_.reserve(InitialPointLightCapacity);}
    void Clear(){vertices.clear();worldVertices.clear();worldIndices.clear();meshInstances.clear();pointLights_.clear();transform_={};transformCosine_=1.f;transformSine_=0.f;clipEnabled_=false;viewProjection_=Matrix4::Identity();atmosphere_={};stats_.inputTriangles=stats_.submittedTriangles=stats_.culledTriangles=stats_.textGlyphs=stats_.textSourcePixels=stats_.textSpans=0;}
    void SetTranslation(Vec2 translation){transform_.translation=translation;}
    Vec2 Translation()const{return transform_.translation;}
    void SetTransform2D(Transform2D transform){transform_=transform;transformCosine_=std::cos(transform.rotation);transformSine_=std::sin(transform.rotation);}
    Transform2D CurrentTransform2D()const{return transform_;}
    void ResetTransform2D(){SetTransform2D({});}
    void SetClipRect(Vec2 minimum,Vec2 maximum){clipMinimum_={(std::min)(minimum.x,maximum.x),(std::min)(minimum.y,maximum.y)};clipMaximum_={(std::max)(minimum.x,maximum.x),(std::max)(minimum.y,maximum.y)};clipEnabled_=true;}
    void DisableClip(){clipEnabled_=false;}
    bool ClipEnabled()const{return clipEnabled_;}
    DrawListStats2D Stats2D()const{return stats_;}
    void ResetStats2D(){stats_={};}
    void TrimTransientMemory();
    [[nodiscard]] TransientMemoryStats MemoryStats()const;
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void WorldTriangle(Vec3 a,Vec3 b,Vec3 c,Color color);
    void MeshInstance(MeshHandle3D mesh,Matrix4 transform,Color tint={1,1,1,1},Surface3D surface={},MeshRenderLayer3D layer=MeshRenderLayer3D::Opaque,MaterialTextureSetHandle3D textures={},float textureScale=.25f){if(mesh)meshInstances.push_back({mesh,transform,CreateNormalMatrix(transform),PackColorRgba8(tint),PackSurface3D(surface),layer,textures,(std::max)(.001f,textureScale)});}
    void SetViewProjection(Matrix4 matrix){viewProjection_=matrix;}
    const Matrix4& ViewProjection()const{return viewProjection_;}
    void SetDirectionalLight(DirectionalLight3D light){light.direction=Normalize(light.direction);if(Length(light.direction)<.0001f)light.direction={0,-1,0};light_=light;}
    const DirectionalLight3D& DirectionalLight()const{return light_;}
    void AddPointLight(PointLight3D light){light.radius=(std::max)(.001f,light.radius);light.intensity=(std::max)(0.f,light.intensity);if(light.intensity>0.f)pointLights_.push_back(light);}
    const std::vector<PointLight3D>& PointLights()const{return pointLights_;}
    void SetAtmosphere(Atmosphere3D atmosphere){atmosphere.fogEnd=(std::max)(atmosphere.fogStart+.001f,atmosphere.fogEnd);atmosphere_=atmosphere;}
    const Atmosphere3D& Atmosphere()const{return atmosphere_;}
    void SetBackfaceCulling(bool enabled){backfaceCulling_=enabled;}
    bool BackfaceCulling()const{return backfaceCulling_;}
    void Quad(Vec2 center,Vec2 size,Color color);
    void Diamond(Vec2 center,float rx,float ry,Color color);
    void IsoPrism(Vec2 center,float rx,float ry,float height,Color top,Color left,Color right);
    void Circle(Vec2 center,float radius,Color color,int segments=18);
    void Ellipse(Vec2 center,float rx,float ry,Color color,int segments=24);
    void ProjectedEllipseShadow(Vec2 base,float rx,float ry,Vec2 lightDirection,float height,float opacity=.4f,int segments=18);
    void Line(Vec2 a,Vec2 b,float width,Color color);
    void Text(Vec2 pos,std::string_view text,float scale,Color color);
    Vec2 MeasureText(std::string_view text,float scale)const;
    std::vector<Vertex> vertices;
    std::vector<Vertex3D> worldVertices;
    std::vector<uint32_t> worldIndices;
    std::vector<MeshInstance3D> meshInstances;
private:
    Vec2 ApplyTransform2D(Vec2 point)const{const Vec2 local{(point.x-transform_.pivot.x)*transform_.scale.x,(point.y-transform_.pivot.y)*transform_.scale.y};return{local.x*transformCosine_-local.y*transformSine_+transform_.pivot.x+transform_.translation.x,local.x*transformSine_+local.y*transformCosine_+transform_.pivot.y+transform_.translation.y};}
    Transform2D transform_{};
    float transformCosine_{1.f},transformSine_{};
    Matrix4 viewProjection_{Matrix4::Identity()};
    DirectionalLight3D light_{};
    Atmosphere3D atmosphere_{};
    std::vector<PointLight3D> pointLights_;
    Vec2 clipMinimum_{},clipMaximum_{};
    bool clipEnabled_{};
    DrawListStats2D stats_{};
    size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
    bool backfaceCulling_{true};
};
class D3D12Renderer {
public:
    static constexpr unsigned BufferedFrames=3;
    static constexpr size_t MaxVertices=240000;
    static constexpr size_t MaxWorldVertices=240000;
    static constexpr size_t MaxWorldIndices=480000;
    static constexpr size_t MaxMeshInstances=65536;
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
    static constexpr size_t InitialFrameUploadBytes=BufferedFrames*(InitialFrameVertices*sizeof(Vertex)+InitialFrameWorldVertices*sizeof(Vertex3D)+InitialFrameWorldIndices*sizeof(uint32_t)+InitialFrameMeshInstances*MeshInstanceUploadBytes);
    static size_t DrawableWorldIndexCount(const std::vector<uint32_t>& indices,size_t vertexCount);
    D3D12Renderer(); ~D3D12Renderer();
    void Initialize(HWND hwnd,unsigned width,unsigned height,unsigned logicalWidth=1280,unsigned logicalHeight=720,bool enableDLSS=false);
    void Resize(unsigned width,unsigned height);
    void Render(const DrawList& list,Color clear);
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
