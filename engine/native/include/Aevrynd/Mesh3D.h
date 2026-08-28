#pragma once
#include "Renderer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <span>
#include <vector>

namespace Aevrynd {
struct Material3D {Color tint{1,1,1,1};Surface3D surface{};bool castsShadow{true};MaterialTextureSetHandle3D textures{};float textureScale{.25f};bool useUv{};MaterialClass3D materialClass{MaterialClass3D::Surface};CharacterShadingModel3D shadingModel{CharacterShadingModel3D::Default};MaterialAlphaMode3D alphaMode{MaterialAlphaMode3D::Opaque};float alphaCutoff{.5f};bool doubleSided{};float normalScale{1.f};float baseTextureScale{},detailTextureScale{};};
struct Transform3D {Vec3 position{},rotation{},scale{1,1,1};Matrix4 Matrix()const{return CreateTransformMatrix(position,rotation,scale);}};
struct MeshBounds3D {Vec3 center{};float radius{};Vec3 extents{};};
struct MeshRaycastEntry3D {Vec3 center{},extents{};std::uint32_t triangleIndex{};};
struct MeshRaycastNode3D {Vec3 center{},extents{};std::uint32_t first{},count{},left{},right{};};
struct MeshRaycastIndex3D {
    std::vector<MeshRaycastEntry3D> entries;
    std::vector<MeshRaycastNode3D> nodes;
    std::size_t RetainedBytes()const{return entries.capacity()*sizeof(MeshRaycastEntry3D)+nodes.capacity()*sizeof(MeshRaycastNode3D);}
};
struct MeshRaycastCache3D {std::atomic<std::shared_ptr<const MeshRaycastIndex3D>> index;};

struct Mesh3D {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    // 網格建立完成後視為不可變；共享快取讓相同 CPU 網格的所有世界實例只建索引一次。
    mutable std::shared_ptr<MeshRaycastCache3D> raycastCache{std::make_shared<MeshRaycastCache3D>()};
    Mesh3D()=default;
    Mesh3D(const Mesh3D& other):vertices(other.vertices),indices(other.indices){}
    Mesh3D& operator=(const Mesh3D& other){if(this!=&other){vertices=other.vertices;indices=other.indices;raycastCache=std::make_shared<MeshRaycastCache3D>();}return *this;}
    Mesh3D(Mesh3D&&)=default;
    Mesh3D& operator=(Mesh3D&&)=default;
    // 公開頂點或索引被修改後必須呼叫此函式，避免沿用舊的碰撞加速索引。
    void InvalidateRaycastIndex()const{raycastCache->index.store({},std::memory_order_release);}
    std::size_t RaycastIndexBytes()const{const auto index=raycastCache->index.load(std::memory_order_acquire);return index?index->RetainedBytes():0;}
    std::size_t TrimRaycastIndex()const{const auto index=raycastCache->index.exchange({},std::memory_order_acq_rel);return index?index->RetainedBytes():0;}
    bool Valid()const{return !vertices.empty()&&!indices.empty()&&indices.size()%3==0&&std::all_of(indices.begin(),indices.end(),[this](uint32_t index){return index<vertices.size();});}
    MeshBounds3D Bounds()const{if(vertices.empty())return{};Vec3 minimum{vertices.front().x,vertices.front().y,vertices.front().z},maximum=minimum;for(const Vertex3D& vertex:vertices){minimum.x=(std::min)(minimum.x,vertex.x);minimum.y=(std::min)(minimum.y,vertex.y);minimum.z=(std::min)(minimum.z,vertex.z);maximum.x=(std::max)(maximum.x,vertex.x);maximum.y=(std::max)(maximum.y,vertex.y);maximum.z=(std::max)(maximum.z,vertex.z);}const Vec3 center=(minimum+maximum)*.5f;float radius=0;for(const Vertex3D& vertex:vertices)radius=(std::max)(radius,Length(Vec3{vertex.x,vertex.y,vertex.z}-center));return{center,radius,(maximum-minimum)*.5f};}
    static Mesh3D CreateUnitBox(){Mesh3D mesh;mesh.vertices.reserve(24);mesh.indices.reserve(36);auto face=[&](std::array<Vec3,4> points,float shade){const uint32_t base=uint32_t(mesh.vertices.size()),color=PackColorRgba8({shade,shade,shade,1}),normal=PackNormalSnorm8(Normalize(Cross(points[2]-points[0],points[1]-points[0])));for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,normal,color});mesh.indices.insert(mesh.indices.end(),{base,base+2,base+1,base,base+3,base+2});};face({Vec3{-.5f,-.5f,-.5f},Vec3{.5f,-.5f,-.5f},Vec3{.5f,.5f,-.5f},Vec3{-.5f,.5f,-.5f}},.55f);face({Vec3{.5f,-.5f,.5f},Vec3{-.5f,-.5f,.5f},Vec3{-.5f,.5f,.5f},Vec3{.5f,.5f,.5f}},.85f);face({Vec3{-.5f,-.5f,.5f},Vec3{-.5f,-.5f,-.5f},Vec3{-.5f,.5f,-.5f},Vec3{-.5f,.5f,.5f}},.68f);face({Vec3{.5f,-.5f,-.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,.5f,.5f},Vec3{.5f,.5f,-.5f}},.76f);face({Vec3{-.5f,.5f,-.5f},Vec3{.5f,.5f,-.5f},Vec3{.5f,.5f,.5f},Vec3{-.5f,.5f,.5f}},1.f);face({Vec3{-.5f,-.5f,.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,-.5f,-.5f},Vec3{-.5f,-.5f,-.5f}},.45f);return mesh;}
    static Mesh3D CreateUnitPyramid(){Mesh3D mesh;mesh.vertices.reserve(16);mesh.indices.reserve(18);auto face=[&](std::span<const Vec3> points,float shade){const uint32_t base=uint32_t(mesh.vertices.size()),color=PackColorRgba8({shade,shade,shade,1});const bool quad=points.size()==4;const Vec3 normal=quad?Normalize(Cross(points[2]-points[0],points[1]-points[0])):Normalize(Cross(points[1]-points[0],points[2]-points[0]));const uint32_t packedNormal=PackNormalSnorm8(normal);for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,packedNormal,color});if(!quad)mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2});else mesh.indices.insert(mesh.indices.end(),{base,base+2,base+1,base,base+3,base+2});};const std::array<Vec3,4> base={Vec3{-.5f,-.5f,.5f},Vec3{.5f,-.5f,.5f},Vec3{.5f,-.5f,-.5f},Vec3{-.5f,-.5f,-.5f}};face(base,.45f);const Vec3 top{0,.5f,0};const std::array<std::array<Vec3,3>,4> sides={{{base[0],base[1],top},{base[1],base[2],top},{base[2],base[3],top},{base[3],base[0],top}}};for(std::size_t index=0;index<sides.size();++index)face(sides[index],.64f+.09f*float(index));return mesh;}
    static Mesh3D CreateUnitCylinder(unsigned sides=12){sides=(std::max)(3u,sides);Mesh3D mesh;mesh.vertices.reserve(std::size_t(sides)*6+2);mesh.indices.reserve(std::size_t(sides)*12);constexpr float Pi=3.14159265358979323846f;for(unsigned side=0;side<sides;++side){const float angle=2.f*Pi*float(side)/float(sides);const Vec3 normal{std::sin(angle),0,std::cos(angle)};mesh.vertices.push_back({normal.x*.5f,-.5f,normal.z*.5f,PackNormalSnorm8(normal),0xffffffffu});mesh.vertices.push_back({normal.x*.5f,.5f,normal.z*.5f,PackNormalSnorm8(normal),0xffffffffu});}const uint32_t bottom=uint32_t(mesh.vertices.size()),top=bottom+1;mesh.vertices.push_back({0,-.5f,0,PackNormalSnorm8({0,-1,0}),0xffffffffu});mesh.vertices.push_back({0,.5f,0,PackNormalSnorm8({0,1,0}),0xffffffffu});for(unsigned side=0;side<sides;++side){const uint32_t next=(side+1)%sides,a=side*2,b=a+1,c=next*2,d=c+1;mesh.indices.insert(mesh.indices.end(),{a,b,c,c,b,d,b,top,d,a,c,bottom});}return mesh;}
    static Mesh3D CreateUnitCone(unsigned sides=12){sides=(std::max)(3u,sides);Mesh3D mesh;mesh.vertices.reserve(std::size_t(sides)+2);mesh.indices.reserve(std::size_t(sides)*6);constexpr float Pi=3.14159265358979323846f;for(unsigned side=0;side<sides;++side){const float angle=2.f*Pi*float(side)/float(sides);const Vec3 radial{std::sin(angle),0,std::cos(angle)},normal=Normalize({radial.x,.5f,radial.z});mesh.vertices.push_back({radial.x*.5f,-.5f,radial.z*.5f,PackNormalSnorm8(normal),0xffffffffu});}const uint32_t apex=uint32_t(mesh.vertices.size()),bottom=apex+1;mesh.vertices.push_back({0,.5f,0,PackNormalSnorm8({0,1,0}),0xffffffffu});mesh.vertices.push_back({0,-.5f,0,PackNormalSnorm8({0,-1,0}),0xffffffffu});for(unsigned side=0;side<sides;++side){const uint32_t next=(side+1)%sides;mesh.indices.insert(mesh.indices.end(),{side,apex,next,side,next,bottom});}return mesh;}
    static Mesh3D CreateUnitSphere(unsigned slices=12,unsigned stacks=8){slices=(std::max)(3u,slices);stacks=(std::max)(2u,stacks);Mesh3D mesh;const unsigned stride=slices+1;mesh.vertices.reserve(std::size_t(stride)*(stacks+1));mesh.indices.reserve(std::size_t(slices)*(stacks-1)*6);constexpr float Pi=3.14159265358979323846f;for(unsigned stack=0;stack<=stacks;++stack){const float latitude=Pi*float(stack)/float(stacks),ring=std::sin(latitude),y=std::cos(latitude);for(unsigned slice=0;slice<=slices;++slice){const float longitude=2.f*Pi*float(slice)/float(slices);const Vec3 normal{ring*std::sin(longitude),y,ring*std::cos(longitude)};mesh.vertices.push_back({normal.x*.5f,normal.y*.5f,normal.z*.5f,PackNormalSnorm8(normal),0xffffffffu});}}for(unsigned stack=0;stack<stacks;++stack)for(unsigned slice=0;slice<slices;++slice){const uint32_t a=stack*stride+slice,b=a+stride,c=b+1,d=a+1;if(stack)mesh.indices.insert(mesh.indices.end(),{a,b,d});if(stack+1<stacks)mesh.indices.insert(mesh.indices.end(),{d,b,c});}return mesh;}
};

struct MeshResource3D {
    MeshHandle3D handle{};MeshBounds3D bounds{};std::size_t vertexCount{},indexCount{};std::shared_ptr<const Mesh3D> cpuMesh;
    explicit operator bool()const{return bool(handle)&&indexCount>0;}
    std::size_t TriangleCount()const{return indexCount/3;}
};

// 純球體裁切不需要計算旋轉矩陣，保持大量實例每幀提交的低成本路徑。
inline MeshBounds3D TransformSphereBounds(const MeshBounds3D& bounds,const Transform3D& transform){const float largestScale=(std::max)({std::abs(transform.scale.x),std::abs(transform.scale.y),std::abs(transform.scale.z)});const bool centered=Length(bounds.center)<.00001f;return{centered?transform.position:TransformPoint(transform.Matrix(),bounds.center),bounds.radius*largestScale,{}};}

// AABB 使用旋轉矩陣絕對值求世界軸投影；無旋轉物件則略過三角函數與矩陣乘法。
inline MeshBounds3D TransformBounds(const MeshBounds3D& bounds,const Transform3D& transform){const float largestScale=(std::max)({std::abs(transform.scale.x),std::abs(transform.scale.y),std::abs(transform.scale.z)});Vec3 localExtents=bounds.extents;if(Length(localExtents)<.00001f)localExtents={bounds.radius,bounds.radius,bounds.radius};if(std::abs(transform.rotation.x)+std::abs(transform.rotation.y)+std::abs(transform.rotation.z)<.000001f){const Vec3 scaledCenter{bounds.center.x*transform.scale.x,bounds.center.y*transform.scale.y,bounds.center.z*transform.scale.z},worldExtents{localExtents.x*std::abs(transform.scale.x),localExtents.y*std::abs(transform.scale.y),localExtents.z*std::abs(transform.scale.z)};return{transform.position+scaledCenter,bounds.radius*largestScale,worldExtents};}const Matrix4 matrix=transform.Matrix();const Vec3 worldExtents{std::abs(matrix[0][0])*localExtents.x+std::abs(matrix[0][1])*localExtents.y+std::abs(matrix[0][2])*localExtents.z,std::abs(matrix[1][0])*localExtents.x+std::abs(matrix[1][1])*localExtents.y+std::abs(matrix[1][2])*localExtents.z,std::abs(matrix[2][0])*localExtents.x+std::abs(matrix[2][1])*localExtents.y+std::abs(matrix[2][2])*localExtents.z};return{TransformPoint(matrix,bounds.center),bounds.radius*largestScale,worldExtents};}

inline Color UnpackColorRgba8(uint32_t packed){constexpr float inverse=1.f/255.f;return{float(packed&255)*inverse,float((packed>>8)&255)*inverse,float((packed>>16)&255)*inverse,float((packed>>24)&255)*inverse};}

// 網格拓撲可跨實例重用；每幀只展開變換後的唯一頂點與偏移索引，避免重複三角形頂點。
inline bool SubmitMesh(DrawList& draw,const Mesh3D& mesh,const Transform3D& transform,Material3D material={}){if(!mesh.Valid())return false;const uint32_t base=uint32_t(draw.worldVertices.size());const Matrix4 matrix=transform.Matrix(),normalMatrix=CreateNormalMatrix(matrix);for(const Vertex3D& vertex:mesh.vertices){Vec3 position=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});const Vec4 unpackedNormal=UnpackNormalSnorm8(vertex.normal);const Vec3 normal=TransformNormal(normalMatrix,{unpackedNormal.x,unpackedNormal.y,unpackedNormal.z});Color source=UnpackColorRgba8(vertex.color);Color color{source.r*material.tint.r,source.g*material.tint.g,source.b*material.tint.b,source.a*material.tint.a};draw.worldVertices.push_back({position.x,position.y,position.z,PackNormalSnorm8(normal,unpackedNormal.w),PackColorRgba8(color),vertex.u,vertex.v});}for(uint32_t index:mesh.indices)draw.worldIndices.push_back(base+index);return true;}
}
