#pragma once
#include "MistwatchWorld.h"
#include <Aevrynd/Mesh3D.h>
#include <cmath>
#include <stdexcept>

namespace Mistwatch {
enum class RangerDeskPartKind { Top,Leg,Apron };
struct RangerDeskPart {
    RangerDeskPartKind kind{};
    std::size_t firstIndex{},indexCount{};
    Aevrynd::Vec3 low{},high{};
};
// 網格相對原桌collider.center；只接受原桌尺寸，不能把錯誤代理悄悄拉伸成新家具。
// 世界transform仍用原center，原碰撞／書本／任務座標皆不改。
inline Aevrynd::Mesh3D CreateRangerDesk(const Aevrynd::BoxCollider3D& collider,
    std::vector<RangerDeskPart>* parts=nullptr) {
    using namespace Aevrynd;
    if(!Session::Finite(collider.center)||!Session::Finite(collider.extents)||
        collider.extents.x!=.35f||collider.extents.y!=.5f||collider.extents.z!=.8f)
        throw std::invalid_argument("Ranger desk requires original finite collision proxy");
    Mesh3D mesh;
    std::vector<RangerDeskPart> authored;
    mesh.vertices.reserve(216);mesh.indices.reserve(324);authored.reserve(9);
    const auto box=Mesh3D::CreateUnitBox();
    const auto append=[&](Vec3 low,Vec3 high,RangerDeskPartKind kind) {
        const auto firstIndex=mesh.indices.size();
        const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
        const auto size=high-low;
        for(auto v:box.vertices) {
            const Vec3 unit{v.x,v.y,v.z};const auto normal=UnpackNormalSnorm8(v.normal);
            v.x=unit.x<0?low.x:high.x;v.y=unit.y<0?low.y:high.y;v.z=unit.z<0?low.z:high.z;
            // 使用原box完整normal／face color，不將碰撞proxy當可見整塊；UV給有限真長軸。
            const std::array lengths{size.x,size.y,size.z},coordinates{unit.x,unit.y,unit.z},
                normals{normal.x,normal.y,normal.z};
            unsigned plane=0;for(unsigned axis=1;axis<3;++axis)if(std::abs(normals[axis])>std::abs(normals[plane]))plane=axis;
            unsigned grain=plane==0?1u:0u;
            for(unsigned axis=0;axis<3;++axis)if(axis!=plane&&lengths[axis]>lengths[grain])grain=axis;
            const unsigned across=3u-plane-grain;
            Vec3 u{},w{};if(grain==0)u.x=1;else if(grain==1)u.y=1;else u.z=1;
            if(across==0)w.x=1;else if(across==1)w.y=1;else w.z=1;
            const float sign=Dot(Cross(u,w),{normal.x,normal.y,normal.z})<0?-1.f:1.f;
            v.u=(coordinates[grain]+.5f)*lengths[grain]*.5f;
            v.v=(coordinates[across]*sign+.5f)*lengths[across]*.5f;
            mesh.vertices.push_back(v);
        }
        for(const auto index:box.indices)mesh.indices.push_back(base+index);
        authored.push_back({kind,firstIndex,36,low,high});
    };
    const auto half=collider.extents;
    const float under=half.y-.06f,innerX=half.x-.08f,innerZ=half.z-.08f;
    append({-half.x,under,-half.z},{half.x,half.y,half.z},RangerDeskPartKind::Top);
    for(const float sideX:{-1.f,1.f})for(const float sideZ:{-1.f,1.f}) {
        const float lowX=sideX<0?-half.x:innerX,highX=sideX<0?-innerX:half.x;
        const float lowZ=sideZ<0?-half.z:innerZ,highZ=sideZ<0?-innerZ:half.z;
        append({lowX,-half.y,lowZ},{highX,under,highZ},RangerDeskPartKind::Leg);
    }
    // 四道140mm桌裙接到腿與桌板；只有上緣木作，中央與近地空間完整留空。
    for(const float side:{-1.f,1.f}) {
        const float low=side<0?-half.x:half.x-.04f,high=side<0?-half.x+.04f:half.x;
        append({low,under-.14f,-innerZ},{high,under,innerZ},RangerDeskPartKind::Apron);
    }
    for(const float side:{-1.f,1.f}) {
        const float low=side<0?-half.z:half.z-.04f,high=side<0?-half.z+.04f:half.z;
        append({-innerX,under-.14f,low},{innerX,under,high},RangerDeskPartKind::Apron);
    }
    if(parts)*parts=std::move(authored); // 完整成功後才提交parts，錯誤輸入不破壞呼叫者。
    return mesh;
}
inline Aevrynd::Mesh3D CreateRangerDesk(std::vector<RangerDeskPart>* parts=nullptr) {
    return CreateRangerDesk(RangerHutSolids()[2],parts);
}
}
