#pragma once
#include <algorithm>
#include <windows.h>
#include <cstddef>
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
struct Surface3D {float roughness{.72f},metallic{},emissive{},occlusion{1.f};};
constexpr uint32_t PackSurface3D(Surface3D surface){return PackColorRgba8({surface.roughness,surface.metallic,surface.emissive,surface.occlusion});}
enum class MeshRenderLayer3D:uint8_t {Opaque,Overlay};
struct MeshInstance3D {MeshHandle3D mesh{};Matrix4 transform{Matrix4::Identity()};Matrix4 normalTransform{Matrix4::Identity()};uint32_t tint{0xffffffffu},surface{PackSurface3D({})};MeshRenderLayer3D layer{MeshRenderLayer3D::Opaque};};
struct DirectionalLight3D {Vec3 direction{.4f,-1.f,.3f};float ambient{.28f};Color color{1.f,.96f,.86f,1.f};float intensity{.72f};};
struct PointLight3D {Vec3 position{};float radius{8.f};Color color{1.f,.72f,.38f,1.f};float intensity{1.f};};
struct Atmosphere3D {Color fogColor{.35f,.48f,.52f,1.f};Vec3 cameraPosition{};float fogStart{45.f},fogEnd{140.f};bool enabled{};};
struct DrawListStats2D {
    size_t inputTriangles{},submittedTriangles{},culledTriangles{},peakSubmittedTriangles{};
    uint64_t totalInputTriangles{},totalCulledTriangles{};
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
    size_t peakSubmittedMeshInstances{};
    size_t totalDroppedMeshInstances{};
    size_t totalDroppedPointLights{};
    size_t lastInputTriangles2D{};
    size_t lastCulledTriangles2D{};
    uint64_t totalCulledTriangles2D{};
    size_t residentMeshes{};
    size_t residentMeshBytes{};
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
    DrawList(){vertices.reserve(InitialVertexCapacity);worldVertices.reserve(InitialVertexCapacity);worldIndices.reserve(InitialVertexCapacity*3/2);meshInstances.reserve(1024);}
    void Clear(){vertices.clear();worldVertices.clear();worldIndices.clear();meshInstances.clear();pointLights_.clear();translation_={};clipEnabled_=false;viewProjection_=Matrix4::Identity();atmosphere_={};stats_.inputTriangles=stats_.submittedTriangles=stats_.culledTriangles=0;}
    void SetTranslation(Vec2 translation){translation_=translation;}
    Vec2 Translation()const{return translation_;}
    void SetClipRect(Vec2 minimum,Vec2 maximum){clipMinimum_={(std::min)(minimum.x,maximum.x),(std::min)(minimum.y,maximum.y)};clipMaximum_={(std::max)(minimum.x,maximum.x),(std::max)(minimum.y,maximum.y)};clipEnabled_=true;}
    void DisableClip(){clipEnabled_=false;}
    bool ClipEnabled()const{return clipEnabled_;}
    DrawListStats2D Stats2D()const{return stats_;}
    void ResetStats2D(){stats_={};}
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void WorldTriangle(Vec3 a,Vec3 b,Vec3 c,Color color);
    void MeshInstance(MeshHandle3D mesh,Matrix4 transform,Color tint={1,1,1,1},Surface3D surface={},MeshRenderLayer3D layer=MeshRenderLayer3D::Opaque){if(mesh)meshInstances.push_back({mesh,transform,CreateNormalMatrix(transform),PackColorRgba8(tint),PackSurface3D(surface),layer});}
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
    std::vector<Vertex> vertices;
    std::vector<Vertex3D> worldVertices;
    std::vector<uint32_t> worldIndices;
    std::vector<MeshInstance3D> meshInstances;
private:
    Vec2 translation_{};
    Matrix4 viewProjection_{Matrix4::Identity()};
    DirectionalLight3D light_{};
    Atmosphere3D atmosphere_{};
    std::vector<PointLight3D> pointLights_;
    Vec2 clipMinimum_{},clipMaximum_{};
    bool clipEnabled_{};
    DrawListStats2D stats_{};
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
    static constexpr size_t MeshInstanceUploadBytes=sizeof(Matrix4)+sizeof(float)*12+sizeof(uint32_t)*2;
    static constexpr size_t DrawableVertexCount(size_t requested){const size_t limited=requested<MaxVertices?requested:MaxVertices;return limited-limited%3;}
    static constexpr size_t DrawableWorldVertexCount(size_t requested){return requested<MaxWorldVertices?requested:MaxWorldVertices;}
    static size_t DrawableWorldIndexCount(const std::vector<uint32_t>& indices,size_t vertexCount);
    D3D12Renderer(); ~D3D12Renderer();
    void Initialize(HWND hwnd,unsigned width,unsigned height,unsigned logicalWidth=1280,unsigned logicalHeight=720);
    void Resize(unsigned width,unsigned height);
    void Render(const DrawList& list,Color clear);
    MeshHandle3D CreateMesh(std::span<const Vertex3D> vertices,std::span<const uint32_t> indices);
    void DestroyMesh(MeshHandle3D mesh);
    void WaitIdle();
    bool SupportsDLSS() const;
    bool DLSSOn() const;
    void ToggleDLSS();
    RendererStats GetStats()const;
    unsigned RenderWidth() const;
    unsigned RenderHeight() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
