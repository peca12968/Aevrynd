#pragma once
#include "Camera.h"
#include "Renderer.h"
#include <algorithm>
#include <array>
#include <vector>

namespace Aevrynd {
struct Triangle3D {Vec3 a{},b{},c{};Color color{};};
struct Line3D {Vec3 a{},b{};float width{1.f};Color color{};};
struct Scene3DStats {std::size_t inputTriangles{},visibleTriangles{},culledTriangles{},inputLines{},visibleLines{};};

class Scene3D {
public:
    void Clear(){triangles_.clear();lines_.clear();}
    void Triangle(Vec3 a,Vec3 b,Vec3 c,Color color){triangles_.push_back({a,b,c,color});}
    void Line(Vec3 a,Vec3 b,float width,Color color){lines_.push_back({a,b,width,color});}
    void Box(Vec3 center,Vec3 size,Color color){const Vec3 half=size*.5f;const std::array<Vec3,8> points={center+Vec3{-half.x,-half.y,-half.z},center+Vec3{half.x,-half.y,-half.z},center+Vec3{half.x,half.y,-half.z},center+Vec3{-half.x,half.y,-half.z},center+Vec3{-half.x,-half.y,half.z},center+Vec3{half.x,-half.y,half.z},center+Vec3{half.x,half.y,half.z},center+Vec3{-half.x,half.y,half.z}};auto shade=[&](float scale){return Color{color.r*scale,color.g*scale,color.b*scale,color.a};};auto face=[&](int a,int b,int c,int d,Color faceColor){Triangle(points[a],points[b],points[c],faceColor);Triangle(points[a],points[c],points[d],faceColor);};face(0,1,2,3,shade(.55f));face(5,4,7,6,shade(.85f));face(4,0,3,7,shade(.68f));face(1,5,6,2,shade(.76f));face(3,2,6,7,shade(1.f));face(4,5,1,0,shade(.45f));}

    // 第一階段以穩定畫家排序驗證通用場景與相機；GPU 深度緩衝會在後續硬體 3D 管線取代此排序。
    Scene3DStats Render(DrawList& draw,const Camera3D& camera,Vec2 viewport)const{struct VisibleTriangle{ProjectedPoint a,b,c;float depth{};Color color{};std::size_t order{};};std::vector<VisibleTriangle> visible;visible.reserve(triangles_.size());Scene3DStats stats{};stats.inputTriangles=triangles_.size();stats.inputLines=lines_.size();for(std::size_t index=0;index<triangles_.size();++index){const auto& triangle=triangles_[index];ProjectedPoint a=ProjectToViewport(triangle.a,camera,viewport),b=ProjectToViewport(triangle.b,camera,viewport),c=ProjectToViewport(triangle.c,camera,viewport);if(!a.visible||!b.visible||!c.visible)continue;visible.push_back({a,b,c,(a.depth+b.depth+c.depth)/3.f,triangle.color,index});}std::stable_sort(visible.begin(),visible.end(),[](const auto& left,const auto& right){return left.depth>right.depth;});for(const auto& triangle:visible)draw.Triangle(triangle.a.position,triangle.b.position,triangle.c.position,triangle.color);for(const auto& line:lines_){ProjectedPoint a=ProjectToViewport(line.a,camera,viewport),b=ProjectToViewport(line.b,camera,viewport);if(!a.visible||!b.visible)continue;draw.Line(a.position,b.position,line.width,line.color);++stats.visibleLines;}stats.visibleTriangles=visible.size();stats.culledTriangles=stats.inputTriangles-stats.visibleTriangles;return stats;}

    // 立體三角形與面向相機的線段帶都交由 GPU 深度測試，避免除錯網格穿透場景物件。
    Scene3DStats Submit(DrawList& draw,const Camera3D& camera,Vec2 viewport)const{draw.SetViewProjection(CreateViewProjectionMatrix(camera,viewport.x/(std::max)(1.f,viewport.y)));for(const auto& triangle:triangles_)draw.WorldTriangle(triangle.a,triangle.b,triangle.c,triangle.color);Scene3DStats stats{};stats.inputTriangles=triangles_.size();stats.visibleTriangles=triangles_.size();stats.inputLines=lines_.size();const Vec3 cameraForward=Normalize(camera.target-camera.position);for(const auto& line:lines_){const Vec3 direction=Normalize(line.b-line.a);Vec3 side=Normalize(Cross(direction,cameraForward));if(Length(side)<.0001f)side=Normalize(Cross(direction,camera.up));side=side*(line.width*.0125f);draw.WorldTriangle(line.a-side,line.a+side,line.b+side,line.color);draw.WorldTriangle(line.a-side,line.b+side,line.b-side,line.color);++stats.visibleLines;}return stats;}

    std::size_t TriangleCount()const{return triangles_.size();}
    std::size_t LineCount()const{return lines_.size();}
private:
    std::vector<Triangle3D> triangles_;
    std::vector<Line3D> lines_;
};
}
