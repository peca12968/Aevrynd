#pragma once
#include "Renderer.h"
#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace Aevrynd {
struct Material3D {Color tint{1,1,1,1};};
struct Transform3D {Vec3 position{},rotation{},scale{1,1,1};Matrix4 Matrix()const{return CreateTransformMatrix(position,rotation,scale);}};

struct Mesh3D {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    bool Valid()const{return !vertices.empty()&&!indices.empty()&&indices.size()%3==0&&std::all_of(indices.begin(),indices.end(),[this](uint32_t index){return index<vertices.size();});}
    static Mesh3D CreateUnitBox(){Mesh3D mesh;mesh.vertices.reserve(24);mesh.indices.reserve(36);auto face=[&](std::array<Vec3,4> points,float shade){const uint32_t base=uint32_t(mesh.vertices.size()),color=PackColorRgba8({shade,shade,shade,1});for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,color});mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});};face({Vec3{-.5f,-.5f,-.5f},Vec3{.5f,-.5f,-.5f},Vec3{.5f,.5f,-.5f},Vec3{-.5f,.5f,-.5f}},.55f);face({Vec3{.5f,-.5f,.5f},Vec3{-.5f,-.5f,.5f},Vec3{-.5f,.5f,.5f},Vec3{.5f,.5f,.5f}},.85f);face({Vec3{-.5f,-.5f,.5f},Vec3{-.5f,-.5f,-.5f},Vec3{-.5f,.5f,-.5f},Vec3{-.5f,.5f,.5f}},.68f);face({Vec3{.5f,-.5f,-.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,.5f,.5f},Vec3{.5f,.5f,-.5f}},.76f);face({Vec3{-.5f,.5f,-.5f},Vec3{.5f,.5f,-.5f},Vec3{.5f,.5f,.5f},Vec3{-.5f,.5f,.5f}},1.f);face({Vec3{-.5f,-.5f,.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,-.5f,-.5f},Vec3{-.5f,-.5f,-.5f}},.45f);return mesh;}
};

inline Color UnpackColorRgba8(uint32_t packed){constexpr float inverse=1.f/255.f;return{float(packed&255)*inverse,float((packed>>8)&255)*inverse,float((packed>>16)&255)*inverse,float((packed>>24)&255)*inverse};}

// 網格拓撲可跨實例重用；每幀只展開變換後的唯一頂點與偏移索引，避免重複三角形頂點。
inline bool SubmitMesh(DrawList& draw,const Mesh3D& mesh,const Transform3D& transform,Material3D material={}){if(!mesh.Valid())return false;const uint32_t base=uint32_t(draw.worldVertices.size());const Matrix4 matrix=transform.Matrix();for(const Vertex3D& vertex:mesh.vertices){Vec3 position=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});Color source=UnpackColorRgba8(vertex.color);Color color{source.r*material.tint.r,source.g*material.tint.g,source.b*material.tint.b,source.a*material.tint.a};draw.worldVertices.push_back({position.x,position.y,position.z,PackColorRgba8(color)});}for(uint32_t index:mesh.indices)draw.worldIndices.push_back(base+index);return true;}
}
