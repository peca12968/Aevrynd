#pragma once
#include <windows.h>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>
#include "Math.h"

namespace Aevrynd {
struct Vertex { float x,y; uint32_t color; };
struct Vertex3D {float x,y,z;uint32_t color;};
struct RendererStats {
    size_t lastSubmittedVertices{};
    size_t lastSubmittedWorldVertices{};
    size_t lastUploadBytes{};
    size_t peakSubmittedVertices{};
    size_t peakSubmittedWorldVertices{};
    size_t totalDroppedVertices{};
    size_t totalDroppedWorldVertices{};
    uint32_t lastDrawCalls{};
    uint64_t submittedFrames{};
    uint64_t resourceWaits{};
    uint64_t validationWaits{};
};
class DrawList {
public:
    static constexpr size_t InitialVertexCapacity=32768;
    DrawList(){vertices.reserve(InitialVertexCapacity);worldVertices.reserve(InitialVertexCapacity);}
    void Clear(){vertices.clear();worldVertices.clear();translation_={};viewProjection_=Matrix4::Identity();}
    void SetTranslation(Vec2 translation){translation_=translation;}
    Vec2 Translation()const{return translation_;}
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void WorldTriangle(Vec3 a,Vec3 b,Vec3 c,Color color);
    void SetViewProjection(Matrix4 matrix){viewProjection_=matrix;}
    const Matrix4& ViewProjection()const{return viewProjection_;}
    void Quad(Vec2 center,Vec2 size,Color color);
    void Diamond(Vec2 center,float rx,float ry,Color color);
    void IsoPrism(Vec2 center,float rx,float ry,float height,Color top,Color left,Color right);
    void Circle(Vec2 center,float radius,Color color,int segments=18);
    void Ellipse(Vec2 center,float rx,float ry,Color color,int segments=24);
    void Line(Vec2 a,Vec2 b,float width,Color color);
    void Text(Vec2 pos,std::string_view text,float scale,Color color);
    std::vector<Vertex> vertices;
    std::vector<Vertex3D> worldVertices;
private:
    Vec2 translation_{};
    Matrix4 viewProjection_{Matrix4::Identity()};
};
class D3D12Renderer {
public:
    static constexpr unsigned BufferedFrames=2;
    static constexpr size_t MaxVertices=240000;
    static constexpr size_t MaxWorldVertices=240000;
    static constexpr size_t DrawableVertexCount(size_t requested){const size_t limited=requested<MaxVertices?requested:MaxVertices;return limited-limited%3;}
    static constexpr size_t DrawableWorldVertexCount(size_t requested){const size_t limited=requested<MaxWorldVertices?requested:MaxWorldVertices;return limited-limited%3;}
    D3D12Renderer(); ~D3D12Renderer();
    void Initialize(HWND hwnd,unsigned width,unsigned height);
    void Resize(unsigned width,unsigned height);
    void Render(const DrawList& list,Color clear);
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
