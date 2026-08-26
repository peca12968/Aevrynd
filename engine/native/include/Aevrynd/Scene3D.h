#pragma once
#include "Camera.h"
#include "Mesh3D.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace Aevrynd {
struct Triangle3D {Vec3 a{},b{},c{};Color color{};};
struct Line3D {Vec3 a{},b{};float width{1.f};Color color{};};
struct Box3D {Transform3D transform{};Material3D material{};};
struct MeshObject3D {MeshResource3D mesh{};Transform3D transform{};Material3D material{};};
struct Scene3DSubmissionOptions {bool frustumCulling{true};bool gpuInstancing{true};};
struct Scene3DStats {std::size_t inputTriangles{},visibleTriangles{},culledTriangles{},inputLines{},visibleLines{},meshInstances{},visibleMeshInstances{},culledMeshInstances{};};

class Scene3D {
public:
    void Clear(){triangles_.clear();lines_.clear();boxes_.clear();meshes_.clear();}
    void Triangle(Vec3 a,Vec3 b,Vec3 c,Color color){triangles_.push_back({a,b,c,color});}
    void Line(Vec3 a,Vec3 b,float width,Color color){lines_.push_back({a,b,width,color});}
    void Box(Vec3 center,Vec3 size,Color color){boxes_.push_back({Transform3D{center,{},size},Material3D{color}});}
    void Mesh(const MeshResource3D& mesh,Transform3D transform,Material3D material={}){if(mesh)meshes_.push_back({mesh,transform,material});}
    void SetBoxMesh(MeshResource3D mesh){boxMesh_=std::move(mesh);}
    const MeshResource3D& BoxMesh()const{return boxMesh_;}

    // CPU 路徑保留作為投影與排序的參考實作，方便對照硬體深度結果。
    Scene3DStats Render(DrawList& draw,const Camera3D& camera,Vec2 viewport)const{
        struct VisibleTriangle {ProjectedPoint a,b,c;float depth{};Color color{};};
        std::vector<VisibleTriangle> visible;
        visible.reserve(TriangleCount());
        ForEachTriangle([&](Vec3 a,Vec3 b,Vec3 c,Color color){ProjectedPoint pa=ProjectToViewport(a,camera,viewport),pb=ProjectToViewport(b,camera,viewport),pc=ProjectToViewport(c,camera,viewport);if(pa.visible&&pb.visible&&pc.visible)visible.push_back({pa,pb,pc,(pa.depth+pb.depth+pc.depth)/3.f,color});});
        std::stable_sort(visible.begin(),visible.end(),[](const auto& left,const auto& right){return left.depth>right.depth;});
        for(const auto& triangle:visible)draw.Triangle(triangle.a.position,triangle.b.position,triangle.c.position,triangle.color);
        Scene3DStats stats{};stats.inputTriangles=TriangleCount();stats.visibleTriangles=visible.size();stats.culledTriangles=stats.inputTriangles-stats.visibleTriangles;stats.inputLines=lines_.size();stats.meshInstances=MeshCount();
        for(const auto& line:lines_){ProjectedPoint a=ProjectToViewport(line.a,camera,viewport),b=ProjectToViewport(line.b,camera,viewport);if(!a.visible||!b.visible)continue;draw.Line(a.position,b.position,line.width,line.color);++stats.visibleLines;}
        return stats;
    }

    // 網格與線段合併成單一索引批次，由 GPU 深度測試處理所有遮擋。
    Scene3DStats Submit(DrawList& draw,const Camera3D& camera,Vec2 viewport,Scene3DSubmissionOptions options={})const{
        draw.SetViewProjection(CreateViewProjectionMatrix(camera,viewport.x/(std::max)(1.f,viewport.y)));
        for(const auto& triangle:triangles_)draw.WorldTriangle(triangle.a,triangle.b,triangle.c,triangle.color);
        Scene3DStats stats{};stats.inputTriangles=TriangleCount();stats.visibleTriangles=triangles_.size();stats.inputLines=lines_.size();stats.meshInstances=MeshCount();const float aspect=viewport.x/(std::max)(1.f,viewport.y);const Mesh3D& fallbackBox=UnitBox();
        auto submit=[&](const MeshResource3D& resource,const Mesh3D* fallback,const MeshBounds3D* fallbackBounds,const Transform3D& transform,Material3D material){const MeshBounds3D localBounds=resource?resource.bounds:*fallbackBounds,worldBounds=TransformBounds(localBounds,transform);const std::size_t triangles=resource?resource.TriangleCount():fallback->indices.size()/3;if(options.frustumCulling&&!SphereIntersectsFrustum(worldBounds.center,worldBounds.radius,camera,aspect)){++stats.culledMeshInstances;return;}if(options.gpuInstancing&&resource)draw.MeshInstance(resource.handle,transform.Matrix(),material.tint);else if(resource.cpuMesh)SubmitMesh(draw,*resource.cpuMesh,transform,material);else if(fallback)SubmitMesh(draw,*fallback,transform,material);++stats.visibleMeshInstances;stats.visibleTriangles+=triangles;};
        for(const Box3D& box:boxes_)submit(boxMesh_,&fallbackBox,&UnitBoxBounds(),box.transform,box.material);for(const MeshObject3D& mesh:meshes_)submit(mesh.mesh,mesh.mesh.cpuMesh.get(),nullptr,mesh.transform,mesh.material);
        stats.culledTriangles=stats.inputTriangles-stats.visibleTriangles;const Vec3 cameraForward=Normalize(camera.target-camera.position);
        for(const auto& line:lines_){const Vec3 direction=Normalize(line.b-line.a);if(Length(direction)<.0001f)continue;Vec3 side=Normalize(Cross(direction,cameraForward));if(Length(side)<.0001f)side=Normalize(Cross(direction,camera.up));if(Length(side)<.0001f)continue;side=side*(line.width*.0125f);const uint32_t base=uint32_t(draw.worldVertices.size()),color=PackColorRgba8(line.color);for(Vec3 point:{line.a-side,line.a+side,line.b+side,line.b-side})draw.worldVertices.push_back({point.x,point.y,point.z,color});draw.worldIndices.insert(draw.worldIndices.end(),{base,base+1,base+2,base,base+2,base+3});++stats.visibleLines;}
        return stats;
    }

    std::size_t TriangleCount()const{std::size_t count=triangles_.size()+boxes_.size()*12;for(const MeshObject3D& mesh:meshes_)count+=mesh.mesh.TriangleCount();return count;}
    std::size_t LineCount()const{return lines_.size();}
    std::size_t MeshCount()const{return boxes_.size()+meshes_.size();}
private:
    static const Mesh3D& UnitBox(){static const Mesh3D mesh=Mesh3D::CreateUnitBox();return mesh;}
    static const MeshBounds3D& UnitBoxBounds(){static const MeshBounds3D bounds=UnitBox().Bounds();return bounds;}
    template<class Callback>void ForEachTriangle(Callback&& callback)const{
        for(const auto& triangle:triangles_)callback(triangle.a,triangle.b,triangle.c,triangle.color);
        const Mesh3D& boxMesh=UnitBox();for(const Box3D& box:boxes_)ForEachMeshTriangle(boxMesh,box.transform,box.material,callback);for(const MeshObject3D& mesh:meshes_)if(mesh.mesh.cpuMesh)ForEachMeshTriangle(*mesh.mesh.cpuMesh,mesh.transform,mesh.material,callback);
    }
    template<class Callback>static void ForEachMeshTriangle(const Mesh3D& mesh,const Transform3D& transform,Material3D material,Callback& callback){const Matrix4 matrix=transform.Matrix();for(std::size_t index=0;index<mesh.indices.size();index+=3){auto vertex=[&](uint32_t item){const Vertex3D& source=mesh.vertices[item];return TransformPoint(matrix,{source.x,source.y,source.z});};Color source=UnpackColorRgba8(mesh.vertices[mesh.indices[index]].color);Color color{source.r*material.tint.r,source.g*material.tint.g,source.b*material.tint.b,source.a*material.tint.a};callback(vertex(mesh.indices[index]),vertex(mesh.indices[index+1]),vertex(mesh.indices[index+2]),color);}}
    std::vector<Triangle3D> triangles_;
    std::vector<Line3D> lines_;
    std::vector<Box3D> boxes_;
    std::vector<MeshObject3D> meshes_;
    MeshResource3D boxMesh_{};
};
}
