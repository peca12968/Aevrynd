#pragma once
#include <windows.h>
#include <memory>
#include <string_view>
#include <vector>
#include "Math.h"

namespace Rift {
struct Vertex { float x,y; Color color; };
class DrawList {
public:
    void Clear(){vertices.clear();}
    void Triangle(Vec2 a,Vec2 b,Vec2 c,Color color);
    void Quad(Vec2 center,Vec2 size,Color color);
    void Diamond(Vec2 center,float rx,float ry,Color color);
    void Circle(Vec2 center,float radius,Color color,int segments=18);
    void Line(Vec2 a,Vec2 b,float width,Color color);
    void Text(Vec2 pos,std::string_view text,float scale,Color color);
    std::vector<Vertex> vertices;
};
class D3D12Renderer {
public:
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
