#pragma once
#include "MistwatchRootStemTerrain.h"

namespace Mistwatch::ValleyTerrain {
struct SurfaceCell {unsigned x{},z{};float minimumGap{};};
struct SupportSurface {std::vector<SurfaceCell> cells;unsigned triangles{},intersections{},contactCells{};};
// 真實可見網格與原地形逐三角相交；低於15cm的面格決定接地帶，不用整個模型XZ外框。
// 所有較高三角面也保留，之後限制地形不能新增穿透；15cm是作者帶寬，不是接觸驗收容差。
inline SupportSurface MeasureSupport(const Aevrynd::Mesh3D& mesh,const Aevrynd::Transform3D& pose,
    const Aevrynd::HeightField3D& terrain){
    if(!mesh.Valid()||mesh.indices.size()>1500000||!Session::Finite(pose.position)||!Session::Finite(pose.rotation)||
        !Session::Finite(pose.scale)||pose.scale.x<=0||pose.scale.y<=0||pose.scale.z<=0)
        throw std::invalid_argument("VALLEY_CONTACT_SOURCE");
    SupportSurface out;const auto& s=terrain.Settings();const auto matrix=pose.Matrix();
    std::vector<float> minimum(std::size_t(s.width-1)*(s.depth-1),std::numeric_limits<float>::infinity());
    const auto cell=[&](float p,float origin,unsigned count){return std::clamp(int(std::floor((p-origin)/s.cellSize)),0,int(count)-2);};
    for(std::size_t f=0;f<mesh.indices.size();f+=3){std::array<Aevrynd::Vec3,3> face;
        for(unsigned j=0;j<3;++j){const auto& v=mesh.vertices[mesh.indices[f+j]];face[j]=Aevrynd::TransformPoint(matrix,{v.x,v.y,v.z});
            if(!Session::Finite(face[j])||!terrain.Contains(face[j].x,face[j].z))throw std::invalid_argument("VALLEY_CONTACT_OUTSIDE_TERRAIN");}
        ++out.triangles;
        const float minX=(std::min)({face[0].x,face[1].x,face[2].x}),maxX=(std::max)({face[0].x,face[1].x,face[2].x});
        const float minZ=(std::min)({face[0].z,face[1].z,face[2].z}),maxZ=(std::max)({face[0].z,face[1].z,face[2].z});
        for(int z=cell(minZ,s.origin.z,s.depth);z<=cell(maxZ,s.origin.z,s.depth);++z)
        for(int x=cell(minX,s.origin.x,s.width);x<=cell(maxX,s.origin.x,s.width);++x){
            const auto a=terrain.SamplePosition(unsigned(x),unsigned(z)),b=terrain.SamplePosition(unsigned(x+1),unsigned(z));
            const auto c=terrain.SamplePosition(unsigned(x),unsigned(z+1)),d=terrain.SamplePosition(unsigned(x+1),unsigned(z+1));
            for(const auto tri:std::array<std::array<Aevrynd::Vec3,3>,2>{{{a,b,c},{b,d,c}}}){
                std::vector<Aevrynd::Vec3> polygon(face.begin(),face.end());
                for(unsigned edge=0;edge<3&&!polygon.empty();++edge){std::vector<Aevrynd::Vec3> clipped;
                    const auto p=tri[edge],q=tri[(edge+1)%3];
                    for(std::size_t i=0;i<polygon.size();++i){const auto v=polygon[i],w=polygon[(i+1)%polygon.size()];
                        const float dv=RootOrientation(p,q,v),dw=RootOrientation(p,q,w);
                        if(dv>=0)clipped.push_back(v);
                        if((dv>=0)!=(dw>=0))clipped.push_back(v+(w-v)*(dv/(dv-dw)));
                    }polygon=std::move(clipped);
                }
                if(polygon.empty())continue;
                ++out.intersections;auto& gap=minimum[std::size_t(z)*(s.width-1)+unsigned(x)];
                for(const auto p:polygon)gap=(std::min)(gap,p.y-terrain.SampleHeight(p.x,p.z));
            }
        }
    }
    for(unsigned z=0;z+1<s.depth;++z)for(unsigned x=0;x+1<s.width;++x){const float gap=minimum[std::size_t(z)*(s.width-1)+x];
        if(!std::isfinite(gap))continue;
        out.cells.push_back({x,z,gap});out.contactCells+=gap<=.15f;
    }
    if(out.cells.empty()||!out.contactCells)throw std::invalid_argument("VALLEY_CONTACT_NO_ACTUAL_GROUND_BAND");
    return out;
}
}
