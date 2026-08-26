#pragma once
#include "Camera.h"
#include "Geometry3D.h"
#include "Mesh3D.h"
#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace Aevrynd {
struct Triangle3D {Vec3 a{},b{},c{};Color color{};};
struct Line3D {Vec3 a{},b{};float width{1.f};Color color{};};
struct Box3D {Transform3D transform{};Material3D material{};};
struct MeshObject3D {MeshResource3D mesh{};Transform3D transform{};Material3D material{};};
struct MeshLodObject3D {MeshResource3D nearMesh{},farMesh{};float switchDistance{40.f};Transform3D transform{};Material3D material{};};
struct Scene3DSubmissionOptions {bool frustumCulling{true};bool gpuInstancing{true};};
struct Scene3DStats {std::size_t inputTriangles{},visibleTriangles{},culledTriangles{},inputLines{},visibleLines{},meshInstances{},visibleMeshInstances{},culledMeshInstances{},lodMeshInstances{},nearLodInstances{},farLodInstances{},lodTrianglesSaved{};};
enum class SceneObjectType3D {None,Triangle,Box,Mesh};
struct SceneQueryStats3D {std::size_t boundsTested{},boundsRejected{},trianglesTested{};};
struct SceneHit3D {SceneObjectType3D objectType{SceneObjectType3D::None};std::size_t objectIndex{},triangleIndex{};float distance{std::numeric_limits<float>::infinity()};Vec3 position{},normal{},barycentric{};explicit operator bool()const{return objectType!=SceneObjectType3D::None;}};

class Scene3D {
public:
    void Clear(){triangles_.clear();lines_.clear();boxes_.clear();meshes_.clear();lodMeshes_.clear();}
    void Triangle(Vec3 a,Vec3 b,Vec3 c,Color color){triangles_.push_back({a,b,c,color});}
    void Line(Vec3 a,Vec3 b,float width,Color color){lines_.push_back({a,b,width,color});}
    void Box(Vec3 center,Vec3 size,Color color){boxes_.push_back({Transform3D{center,{},size},Material3D{color}});}
    void MaterialBox(Vec3 center,Vec3 size,Material3D material){boxes_.push_back({Transform3D{center,{},size},material});}
    void Mesh(const MeshResource3D& mesh,Transform3D transform,Material3D material={}){if(mesh)meshes_.push_back({mesh,transform,material});}
    void MeshLod(const MeshResource3D& nearMesh,const MeshResource3D& farMesh,float switchDistance,Transform3D transform,Material3D material={}){if(nearMesh&&farMesh&&farMesh.TriangleCount()<=nearMesh.TriangleCount())lodMeshes_.push_back({nearMesh,farMesh,(std::max)(.001f,switchDistance),transform,material});}
    void SetBoxMesh(MeshResource3D mesh){boxMesh_=std::move(mesh);}
    const MeshResource3D& BoxMesh()const{return boxMesh_;}

    SceneHit3D Raycast(Ray3D ray,float maximumDistance=std::numeric_limits<float>::infinity(),SceneQueryStats3D* queryStats=nullptr,bool cullBackfaces=false)const{
        SceneQueryStats3D stats{};SceneHit3D closest{};ray.direction=Normalize(ray.direction);if(Length(ray.direction)<.0001f){if(queryStats)*queryStats=stats;return closest;}const float limit=(std::max)(0.f,maximumDistance);
        auto testTriangle=[&](Vec3 a,Vec3 b,Vec3 c,SceneObjectType3D type,std::size_t objectIndex,std::size_t triangleIndex){++stats.trianglesTested;const RayTriangleHit3D hit=RayIntersectsTriangle(ray,a,b,c,(std::min)(limit,closest.distance),cullBackfaces);if(!hit)return;closest={type,objectIndex,triangleIndex,hit.distance,ray.origin+ray.direction*hit.distance,hit.normal,hit.barycentric};};
        for(std::size_t index=0;index<triangles_.size();++index){const Triangle3D& triangle=triangles_[index];testTriangle(triangle.a,triangle.b,triangle.c,SceneObjectType3D::Triangle,index,0);}
        auto testMesh=[&](const Mesh3D& mesh,const MeshBounds3D& localBounds,const Transform3D& transform,SceneObjectType3D type,std::size_t objectIndex){++stats.boundsTested;const MeshBounds3D bounds=TransformBounds(localBounds,transform);if(!RayIntersectsAabb(ray,bounds.center,bounds.extents,(std::min)(limit,closest.distance))){++stats.boundsRejected;return;}const Matrix4 matrix=transform.Matrix();for(std::size_t triangleIndex=0;triangleIndex<mesh.indices.size()/3;++triangleIndex){const std::size_t first=triangleIndex*3;const auto point=[&](uint32_t index){const Vertex3D& vertex=mesh.vertices[index];return TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});};testTriangle(point(mesh.indices[first]),point(mesh.indices[first+1]),point(mesh.indices[first+2]),type,objectIndex,triangleIndex);}};
        const Mesh3D& unitBox=UnitBox();for(std::size_t index=0;index<boxes_.size();++index)testMesh(unitBox,UnitBoxBounds(),boxes_[index].transform,SceneObjectType3D::Box,index);for(std::size_t index=0;index<meshes_.size();++index){const MeshObject3D& mesh=meshes_[index];if(mesh.mesh.cpuMesh)testMesh(*mesh.mesh.cpuMesh,mesh.mesh.bounds,mesh.transform,SceneObjectType3D::Mesh,index);}for(std::size_t index=0;index<lodMeshes_.size();++index){const MeshLodObject3D& mesh=lodMeshes_[index];if(mesh.nearMesh.cpuMesh)testMesh(*mesh.nearMesh.cpuMesh,mesh.nearMesh.bounds,mesh.transform,SceneObjectType3D::Mesh,meshes_.size()+index);}
        if(queryStats)*queryStats=stats;return closest;
    }

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
        auto submit=[&](const MeshResource3D& resource,const Mesh3D* fallback,const MeshBounds3D* fallbackBounds,const Transform3D& transform,Material3D material){const MeshBounds3D localBounds=resource?resource.bounds:*fallbackBounds,worldBounds=TransformSphereBounds(localBounds,transform);const std::size_t triangles=resource?resource.TriangleCount():fallback->indices.size()/3;if(options.frustumCulling&&!SphereIntersectsFrustum(worldBounds.center,worldBounds.radius,camera,aspect)){++stats.culledMeshInstances;return false;}if(options.gpuInstancing&&resource)draw.MeshInstance(resource.handle,transform.Matrix(),material.tint,material.surface);else if(resource.cpuMesh)SubmitMesh(draw,*resource.cpuMesh,transform,material);else if(fallback)SubmitMesh(draw,*fallback,transform,material);++stats.visibleMeshInstances;stats.visibleTriangles+=triangles;return true;};
        for(const Box3D& box:boxes_)submit(boxMesh_,&fallbackBox,&UnitBoxBounds(),box.transform,box.material);for(const MeshObject3D& mesh:meshes_)submit(mesh.mesh,mesh.mesh.cpuMesh.get(),nullptr,mesh.transform,mesh.material);for(const MeshLodObject3D& lod:lodMeshes_){++stats.lodMeshInstances;const MeshBounds3D worldBounds=TransformSphereBounds(lod.nearMesh.bounds,lod.transform);const bool useFar=Length(worldBounds.center-camera.position)>=lod.switchDistance;const MeshResource3D& selected=useFar?lod.farMesh:lod.nearMesh;if(useFar)++stats.farLodInstances;else ++stats.nearLodInstances;if(submit(selected,selected.cpuMesh.get(),nullptr,lod.transform,lod.material)&&lod.nearMesh.TriangleCount()>selected.TriangleCount())stats.lodTrianglesSaved+=lod.nearMesh.TriangleCount()-selected.TriangleCount();}
        stats.culledTriangles=stats.inputTriangles>stats.visibleTriangles?stats.inputTriangles-stats.visibleTriangles:0;const Vec3 cameraForward=Normalize(camera.target-camera.position);
        for(const auto& line:lines_){const Vec3 direction=Normalize(line.b-line.a);if(Length(direction)<.0001f)continue;Vec3 side=Normalize(Cross(direction,cameraForward));if(Length(side)<.0001f)side=Normalize(Cross(direction,camera.up));if(Length(side)<.0001f)continue;side=side*(line.width*.0125f);const uint32_t base=uint32_t(draw.worldVertices.size()),color=PackColorRgba8(line.color),normal=PackNormalSnorm8({0,1,0},0);for(Vec3 point:{line.a-side,line.a+side,line.b+side,line.b-side})draw.worldVertices.push_back({point.x,point.y,point.z,normal,color});draw.worldIndices.insert(draw.worldIndices.end(),{base,base+1,base+2,base,base+2,base+3});++stats.visibleLines;}
        return stats;
    }

    std::size_t TriangleCount()const{std::size_t count=triangles_.size()+boxes_.size()*12;for(const MeshObject3D& mesh:meshes_)count+=mesh.mesh.TriangleCount();for(const MeshLodObject3D& mesh:lodMeshes_)count+=mesh.nearMesh.TriangleCount();return count;}
    std::size_t LineCount()const{return lines_.size();}
    std::size_t MeshCount()const{return boxes_.size()+meshes_.size()+lodMeshes_.size();}
private:
    static const Mesh3D& UnitBox(){static const Mesh3D mesh=Mesh3D::CreateUnitBox();return mesh;}
    static const MeshBounds3D& UnitBoxBounds(){static const MeshBounds3D bounds=UnitBox().Bounds();return bounds;}
    template<class Callback>void ForEachTriangle(Callback&& callback)const{
        for(const auto& triangle:triangles_)callback(triangle.a,triangle.b,triangle.c,triangle.color);
        const Mesh3D& boxMesh=UnitBox();for(const Box3D& box:boxes_)ForEachMeshTriangle(boxMesh,box.transform,box.material,callback);for(const MeshObject3D& mesh:meshes_)if(mesh.mesh.cpuMesh)ForEachMeshTriangle(*mesh.mesh.cpuMesh,mesh.transform,mesh.material,callback);for(const MeshLodObject3D& mesh:lodMeshes_)if(mesh.nearMesh.cpuMesh)ForEachMeshTriangle(*mesh.nearMesh.cpuMesh,mesh.transform,mesh.material,callback);
    }
    template<class Callback>static void ForEachMeshTriangle(const Mesh3D& mesh,const Transform3D& transform,Material3D material,Callback& callback){const Matrix4 matrix=transform.Matrix();for(std::size_t index=0;index<mesh.indices.size();index+=3){auto vertex=[&](uint32_t item){const Vertex3D& source=mesh.vertices[item];return TransformPoint(matrix,{source.x,source.y,source.z});};Color source=UnpackColorRgba8(mesh.vertices[mesh.indices[index]].color);Color color{source.r*material.tint.r,source.g*material.tint.g,source.b*material.tint.b,source.a*material.tint.a};callback(vertex(mesh.indices[index]),vertex(mesh.indices[index+1]),vertex(mesh.indices[index+2]),color);}}
    std::vector<Triangle3D> triangles_;
    std::vector<Line3D> lines_;
    std::vector<Box3D> boxes_;
    std::vector<MeshObject3D> meshes_;
    std::vector<MeshLodObject3D> lodMeshes_;
    MeshResource3D boxMesh_{};
};
}
