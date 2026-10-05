#pragma once
#include "MistwatchCanopyPopulation.h"

namespace Mistwatch {
inline Mesh3D PlantDerivedRootStem(const Mesh3D& derived,const CanopyGeometry& originalGeometry,
    const Transform3D& pose,const HeightField3D& terrain) {
    if(!derived.Valid())throw std::invalid_argument("Invalid derived root mesh");
    auto result=RootCanopyToTerrain(derived,originalGeometry,pose,terrain);
    // 原作者錨點與原完整碰撞不改；新短幹僅最低10cm漸入地面2cm。
    for(std::size_t i=0;i<result.vertices.size();++i){
        const float t=std::clamp((derived.vertices[i].y-originalGeometry.rootMinimum)/.10f,0.f,1.f);
        result.vertices[i].y-=.02f*(1.f-t*t*(3.f-2.f*t))/pose.scale.x;
    }
    std::vector<Vec3> normals(result.vertices.size());
    for(std::size_t f=0;f<result.indices.size();f+=3){
        const auto a=result.indices[f],b=result.indices[f+1],c=result.indices[f+2];
        const auto point=[&](unsigned i){const auto& v=result.vertices[i];return Vec3{v.x,v.y,v.z};};
        const auto n=Cross(point(b)-point(a),point(c)-point(a));
        normals[a]=normals[a]+n;normals[b]=normals[b]+n;normals[c]=normals[c]+n;
    }
    for(std::size_t i=0;i<result.vertices.size();++i)if(derived.vertices[i].y-originalGeometry.rootMinimum<.35f){
        if(!Session::Finite(normals[i])||Length(normals[i])<1.e-8f)throw std::invalid_argument("Degenerate rooted stem normal");
        result.vertices[i].normal=PackNormalSnorm8(Normalize(normals[i]),UnpackNormalSnorm8(derived.vertices[i].normal).w);
    }
    return result;
}

inline BoxCollider3D ExtendRootCollision(BoxCollider3D original,const Mesh3D& rooted,const Transform3D& pose) {
    Vec3 minimum=original.center-original.extents,maximum=original.center+original.extents;
    const auto matrix=pose.Matrix();
    for(const auto& v:rooted.vertices)Expand(minimum,maximum,TransformPoint(matrix,{v.x,v.y,v.z}));
    // 水平包絡不重新round-trip，保持原作者碰撞與散布的float身分；只允許擴充Y。
    if(minimum.x<original.center.x-original.extents.x-1.e-4f||maximum.x>original.center.x+original.extents.x+1.e-4f||
       minimum.z<original.center.z-original.extents.z-1.e-4f||maximum.z>original.center.z+original.extents.z+1.e-4f)
        throw std::invalid_argument("Derived root extends horizontal envelope");
    auto result=original;result.center.y=(minimum.y+maximum.y)*.5f;result.extents.y=(maximum.y-minimum.y)*.5f;
    return result;
}

// 真三角面與每個地形三角面在XZ做交集；高度差為線性，極值必在交集頂點。
// 不只靠中心／頂點抽樣就宣稱地下封口安全。
struct RootGroundRange {float minimum{10000},maximum{-10000};unsigned intersections{},points{};};
inline float RootOrientation(Vec3 a,Vec3 b,Vec3 c){return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);}
inline RootGroundRange RootGroundOverlay(std::array<Vec3,3> face,const HeightField3D& terrain) {
    RootGroundRange range;const auto& s=terrain.Settings();
    float minX=face[0].x,maxX=minX,minZ=face[0].z,maxZ=minZ;
    for(const auto p:face){
        if(!Session::Finite(p)||!terrain.Contains(p.x,p.z))throw std::invalid_argument("Root outside terrain");
        minX=(std::min)(minX,p.x);maxX=(std::max)(maxX,p.x);minZ=(std::min)(minZ,p.z);maxZ=(std::max)(maxZ,p.z);
    }
    const auto cell=[&](float p,float origin,unsigned limit){return std::clamp(int(std::floor((p-origin)/s.cellSize)),0,int(limit)-2);};
    for(int z=cell(minZ,s.origin.z,s.depth);z<=cell(maxZ,s.origin.z,s.depth);++z)
    for(int x=cell(minX,s.origin.x,s.width);x<=cell(maxX,s.origin.x,s.width);++x){
        const auto a=terrain.SamplePosition(unsigned(x),unsigned(z)),b=terrain.SamplePosition(unsigned(x+1),unsigned(z));
        const auto c=terrain.SamplePosition(unsigned(x),unsigned(z+1)),d=terrain.SamplePosition(unsigned(x+1),unsigned(z+1));
        for(const auto tri:std::array<std::array<Vec3,3>,2>{{{a,b,c},{b,d,c}}}){
            std::vector<Vec3> polygon(face.begin(),face.end());
            for(unsigned edge=0;edge<3&&!polygon.empty();++edge){
                std::vector<Vec3> clipped;const auto p=tri[edge],q=tri[(edge+1)%3];
                for(std::size_t i=0;i<polygon.size();++i){
                    const auto v=polygon[i],w=polygon[(i+1)%polygon.size()];
                    const float dv=RootOrientation(p,q,v),dw=RootOrientation(p,q,w);
                    if(dv>=0)clipped.push_back(v);
                    if((dv>=0)!=(dw>=0))clipped.push_back(v+(w-v)*(dv/(dv-dw)));
                }
                polygon=std::move(clipped);
            }
            if(polygon.empty())continue;
            ++range.intersections;
            for(const auto p:polygon){const float gap=p.y-terrain.SampleHeight(p.x,p.z);
                range.minimum=(std::min)(range.minimum,gap);range.maximum=(std::max)(range.maximum,gap);++range.points;
            }
        }
    }
    if(range.points==0)throw std::invalid_argument("Root overlay empty");
    return range;
}
}
