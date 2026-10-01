#pragma once
#include "MistwatchSession.h"
#include "MistwatchRiverProfile.h"
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <array>
#include <vector>
namespace Mistwatch {
struct ForestProp {
    unsigned mesh{};
    Aevrynd::Transform3D transform;
    Aevrynd::BoxCollider3D collider;
    bool FallenLog()const{return mesh==3;}
};
inline const std::array<Aevrynd::Mesh3D,4>& ForestPropMeshes() {
    static const std::array<Aevrynd::Mesh3D,4> meshes{{
        Aevrynd::CreateRockMesh3D(Aevrynd::EnvironmentMeshDetail3D::Near,19),
        Aevrynd::CreateRockMesh3D(Aevrynd::EnvironmentMeshDetail3D::Near,37),
        Aevrynd::CreateRockMesh3D(Aevrynd::EnvironmentMeshDetail3D::Near,71),
        Aevrynd::Mesh3D::CreateUnitCylinder(16)}};
    return meshes;
}
inline bool PropClearsObjectives(const Aevrynd::BoxCollider3D& b) {
    const float minX=b.center.x-b.extents.x,maxX=b.center.x+b.extents.x;
    const float minZ=b.center.z-b.extents.z,maxZ=b.center.z+b.extents.z;
    if(minX<4.5f && maxX> -4.5f)return false;
    if(minZ<43.f && maxZ>29.f)return false;
    // 保留舊橋頭禁放區，再沿新彎曲岸線排除整個地物腳印。
    for(unsigned sample=0;sample<5;++sample) {
        const float x=minX+(maxX-minX)*float(sample)*.25f;
        const float center=RiverCenterZ(x),margin=RiverOuterWidth(x)+2.f;
        if(minZ<center+margin && maxZ>center-margin)return false;
    }
    const auto clears=[&](Aevrynd::Vec3 p,float radius) {
        return std::hypot((std::max)(0.f,std::abs(p.x-b.center.x)-b.extents.x),
            (std::max)(0.f,std::abs(p.z-b.center.z)-b.extents.z))>=radius;
    };
    for(const auto p:Session::supplies) {
        if(!clears(p,3.f))return false;
        // 保留補給點通往中央主路的橫向支線。
        if(minZ<p.z+2.f && maxZ>p.z-2.f && minX<(std::max)(0.f,p.x)+2.f && maxX>(std::min)(0.f,p.x)-2.f)return false;
    }
    for(const auto p:Session::repairSites) {
        if(!clears(p,4.f))return false;
        if(minZ<p.z+1.5f && maxZ>p.z-1.5f && minX<(std::max)(0.f,p.x)+1.f && maxX>(std::min)(0.f,p.x)-1.f)return false;
    }
    return clears(Session::arena,7.f) && clears(Session::fuse,5.f) &&
        clears(Session::generator,5.f) && clears(Session::beacon,5.f);
}
inline std::vector<ForestProp> ForestProps(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> trees) {
    using namespace Aevrynd;
    std::vector<ForestProp> props;
    std::uint32_t seed=0x50524f50u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    const auto& meshes=ForestPropMeshes();
    unsigned rockCount{},logCount{};
    for(unsigned candidate=0;candidate<900 && props.size()<44;++candidate) {
        const bool log=candidate%5==0;
        // 各類配額分開，避免先生成的岩石占滿所有倒木名額。
        if((log && logCount>=12) || (!log && rockCount>=32))continue;
        const unsigned mesh=log?3:candidate%3;
        const float size=.9f+random()*1.8f,diameter=.4f+random()*.25f;
        Transform3D transform{{-30.f+60.f*random(),0,-8.f+94.f*random()},
            {log?1.5707963f:0.f,random()*6.2831853f,0},
            log?Vec3{diameter,3.5f+random()*2.f,diameter}:Vec3{size,size*(.7f+.4f*random()),size*(.8f+.5f*random())}};
        // 近路地標使用固定錨點，其餘依固定種子散布。
        if(candidate==0) {transform.position={-7.5f,0,7};transform.rotation={1.5707963f,0,0};}
        if(candidate==1)transform.position={-6,0,2};
        const auto initial=TransformBounds(meshes[mesh].Bounds(),transform);
        const float ground=terrain.SampleHeight(transform.position.x,transform.position.z);
        transform.position.y=ground-(initial.center.y-initial.extents.y)-(log?.05f:.12f);
        const auto bounds=TransformBounds(meshes[mesh].Bounds(),transform);
        BoxCollider3D collider{bounds.center,bounds.extents};
        if(!PropClearsObjectives(collider))continue;
        // 斜坡上的長倒木容易懸空，僅接受腳印高度差小於 15 公分的落點。
        float minimum=ground,maximum=ground;
        for(float x:{-1.f,1.f})for(float z:{-1.f,1.f}) {
            const float height=terrain.SampleHeight(collider.center.x+x*collider.extents.x,collider.center.z+z*collider.extents.z);
            minimum=(std::min)(minimum,height);maximum=(std::max)(maximum,height);
        }
        if(maximum-minimum>(log?.15f:.8f))continue;
        bool overlaps=false;
        for(const auto tree:trees)overlaps|=std::abs(tree.x-collider.center.x)<collider.extents.x+.6f &&
            std::abs(tree.z-collider.center.z)<collider.extents.z+.6f;
        for(const auto& other:props)overlaps|=std::abs(other.collider.center.x-collider.center.x)<other.collider.extents.x+collider.extents.x+.7f &&
            std::abs(other.collider.center.z-collider.center.z)<other.collider.extents.z+collider.extents.z+.7f;
        if(overlaps)continue;
        props.push_back({mesh,transform,collider});
        if(log)++logCount;else ++rockCount;
    }
    return props;
}
}
