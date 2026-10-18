#pragma once

#include "Mesh3D.h"
#include <bit>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>

namespace Aevrynd {
struct GroundConformSettings3D {
    float fullContactBand{.035f},fadeHeight{.35f},maximumGroundDelta{.24f},rootTolerance{.00005f};
};
struct GroundConformResult3D {
    Mesh3D mesh;
    float sourceMinimumY{},sourceMaximumY{};
    Vec3 root{};
    unsigned sampledVertices{},movedVertices{},affectedFaces{},rebuiltNormals{};
    double maximumGroundDelta{};
};
namespace GroundConformDetail3D {
inline void Need(bool yes,const char* reason){if(!yes)throw std::invalid_argument(reason);}
inline bool Finite(Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
inline std::uint32_t Word(float value){return value==0.f?0u:std::bit_cast<std::uint32_t>(value);}
struct Normal {double x{},y{},z{};};
inline Normal Cross(const Vertex3D& a,const Vertex3D& b,const Vertex3D& c){
    const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
    const double vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
    return {uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx};
}
inline double Dot(Normal a,Normal b){return a.x*b.x+a.y*b.y+a.z*b.z;}
}

// 建立作者候選，不代表整株支撐、風擺、碰撞或材質已通過驗收。
// sample 必須從真正提交的地表三角面取得高度；缺面回傳空值或拋出例外。
// 世界 Y 形變保留原 XYZ 接縫、UV、索引、色彩及 normal.w 語意，不移動上部。
template<class GroundSampler>
[[nodiscard]] GroundConformResult3D CreateGroundConformedMesh3D(const Mesh3D& source,
    const Transform3D& pose,GroundSampler&& sample,const GroundConformSettings3D& settings={}){
    namespace D=GroundConformDetail3D;
    D::Need(source.vertices.size()<=1000000&&source.indices.size()<=6000000&&source.Valid(),"GROUND_CONFORM_TOPOLOGY");
    D::Need(D::Finite(pose.position)&&D::Finite(pose.rotation)&&D::Finite(pose.scale)&&pose.rotation.x==0.f&&pose.rotation.z==0.f&&
        pose.scale.x>0.f&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z,"GROUND_CONFORM_POSE");
    D::Need(std::isfinite(settings.fullContactBand)&&std::isfinite(settings.fadeHeight)&&std::isfinite(settings.maximumGroundDelta)&&
        std::isfinite(settings.rootTolerance)&&settings.fullContactBand>0.f&&settings.fadeHeight>settings.fullContactBand&&
        settings.maximumGroundDelta>0.f&&settings.rootTolerance>0.f,"GROUND_CONFORM_SETTINGS");
    GroundConformResult3D result;result.sourceMinimumY=source.vertices.front().y;result.sourceMaximumY=source.vertices.front().y;
    for(const auto& vertex:source.vertices){
        D::Need(D::Finite({vertex.x,vertex.y,vertex.z})&&std::isfinite(vertex.u)&&std::isfinite(vertex.v),"GROUND_CONFORM_SOURCE_VERTEX");
        const auto normal=UnpackNormalSnorm8(vertex.normal);const auto length=Length({normal.x,normal.y,normal.z});
        D::Need(std::isfinite(length)&&length>.95f&&length<1.05f,"GROUND_CONFORM_SOURCE_NORMAL");
        result.sourceMinimumY=(std::min)(result.sourceMinimumY,vertex.y);result.sourceMaximumY=(std::max)(result.sourceMaximumY,vertex.y);
    }
    D::Need(result.sourceMaximumY>result.sourceMinimumY,"GROUND_CONFORM_SOURCE_HEIGHT");
    const auto matrix=pose.Matrix();result.root=TransformPoint(matrix,{0,result.sourceMinimumY,0});
    D::Need(D::Finite(result.root),"GROUND_CONFORM_WORLD_VERTEX");
    const auto rootHeight=sample(result.root.x,result.root.z);
    D::Need(rootHeight.has_value()&&std::isfinite(*rootHeight),"GROUND_CONFORM_SURFACE_MISSING");
    D::Need(std::abs(double(result.root.y)-*rootHeight)<=settings.rootTolerance,"GROUND_CONFORM_ROOT_NOT_ATTACHED");
    result.mesh=source;
    std::vector<bool> moved(source.vertices.size()),affected(source.vertices.size());
    std::map<std::array<std::uint32_t,3>,std::uint32_t> weldedY;
    for(std::size_t i=0;i<source.vertices.size();++i){const auto& original=source.vertices[i];auto& vertex=result.mesh.vertices[i];
        const auto world=TransformPoint(matrix,{original.x,original.y,original.z});
        D::Need(D::Finite(world),"GROUND_CONFORM_WORLD_VERTEX");
        const double height=(double(original.y)-result.sourceMinimumY)*pose.scale.x;
        if(height<double(settings.fadeHeight)){
            const auto ground=sample(world.x,world.z);++result.sampledVertices;
            D::Need(ground.has_value()&&std::isfinite(*ground),"GROUND_CONFORM_SURFACE_MISSING");
            const double delta=*ground-*rootHeight;result.maximumGroundDelta=(std::max)(result.maximumGroundDelta,std::abs(delta));
            D::Need(std::abs(delta)<=settings.maximumGroundDelta,"GROUND_CONFORM_GROUND_DELTA");
            // 原來源最低 35mm 全量貼地，之後以連續 smoothstep 消退；不靠最終 gap 選根。
            const double t=std::clamp((height-settings.fullContactBand)/(settings.fadeHeight-settings.fullContactBand),0.,1.);
            const double weight=1.-t*t*(3.-2.*t);
            vertex.y=static_cast<float>(double(original.y)+delta*weight/pose.scale.x);
            D::Need(std::isfinite(vertex.y),"GROUND_CONFORM_DERIVED_VERTEX");
            moved[i]=std::bit_cast<std::uint32_t>(vertex.y)!=std::bit_cast<std::uint32_t>(original.y);
            if(moved[i])++result.movedVertices;
        }
        D::Need(D::Finite(TransformPoint(matrix,{vertex.x,vertex.y,vertex.z})),"GROUND_CONFORM_DERIVED_WORLD_VERTEX");
        const std::array<std::uint32_t,3> key{D::Word(original.x),D::Word(original.y),D::Word(original.z)};
        const auto [it,inserted]=weldedY.emplace(key,D::Word(vertex.y));
        D::Need(inserted||it->second==D::Word(vertex.y),"GROUND_CONFORM_SPLIT_WELD");
    }
    std::vector<D::Normal> normals(source.vertices.size());
    for(std::size_t face=0;face<source.indices.size();face+=3){const auto a=source.indices[face],b=source.indices[face+1],c=source.indices[face+2];
        const auto before=D::Cross(source.vertices[a],source.vertices[b],source.vertices[c]);
        const auto after=D::Cross(result.mesh.vertices[a],result.mesh.vertices[b],result.mesh.vertices[c]);
        D::Need(std::isfinite(D::Dot(before,before))&&D::Dot(before,before)>0.,"GROUND_CONFORM_SOURCE_FACE");
        D::Need(std::isfinite(D::Dot(after,after))&&D::Dot(after,after)>0.&&D::Dot(before,after)>0.,"GROUND_CONFORM_FACE_ORIENTATION");
        for(const auto index:{a,b,c}){auto& n=normals[index];n.x+=after.x;n.y+=after.y;n.z+=after.z;}
        if(moved[a]||moved[b]||moved[c]){++result.affectedFaces;affected[a]=affected[b]=affected[c]=true;}
    }
    for(std::size_t i=0;i<source.vertices.size();++i)if(affected[i]){
        const auto n=normals[i];const double length=std::sqrt(D::Dot(n,n));
        D::Need(std::isfinite(length)&&length>0.,"GROUND_CONFORM_DERIVED_NORMAL");
        const Vec3 normal{float(n.x/length),float(n.y/length),float(n.z/length)};
        // W 的原始位元不重新量化；植被風擺／切線手性由原資產決定。
        result.mesh.vertices[i].normal=(PackNormalSnorm8(normal)&0x00ffffffu)|(source.vertices[i].normal&0xff000000u);
        ++result.rebuiltNormals;
    }
    result.mesh.InvalidateRaycastIndex();return result;
}
}
