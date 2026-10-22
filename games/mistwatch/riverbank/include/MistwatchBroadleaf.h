#pragma once
#include "MistwatchUnderstory.h"
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <string>
namespace Mistwatch {
// 已烘焙 tree-small-02 LOD2 的左手系根部與下層樹幹；測試核對頂點及跨高度面片截線。
inline constexpr Aevrynd::Vec3 BroadleafRoot{.0056121834f,-.0222652573f,.0014146231f};
inline Aevrynd::MeshBounds3D BroadleafBounds() {
    return {{.1414349675f,2.2739236355f,-.7517286539f},3.44f,{1.4504365921f,2.2739236355f,2.110558629f}};
}
inline Aevrynd::MeshBounds3D BroadleafLowerTrunkBounds() {
    return {{.201391527f,1.f,-.2247139015f},1.093f,{.285086570f,1.f,.3351969105f}};
}
inline std::string BroadleafMeshName(unsigned slot) {
    constexpr const char* names[]{"branches","leaves","trunk"};
    if(slot>=3)return {};
    return "tree-small-02-lod2-node0-mesh0-primitive"+std::to_string(slot)+"-material"+
        std::to_string(slot)+"-tree-small-02-"+names[slot]+".aemesh";
}
inline void RootBroadleafMesh(Aevrynd::Mesh3D& mesh) {
    for(auto& vertex:mesh.vertices){vertex.x-=BroadleafRoot.x;vertex.y-=BroadleafRoot.y;vertex.z-=BroadleafRoot.z;}
}
inline bool BroadleafClearsSettlement(const Aevrynd::MeshBounds3D& crown) {
    for(const auto p:{Session::fuse,Session::generator,Session::rangerLog}) {
        if(crown.center.z-crown.extents.z<p.z+1.5f&&crown.center.z+crown.extents.z>p.z-1.5f&&
            crown.center.x-crown.extents.x<(std::max)(0.f,p.x)+2.f&&
            crown.center.x+crown.extents.x>(std::min)(0.f,p.x)-2.f)return false;
    }
    return true;
}
struct BroadleafTree {
    Aevrynd::Transform3D transform;
    Aevrynd::BoxCollider3D trunk;
};
inline std::vector<BroadleafTree> CreateBroadleafTrees(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> conifers,std::span<const ForestProp> props,const ForestUnderstory& understory,
    const EntryLayout& layout=ActiveEntryLayout) {
    using namespace Aevrynd;
    std::vector<BroadleafTree> result;
    std::uint32_t seed=0x42524f41u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 地形改變後仍以原根部、樹冠與碰撞門檻搜尋；延長有限搜尋而不降低標準。
    for(unsigned attempt=0;attempt<9600&&result.size()<12;++attempt) {
        const float x=-40.f+80.f*random(),z=-12.f+104.f*random();
        const float scale=.95f+.25f*random();
        Transform3D pose{{x,terrain.SampleHeight(x,z)-.045f,z},{0,random()*6.2831853f,0},{scale,scale,scale}};
        const auto crown=TransformBounds(BroadleafBounds(),pose);
        const auto stem=TransformBounds(BroadleafLowerTrunkBounds(),pose);
        const BoxCollider3D trunk{stem.center,stem.extents};
        // 整個樹冠腳印都避開任務與主支線；不以細樹幹碰撞掩蓋視線／通行問題。
        if(!PropClearsObjectives({crown.center,crown.extents},layout)||!BroadleafClearsSettlement(crown)||terrain.SampleNormal(x,z).y<.97f)continue;
        bool occupied=false;
        for(float dx:{-1.f,1.f})for(float dz:{-1.f,1.f}) {
            const float px=stem.center.x+dx*stem.extents.x,pz=stem.center.z+dz*stem.extents.z;
            if(!terrain.Contains(px,pz)||std::abs(terrain.SampleHeight(px,pz)-terrain.SampleHeight(x,z))>.1f)occupied=true;
        }
        for(const auto p:conifers)occupied|=std::hypot(x-p.x,z-p.z)<3.f;
        for(const auto& p:props)occupied|=std::abs(stem.center.x-p.collider.center.x)<stem.extents.x+p.collider.extents.x+.4f&&
            std::abs(stem.center.z-p.collider.center.z)<stem.extents.z+p.collider.extents.z+.4f;
        for(const auto& p:understory.saplings)occupied|=std::hypot(x-p.position.x,z-p.position.z)<1.5f;
        for(const auto& p:understory.ferns)occupied|=std::abs(stem.center.x-p.position.x)<stem.extents.x+.4f&&
            std::abs(stem.center.z-p.position.z)<stem.extents.z+.4f;
        for(const auto& p:result)occupied|=std::hypot(x-p.transform.position.x,z-p.transform.position.z)<5.f;
        if(!occupied)result.push_back({pose,trunk});
    }
    return result;
}
}
