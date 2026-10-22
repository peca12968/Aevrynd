#pragma once
#include "MistwatchWorld.h"
#include <bit>
#include <span>
#include <stdexcept>
#include <string_view>

namespace Mistwatch {
inline bool UseMiddleLayer(bool capture,std::wstring_view diagnosticFlag) {
    // 正常遊玩固定啟用；舊世界僅可在明確截圖比較中選取，不受玩家環境殘值影響。
    if(!capture||diagnosticFlag.empty())return true;
    if(diagnosticFlag==L"0")return false;
    if(diagnosticFlag==L"1")return true;
    throw std::invalid_argument("Invalid middle-layer comparison flag");
}
struct MiddleLayerResult {
    std::vector<Mistwatch::BroadleafTree> trees;
    unsigned attempted{},rejectedClearance{},rejectedTerrain{},rejectedSpacing{};
    bool reachedBudget{};
};
inline std::uint64_t MiddleLayerTransformFingerprint(std::span<const BroadleafTree> trees) {
    if(trees.empty())return 0;
    // 與作者CPU契約一致的matrix float-bit逐word雜湊；不把人口常數冒充位置證據。
    std::uint64_t hash=1469598103934665603ull;
    for(const auto& tree:trees) {
        const auto matrix=tree.transform.Matrix();
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column) {
            hash^=std::bit_cast<std::uint32_t>(matrix[row][column]);hash*=1099511628211ull;
        }
    }
    return hash;
}
inline MiddleLayerResult CreateMiddleLayer(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> conifers,std::span<const Mistwatch::ForestProp> props,
    const Mistwatch::ForestUnderstory& understory,std::span<const Mistwatch::BroadleafTree> original,
    std::span<const Aevrynd::BoxCollider3D> originalSolids,unsigned target=24,
    std::array<float,4> originalFernRadii={.981686473f,.821101248f,.549620509f,.635080874f},
    const EntryLayout& layout=ActiveEntryLayout) {
    using namespace Aevrynd;
    using namespace Mistwatch;
    if(target>96)throw std::invalid_argument("Middle-layer prototype target exceeds bounded budget");
    constexpr std::array<float,4> minimumRadii{.981686473f,.821101248f,.549620509f,.635080874f};
    for(unsigned i=0;i<4;++i)if(!std::isfinite(originalFernRadii[i])||originalFernRadii[i]<minimumRadii[i]-1e-6f||originalFernRadii[i]>4.f)
        throw std::invalid_argument("Middle-layer original fern footprint is invalid or undersized");
    MiddleLayerResult result;
    std::uint32_t seed=0x4d49444cu;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 根錨點與姿態各自固定種子，擴大搜尋不改寫既有候選的姿態前綴。
    std::uint32_t poseSeed=0x4d49504fu;
    const auto poseRandom=[&](){poseSeed=poseSeed*1664525u+1013904223u;return float(poseSeed>>8)/16777216.f;};
    std::vector<Vec3> anchors;
    // 世界路肩與兩側坡腳分層取樣，不依目前相機位置重生。
    for(float z=-10.f;z<=90.f;z+=5.f)for(float x:{8.f,12.f,17.f,23.f,30.f,37.f})for(float sign:{-1.f,1.f})
        anchors.push_back({x*sign+(random()-.5f)*2.f,0,z+(random()-.5f)*2.f});
    for(unsigned i=0;i<20000;++i)anchors.push_back({-40.f+80.f*random(),0,-12.f+104.f*random()});
    for(const auto anchor:anchors) {
        if(result.trees.size()==target)break;
        ++result.attempted;
        const float scale=.65f+.3f*poseRandom(),x=anchor.x,z=anchor.z;
        Transform3D pose{{x,terrain.SampleHeight(x,z)-.045f,z},{0,poseRandom()*6.2831853f,0},{scale,scale,scale}};
        const auto crown=TransformBounds(BroadleafBounds(),pose);
        const auto stem=TransformBounds(BroadleafLowerTrunkBounds(),pose);
        const BoxCollider3D trunk{stem.center,stem.extents};
        // 全冠保留原作者任務排除；細樹幹不能取代樹冠通路檢查。
        if(!PropClearsObjectives({crown.center,crown.extents},layout)||!BroadleafClearsSettlement(crown)) {
            ++result.rejectedClearance;continue;
        }
        bool unsuitable=!terrain.Contains(x,z)||terrain.SampleNormal(x,z).y<.97f;
        for(float dx:{-1.f,1.f})for(float dz:{-1.f,1.f}) {
            const float px=stem.center.x+dx*stem.extents.x,pz=stem.center.z+dz*stem.extents.z;
            unsuitable|=!terrain.Contains(px,pz)||std::abs(terrain.SampleHeight(px,pz)-terrain.SampleHeight(x,z))>.1f;
        }
        if(unsuitable){++result.rejectedTerrain;continue;}
        bool occupied=false;
        for(const auto p:conifers)occupied|=std::hypot(x-p.x,z-p.z)<3.f;
        for(const auto& p:props)occupied|=std::abs(stem.center.x-p.collider.center.x)<stem.extents.x+p.collider.extents.x+.4f&&
            std::abs(stem.center.z-p.collider.center.z)<stem.extents.z+p.collider.extents.z+.4f;
        for(const auto& p:understory.saplings)occupied|=std::hypot(x-p.position.x,z-p.position.z)<1.5f;
        // 原420蕨保留不重生，必須避開其真資產圓形腳印，不能以固定四十公分替代。
        for(std::size_t i=0;i<understory.ferns.size();++i) {
            const auto& p=understory.ferns[i];
            const float dx=(std::max)(0.f,std::abs(stem.center.x-p.position.x)-stem.extents.x);
            const float dz=(std::max)(0.f,std::abs(stem.center.z-p.position.z)-stem.extents.z);
            occupied|=std::hypot(dx,dz)<originalFernRadii[i%4]*p.scale.x+.1f;
        }
        for(const auto& p:original)occupied|=std::hypot(x-p.transform.position.x,z-p.transform.position.z)<5.f;
        for(const auto& p:result.trees)occupied|=std::hypot(x-p.transform.position.x,z-p.transform.position.z)<5.f;
        for(const auto& b:originalSolids)occupied|=std::abs(stem.center.x-b.center.x)<stem.extents.x+b.extents.x+.4f&&
            std::abs(stem.center.z-b.center.z)<stem.extents.z+b.extents.z+.4f;
        if(occupied){++result.rejectedSpacing;continue;}
        result.trees.push_back({pose,trunk});
    }
    result.reachedBudget=result.trees.size()==target;
    return result;
}
}
