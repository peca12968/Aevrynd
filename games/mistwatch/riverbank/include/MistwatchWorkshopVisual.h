#pragma once
#include "MistwatchWorld.h"
#include <Aevrynd/PlankedPanel3D.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace Mistwatch::WorkshopVisual {
inline constexpr const char* Profile="planked-weathered-bands-v1";
inline constexpr std::array<unsigned,4> BoardCounts{20,16,20,20};
inline constexpr std::array<unsigned,4> SplitAxes{0,2,0,0};
inline constexpr unsigned Boards=76,Vertices=7296,Indices=10032;
inline constexpr float Seam=.007f,Bevel=.004f,LongitudinalMeters=1.4f;
// 真 Worn Planks 原圖的灰褐板內取材；材質須 useUv=true、textureScale=1。
inline constexpr std::array<Aevrynd::Vec2,4> Bands{{
    {132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
    {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
inline constexpr float StaticDecodedScale=2048.f*(16.f/32767.f);

[[nodiscard]] inline Aevrynd::Mesh3D Create() {
    const auto solids=WorkshopSolids();
    if(solids.size()!=BoardCounts.size())throw std::runtime_error("Workshop original solid membership");
    Aevrynd::Mesh3D result;
    result.vertices.reserve(Vertices);result.indices.reserve(Indices);
    for(unsigned part=0;part<BoardCounts.size();++part) {
        const auto& solid=solids[part];
        auto panel=Aevrynd::CreatePlankedPanel3D(solid.center,solid.extents*2.f,
            SplitAxes[part],BoardCounts[part],Seam,Bevel,part*3u);
        if(!panel||panel->vertices.size()!=std::size_t(BoardCounts[part])*96u||
           panel->indices.size()!=std::size_t(BoardCounts[part])*132u)
            throw std::runtime_error("Workshop complete beveled panel");
        for(unsigned board=0;board<BoardCounts[part];++board) {
            const auto first=std::size_t(board)*96u;
            Aevrynd::Vec3 low{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()};
            Aevrynd::Vec3 high=low*-1.f;
            for(std::size_t i=first;i<first+96u;++i) {
                const auto& v=panel->vertices[i];
                low.x=(std::min)(low.x,v.x);high.x=(std::max)(high.x,v.x);
                low.y=(std::min)(low.y,v.y);high.y=(std::max)(high.y,v.y);
                low.z=(std::min)(low.z,v.z);high.z=(std::max)(high.z,v.z);
            }
            if(!(high.x>low.x&&high.y>low.y&&high.z>low.z))
                throw std::runtime_error("Workshop nonzero board bounds");
            const auto unit=[](float p,float lo,float hi) {
                const float value=(p-lo)/(hi-lo);
                if(!std::isfinite(value)||value<0.f||value>1.f)
                    throw std::runtime_error("Workshop transverse source extent");
                return value;
            };
            const auto band=Bands[(board+part*3u)%4u];
            const float phase=.17f*float((board+part*2u)%5u);
            for(std::size_t i=first;i<first+96u;++i) {
                auto& v=panel->vertices[i];const auto n=Aevrynd::UnpackNormalSnorm8(v.normal);
                const float ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);
                float across{},along{};
                // 屋頂主面沿Z；端面與倒角保留非退化最大法線軸投影。
                if(part==0) {
                    if(ay>=ax&&ay>=az){across=unit(v.x,low.x,high.x);along=v.z-low.z;}
                    else if(ax>=az){across=unit(v.y,low.y,high.y);along=v.z-low.z;}
                    else{across=unit(v.y,low.y,high.y);along=v.x-low.x;}
                }else if(ay>=ax&&ay>=az) {
                    across=SplitAxes[part]==0?unit(v.x,low.x,high.x):unit(v.z,low.z,high.z);
                    const float depth=SplitAxes[part]==0?unit(v.z,low.z,high.z):unit(v.x,low.x,high.x);
                    const float end=v.y>=low.y+(high.y-low.y)*.5f?high.y-low.y:0.f;
                    along=end+depth*.05f;
                }else if(ax>=az){across=unit(v.z,low.z,high.z);along=v.y-low.y;}
                else{across=unit(v.x,low.x,high.x);along=v.y-low.y;}
                v.u=(band.x+across*(band.y-band.x))/StaticDecodedScale;
                v.v=(phase+along/LongitudinalMeters)/StaticDecodedScale;
                if(!std::isfinite(v.u)||!std::isfinite(v.v))
                    throw std::runtime_error("Workshop nonfinite authored UV");
            }
        }
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        result.vertices.insert(result.vertices.end(),panel->vertices.begin(),panel->vertices.end());
        for(const auto index:panel->indices)result.indices.push_back(base+index);
    }
    if(result.vertices.size()!=Vertices||result.indices.size()!=Indices||!result.Valid())
        throw std::runtime_error("Workshop final visual membership");
    for(std::size_t i=0;i<result.indices.size();i+=3u) {
        const auto& a=result.vertices[result.indices[i]];const auto& b=result.vertices[result.indices[i+1]];
        const auto& c=result.vertices[result.indices[i+2]];
        const double jac=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);
        if(!std::isfinite(jac)||jac==0)throw std::runtime_error("Workshop nonzero authored UV area");
    }
    result.InvalidateRaycastIndex();return result;
}
[[nodiscard]] inline std::uint64_t Fingerprint(const Aevrynd::Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& v:mesh.vertices)for(const float value:{v.u,v.v}) {
        const auto bits=std::bit_cast<std::uint32_t>(value);
        for(unsigned byte=0;byte<4u;++byte){hash^=(bits>>(byte*8u))&255u;hash*=1099511628211ull;}
    }
    return hash;
}
}
