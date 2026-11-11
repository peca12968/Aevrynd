#pragma once
#include "MistwatchBearingBridge.h"
#include "MistwatchWorkshopVisual.h"
#include <Aevrynd/BeveledBox3D.h>

namespace Mistwatch::BridgeTimberVisual {
inline constexpr const char* Profile="bearing-bridge-grain-bevel-v2";
inline constexpr unsigned Members=53,Vertices=Members*96u,Indices=Members*132u;
inline constexpr float GrainMeters=1.4f;
struct Range {std::size_t part{},firstVertex{},firstIndex{};};
struct Model {Aevrynd::Mesh3D wood;std::vector<Range> ranges;};
inline Model Create(const BearingBridge::Model& original) {
    using namespace Aevrynd;
    if(original.parts.size()!=57||original.additions.size()!=25)
        throw std::invalid_argument("BRIDGE_TIMBER_ORIGINAL_CONSTRUCTION");
    Model out;out.wood.vertices.reserve(Vertices);out.wood.indices.reserve(Indices);
    for(std::size_t part=0;part<original.parts.size();++part) {
        const auto& p=original.parts[part];if(p.role==BearingBridge::Role::Footing)continue;
        const auto first=out.wood.vertices.size();
        out.ranges.push_back({part,first,out.wood.indices.size()});
        // 邊緣只向既有實體內倒角，原中心、包圍盒、橋面高度及碰撞不變。
        const float bevel=(std::min)(.006f,(std::min)({p.size.x,p.size.y,p.size.z})*.06f);
        if(!AppendBeveledBox3D(out.wood,p.center,p.size,bevel,{p.tone,p.tone,p.tone,1}))
            throw std::runtime_error("BRIDGE_TIMBER_BEVELED_MEMBER");
        const std::array<float,3> lengths{p.size.x,p.size.y,p.size.z};
        const auto band=WorkshopVisual::Bands[(part*3u+1u)%WorkshopVisual::Bands.size()];
        // 每塊木料各自偏移、調整紋理密度與方向，打散相鄰木板上重複的木節。
        const float phase=(float((part*37u+11u)%67u)+.5f)/67.f;
        const float grainMeters=GrainMeters+.01f*float(int((part*13u+3u)%17u)-8);
        const bool reverse=(part*19u+7u)%23u<11u;
        for(std::size_t i=first;i<out.wood.vertices.size();++i) {
            auto& v=out.wood.vertices[i];const auto packed=UnpackNormalSnorm8(v.normal);
            const std::array<float,3> normals{packed.x,packed.y,packed.z};
            const std::array<float,3> local{v.x-p.center.x,v.y-p.center.y,v.z-p.center.z};
            unsigned plane{};for(unsigned axis=1;axis<3;++axis)
                if(std::abs(normals[axis])>std::abs(normals[plane]))plane=axis;
            unsigned grain=plane==0?1u:0u;
            for(unsigned axis=0;axis<3;++axis)if(axis!=plane&&lengths[axis]>lengths[grain])grain=axis;
            const unsigned across=3u-plane-grain;
            // 板面長軸沿實際米制木紋；端面／倒角各自投影，避免方盒每面塞入整張多板圖。
            const float transverse=local[across]/lengths[across]+.5f;
            v.u=(band.x+transverse*(band.y-band.x))/WorkshopVisual::StaticDecodedScale;
            const float along=(reverse?-local[grain]:local[grain])+lengths[grain]*.5f;
            v.v=(phase+along/grainMeters)/WorkshopVisual::StaticDecodedScale;
            if(!std::isfinite(v.u)||!std::isfinite(v.v))throw std::runtime_error("BRIDGE_TIMBER_NONFINITE_UV");
        }
    }
    if(out.ranges.size()!=Members||out.wood.vertices.size()!=Vertices||out.wood.indices.size()!=Indices||!out.wood.Valid())
        throw std::runtime_error("BRIDGE_TIMBER_COMPLETE_MEMBERSHIP");
    for(std::size_t i=0;i<out.wood.indices.size();i+=3) {
        const auto& a=out.wood.vertices[out.wood.indices[i]];
        const auto& b=out.wood.vertices[out.wood.indices[i+1]];
        const auto& d=out.wood.vertices[out.wood.indices[i+2]];
        const double area=(double(b.u)-a.u)*(double(d.v)-a.v)-(double(d.u)-a.u)*(double(b.v)-a.v);
        if(!std::isfinite(area)||area==0)throw std::runtime_error("BRIDGE_TIMBER_COLLAPSED_UV");
    }
    out.wood.InvalidateRaycastIndex();return out;
}
}
