#pragma once
#include <windows.h>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>
#include "Math.h"

namespace Aevrynd {
struct Vertex { float x,y; uint32_t color; };
class DrawList {
public:
    static constexpr size_t InitialVertexCapacity=32768;
    DrawList(){vertices.reserve(InitialVertexCapacity);}
    void Clear(){vertices.clear();translation_={};}
    void SetTranslation(Vec2 translation){translation_=translation;}
    Vec2 Translation()const{return translation_;}
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void Quad(Vec2 center,Vec2 size,Color color);
    void Diamond(Vec2 center,float rx,float ry,Color color);
    void IsoPrism(Vec2 center,float rx,float ry,float height,Color top,Color left,Color right);
    void Circle(Vec2 center,float radius,Color color,int segments=18);
    void Ellipse(Vec2 center,float rx,float ry,Color color,int segments=24);
    void Line(Vec2 a,Vec2 b,float width,Color color);
    void Text(Vec2 pos,std::string_view text,float scale,Color color);
    std::vector<Vertex> vertices;
private:
    Vec2 translation_{};
};
class D3D12Renderer {
public:
    static constexpr unsigned BufferedFrames=2;
    static constexpr size_t MaxVertices=240000;
    static constexpr size_t DrawableVertexCount(size_t requested){const size_t limited=requested<MaxVertices?requested:MaxVertices;return limited-limited%3;}
    D3D12Renderer(); ~D3D12Renderer();
    void Initialize(HWND hwnd,unsigned width,unsigned height);
    void Resize(unsigned width,unsigned height);
    void Render(const DrawList& list,Color clear);
    void WaitIdle();
    bool SupportsDLSS() const;
    bool DLSSOn() const;
    void ToggleDLSS();
    unsigned RenderWidth() const;
    unsigned RenderHeight() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
