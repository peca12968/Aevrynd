#pragma once
#include "Renderer.h"
#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <vector>

namespace Aevrynd {
struct Material3D {Color tint{1,1,1,1};};
struct Transform3D {Vec3 position{},rotation{},scale{1,1,1};Matrix4 Matrix()const{return CreateTransformMatrix(position,rotation,scale);}};
struct MeshBounds3D {Vec3 center{};float radius{};};

struct Mesh3D {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    bool Valid()const{return !vertices.empty()&&!indices.empty()&&indices.size()%3==0&&std::all_of(indices.begin(),indices.end(),[this](uint32_t index){return index<vertices.size();});}
    MeshBounds3D Bounds()const{if(vertices.empty())return{};Vec3 minimum{vertices.front().x,vertices.front().y,vertices.front().z},maximum=minimum;for(const Vertex3D& vertex:vertices){minimum.x=(std::min)(minimum.x,vertex.x);minimum.y=(std::min)(minimum.y,vertex.y);minimum.z=(std::min)(minimum.z,vertex.z);maximum.x=(std::max)(maximum.x,vertex.x);maximum.y=(std::max)(maximum.y,vertex.y);maximum.z=(std::max)(maximum.z,vertex.z);}const Vec3 center=(minimum+maximum)*.5f;float radius=0;for(const Vertex3D& vertex:vertices)radius=(std::max)(radius,Length(Vec3{vertex.x,vertex.y,vertex.z}-center));return{center,radius};}
    static Mesh3D CreateUnitBox(){Mesh3D mesh;mesh.vertices.reserve(24);mesh.indices.reserve(36);auto face=[&](std::array<Vec3,4> points,float shade){const uint32_t base=uint32_t(mesh.vertices.size()),color=PackColorRgba8({shade,shade,shade,1});for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,color});mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});};face({Vec3{-.5f,-.5f,-.5f},Vec3{.5f,-.5f,-.5f},Vec3{.5f,.5f,-.5f},Vec3{-.5f,.5f,-.5f}},.55f);face({Vec3{.5f,-.5f,.5f},Vec3{-.5f,-.5f,.5f},Vec3{-.5f,.5f,.5f},Vec3{.5f,.5f,.5f}},.85f);face({Vec3{-.5f,-.5f,.5f},Vec3{-.5f,-.5f,-.5f},Vec3{-.5f,.5f,-.5f},Vec3{-.5f,.5f,.5f}},.68f);face({Vec3{.5f,-.5f,-.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,.5f,.5f},Vec3{.5f,.5f,-.5f}},.76f);face({Vec3{-.5f,.5f,-.5f},Vec3{.5f,.5f,-.5f},Vec3{.5f,.5f,.5f},Vec3{-.5f,.5f,.5f}},1.f);face({Vec3{-.5f,-.5f,.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,-.5f,-.5f},Vec3{-.5f,-.5f,-.5f}},.45f);return mesh;}
    static Mesh3D CreateUnitPyramid(){Mesh3D mesh;mesh.vertices.reserve(16);mesh.indices.reserve(18);auto face=[&](std::span<const Vec3> points,float shade){const uint32_t base=uint32_t(mesh.vertices.size()),color=PackColorRgba8({shade,shade,shade,1});for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,color});if(points.size()==3)mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2});else mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});};const std::array<Vec3,4> base={Vec3{-.5f,-.5f,.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,-.5f,-.5f},Vec3{-.5f,-.5f,-.5f}};face(base,.45f);const Vec3 top{0,.5f,0};const std::array<std::array<Vec3,3>,4> sides={{{base[0],base[1],top},{base[1],base[2],top},{base[2],base[3],top},{base[3],base[0],top}}};for(std::size_t index=0;index<sides.size();++index)face(sides[index],.64f+.09f*float(index));return mesh;}
};

struct MeshResource3D {
    MeshHandle3D handle{};MeshBounds3D bounds{};std::size_t vertexCount{},indexCount{};std::shared_ptr<const Mesh3D> cpuMesh;
    explicit operator bool()const{return bool(handle)&&indexCount>0;}
    std::size_t TriangleCount()const{return indexCount/3;}
};

// 非等比縮放以最大軸放大球體，旋轉後仍保守包住完整網格。
inline MeshBounds3D TransformBounds(const MeshBounds3D& bounds,const Transform3D& transform){const float largestScale=(std::max)({std::abs(transform.scale.x),std::abs(transform.scale.y),std::abs(transform.scale.z)});const bool centered=std::abs(bounds.center.x)<.00001f&&std::abs(bounds.center.y)<.00001f&&std::abs(bounds.center.z)<.00001f;return{centered?transform.position:TransformPoint(transform.Matrix(),bounds.center),bounds.radius*largestScale};}

inline Color UnpackColorRgba8(uint32_t packed){constexpr float inverse=1.f/255.f;return{float(packed&255)*inverse,float((packed>>8)&255)*inverse,float((packed>>16)&255)*inverse,float((packed>>24)&255)*inverse};}

// 網格拓撲可跨實例重用；每幀只展開變換後的唯一頂點與偏移索引，避免重複三角形頂點。
inline bool SubmitMesh(DrawList& draw,const Mesh3D& mesh,const Transform3D& transform,Material3D material={}){if(!mesh.Valid())return false;const uint32_t base=uint32_t(draw.worldVertices.size());const Matrix4 matrix=transform.Matrix();for(const Vertex3D& vertex:mesh.vertices){Vec3 position=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});Color source=UnpackColorRgba8(vertex.color);Color color{source.r*material.tint.r,source.g*material.tint.g,source.b*material.tint.b,source.a*material.tint.a};draw.worldVertices.push_back({position.x,position.y,position.z,PackColorRgba8(color)});}for(uint32_t index:mesh.indices)draw.worldIndices.push_back(base+index);return true;}
}
