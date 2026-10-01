#pragma once
#include "MistwatchSession.h"
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <vector>
namespace Mistwatch {
inline std::vector<Aevrynd::BoxCollider3D> RelaySolids(unsigned index) {
    if(index>=Session::relaySites.size())return {};
    const auto p=Session::relaySites[index];
    // 底座、桅杆與旋轉碟面的包圍體提供一致的角色及鏡頭碰撞。
    return {{p+Aevrynd::Vec3{0,.35f,0},{.65f,.35f,.65f}},
        {p+Aevrynd::Vec3{0,1.55f,0},{.12f,1.2f,.12f}},
        {p+Aevrynd::Vec3{0,2.15f,0},{.8f,.65f,.8f}}};
}
inline Aevrynd::Mesh3D CreateRelayDish() {
    using namespace Aevrynd;
    Mesh3D mesh;
    const auto vertex=[&](Vec3 p){
        const auto normal=Normalize(Vec3{-1.454545f*p.x,-1.454545f*p.y,1});
        mesh.vertices.push_back({p.x,p.y,p.z,PackNormalSnorm8(normal),PackColorRgba8({.72f,.76f,.73f,1}),0,0});
    };
    vertex({0,0,-.22f});
    for(unsigned ring=0;ring<4;++ring)for(unsigned slice=0;slice<16;++slice) {
        const float radius=.55f*float(ring+1)/4.f,angle=float(slice)*6.2831853f/16.f;
        vertex({radius*std::cos(angle),radius*std::sin(angle),-.22f+.22f*(radius/.55f)*(radius/.55f)});
    }
    for(unsigned slice=0;slice<16;++slice) {
        const unsigned next=(slice+1)%16;
        mesh.indices.insert(mesh.indices.end(),{0,1+slice,1+next});
        for(unsigned ring=1;ring<4;++ring) {
            const unsigned a=1+(ring-1)*16+slice,b=1+(ring-1)*16+next,c=1+ring*16+slice,d=1+ring*16+next;
            mesh.indices.insert(mesh.indices.end(),{a,c,d,a,d,b});
        }
    }
    return mesh;
}
}
