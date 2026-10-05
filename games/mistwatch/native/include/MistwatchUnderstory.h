#pragma once
#include "MistwatchForestProps.h"
#include <vector>
namespace Mistwatch {
struct ForestUnderstory {
    std::vector<Aevrynd::Transform3D> ferns;
    std::vector<Aevrynd::Transform3D> saplings;
};
inline Aevrynd::BoxCollider3D SaplingCollider(const Aevrynd::Transform3D& tree) {
    const float radius=tree.scale.x*.25f/.65f;
    return {tree.position+Aevrynd::Vec3{0,1.5f,0},{radius,1.5f,radius}};
}
inline ForestUnderstory CreateUnderstory(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> trees,std::span<const ForestProp> props) {
    using namespace Aevrynd;
    ForestUnderstory result;
    std::uint32_t seed=0x4645524eu;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 加寬河岸後保留相同拒絕條件與亂數前綴，只增加有上限的找位機會。
    for(unsigned attempt=0;attempt<12000 && (result.ferns.size()<420 || result.saplings.size()<42);++attempt) {
        const bool sapling=attempt%6==0;
        if((sapling && result.saplings.size()>=42)||(!sapling && result.ferns.size()>=420))continue;
        const float x=-31.f+62.f*random(),z=-9.f+97.f*random();
        const float radius=sapling?1.4f:.9f;
        const float patch=.5f+.25f*std::sin(x*.35f+z*.17f)+.25f*std::cos(z*.28f-x*.13f);
        if(random()>patch || !PropClearsObjectives({{x,0,z},{radius,1,radius}}))continue;
        if(terrain.SampleNormal(x,z).y<.88f)continue;
        bool occupied=false;
        for(const auto tree:trees)occupied|=std::hypot(x-tree.x,z-tree.z)<(sapling?2.2f:1.f);
        for(const auto& prop:props)occupied|=std::abs(x-prop.collider.center.x)<prop.collider.extents.x+radius &&
            std::abs(z-prop.collider.center.z)<prop.collider.extents.z+radius;
        for(const auto& tree:result.saplings)occupied|=std::hypot(x-tree.position.x,z-tree.position.z)<(sapling?3.f:1.f);
        if(!sapling)for(const auto& fern:result.ferns)occupied|=std::hypot(x-fern.position.x,z-fern.position.z)<1.f;
        // 幼樹稍後產生時也要避開已存在的蕨類，重跑配置得到完全相同的腳印。
        if(sapling)for(const auto& fern:result.ferns)occupied|=std::hypot(x-fern.position.x,z-fern.position.z)<1.f;
        if(occupied)continue;
        const float scale=sapling?.23f+.13f*random():.6f+.18f*random();
        Transform3D plant{{x,terrain.SampleHeight(x,z),z},{0,random()*6.2831853f,0},{scale,scale,scale}};
        (sapling?result.saplings:result.ferns).push_back(plant);
    }
    return result;
}
// 遠處以縮小葉叢平順退場，根部固定；不在距離界線一次刪除完整尺寸植株。
inline float UnderstoryVisibility(float distance,float fullDistance=24.f,float endDistance=38.f) {
    if(!std::isfinite(distance)||distance<0 || !std::isfinite(fullDistance)||!std::isfinite(endDistance)||
        fullDistance<0 || endDistance<=fullDistance)return 0;
    const float t=std::clamp((endDistance-distance)/(endDistance-fullDistance),0.f,1.f);
    return t*t*(3.f-2.f*t);
}
}
