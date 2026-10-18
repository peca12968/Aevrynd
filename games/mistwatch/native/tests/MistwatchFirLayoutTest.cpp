#include "MistwatchWorld.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;

unsigned checks{};
void Require(bool value,std::string_view reason) {
    ++checks;
    if(!value)throw std::runtime_error(std::string(reason));
}
bool Exact(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Exact(const Transform3D& a,const Transform3D& b) {
    return Exact(a.position,b.position)&&Exact(a.rotation,b.rotation)&&Exact(a.scale,b.scale);
}
bool Exact(const BoxCollider3D& a,const BoxCollider3D& b) {
    return Exact(a.center,b.center)&&Exact(a.extents,b.extents);
}
bool Exact(const MatureFirRecord& a,const MatureFirRecord& b) {
    return Exact(a.root,b.root)&&Exact(a.render,b.render)&&Exact(a.collider,b.collider)&&a.patch==b.patch;
}
bool BodyOverlaps(Vec3 center,Vec3 extents,const BoxCollider3D& box) {
    const auto low=box.center-box.extents-extents,high=box.center+box.extents+extents;
    return center.x>low.x&&center.x<high.x&&center.y>low.y&&center.y<high.y&&
        center.z>low.z&&center.z<high.z;
}

void LegacyContract(const HeightField3D& terrain) {
    const auto roots=TreePositions();
    const auto layout=CreateMatureFirLayout(terrain,FirLayoutSelector::Legacy);
    Require(roots.size()==100&&layout.firs.size()==100&&layout.roots.size()==100&&layout.reachedBudget,
        "舊冷杉人口契約改變");
    Require(layout.patchCentres.empty()&&layout.fingerprint==14695981039346656037ull&&
        layout.centreAttempts==0&&layout.plantAttempts==0&&layout.rejectedClearance==0&&layout.rejectedSpacing==0,
        "舊冷杉 metadata 契約改變");
    const auto world=WorldSolids(false);
    for(std::size_t i=0;i<roots.size();++i) {
        const Transform3D render{roots[i],{0,roots[i].z,0},{.65f,.65f,.65f}};
        const BoxCollider3D collider{roots[i]+Vec3{0,2,0},{.25f,2,.25f}};
        Require(Exact(layout.roots[i],roots[i])&&Exact(layout.firs[i].root,roots[i])&&
            Exact(layout.firs[i].render,render)&&Exact(layout.firs[i].collider,collider)&&layout.firs[i].patch==i,
            "舊冷杉完整 record 不再逐位元相同");
        unsigned matches{};
        for(const auto& solid:world)matches+=Exact(solid,collider);
        Require(matches==1,"舊世界遺失或重複冷杉 collider");
    }
}

struct ForestMetrics {
    unsigned routes{},samples{},overlaps{},narrowedBand{};
    float closest{(std::numeric_limits<float>::infinity)()};
    float closestInterPatch{(std::numeric_limits<float>::infinity)()};
};
ForestMetrics ForestWoodContract(const HeightField3D& terrain) {
    const auto first=CreateMatureFirLayout(terrain,FirLayoutSelector::ForestWood);
    const auto second=CreateMatureFirLayout(terrain,FirLayoutSelector::ForestWood);
    Require(first.reachedBudget&&first.firs.size()==100&&first.roots.size()==100,
        "ForestWood 冷杉人口未達 100");
    Require(first.patchCentres.size()==24,"ForestWood 群落中心未達 24");
    Require(first.fingerprint==8236695966185867318ull&&second.fingerprint==first.fingerprint,
        "ForestWood 封存 fingerprint 改變");
    Require(first.firs.size()==second.firs.size()&&first.roots.size()==second.roots.size()&&
        first.patchCentres.size()==second.patchCentres.size(),"ForestWood 重跑大小不一致");
    for(std::size_t i=0;i<first.patchCentres.size();++i)
        Require(Exact(first.patchCentres[i],second.patchCentres[i]),"ForestWood 群落中心重跑不一致");

    ForestMetrics result;
    std::array<unsigned,4> scaleBins{},yawBins{};
    std::array<unsigned,24> patchCounts{};
    float minimumScale=(std::numeric_limits<float>::infinity)(),maximumScale{};
    for(std::size_t i=0;i<first.firs.size();++i) {
        const auto& fir=first.firs[i];
        Require(Exact(fir,second.firs[i])&&Exact(first.roots[i],second.roots[i]),
            "ForestWood 完整 record 重跑不一致");
        Require(Session::Finite(fir.root)&&Session::Finite(fir.render.rotation)&&Session::Finite(fir.render.scale),
            "ForestWood record 含非有限值");
        Require(Exact(fir.root,fir.render.position)&&Exact(fir.root,first.roots[i]),
            "ForestWood root/render 權威分裂");
        const float scale=fir.render.scale.x;
        Require(scale>=.55f&&scale<=.82f&&fir.render.scale.y==scale&&fir.render.scale.z==scale,
            "ForestWood scale 超出 .55 到 .82");
        Require(fir.render.rotation.x==0&&fir.render.rotation.z==0&&fir.render.rotation.y>=0&&
            fir.render.rotation.y<6.283185307179586f,"ForestWood yaw 範圍錯誤");
        Require(Exact(fir.collider,MatureFirCollider(fir.root,scale)),
            "ForestWood render/collider 未共用 record");
        Require(terrain.Contains(fir.root.x,fir.root.z)&&fir.root.y==terrain.SampleHeight(fir.root.x,fir.root.z),
            "ForestWood root 未貼合正式地形");
        Require(ClearsForestWoodWorld(fir.root,fir.collider.extents.x),
            "ForestWood 點清除契約改變");
        Require(fir.patch<patchCounts.size(),"ForestWood patch index 超界");
        ++patchCounts[fir.patch];
        result.narrowedBand+=std::abs(fir.root.x)<4.f+fir.collider.extents.x;
        minimumScale=(std::min)(minimumScale,scale);maximumScale=(std::max)(maximumScale,scale);
        ++scaleBins[(std::min)(unsigned((scale-.55f)/.0675f),3u)];
        ++yawBins[(std::min)(unsigned(fir.render.rotation.y/(6.283185307179586f/4.f)),3u)];
        for(std::size_t j=0;j<i;++j) {
            const float distance=std::hypot(fir.root.x-first.firs[j].root.x,fir.root.z-first.firs[j].root.z);
            result.closest=(std::min)(result.closest,distance);
            if(fir.patch!=first.firs[j].patch)result.closestInterPatch=(std::min)(result.closestInterPatch,distance);
            Require(distance>=(fir.patch==first.firs[j].patch?2.35f:4.8f),"ForestWood 間距契約改變");
        }
    }
    Require(minimumScale<.59f&&maximumScale>.78f,"ForestWood scale 分布未涵蓋兩端");
    for(const auto count:scaleBins)Require(count>=8,"ForestWood scale 分箱過疏");
    for(const auto count:yawBins)Require(count>=8,"ForestWood yaw 分箱過疏");
    for(const auto count:patchCounts)Require(count>=4&&count<=5,"ForestWood patch 人口超出 4 到 5");
    Require(result.narrowedBand>=3,"ForestWood 未實際縮窄舊直線美術空帶");

    // 以正式地形每 5cm 掃描玩家 0.45m 半徑、1.8m 高的完整身體。
    const auto route=[&](Vec3 from,Vec3 to) {
        ++result.routes;
        const auto steps=static_cast<unsigned>(std::ceil(std::hypot(to.x-from.x,to.z-from.z)/.05f));
        for(unsigned i=0;i<=steps;++i) {
            const float t=steps?float(i)/float(steps):0.f;
            const float x=from.x+(to.x-from.x)*t,z=from.z+(to.z-from.z)*t;
            const float feet=terrain.SampleHeight(x,z);++result.samples;
            for(const auto& fir:first.firs)
                result.overlaps+=BodyOverlaps({x,feet+.9f,z},{.45f,.9f,.45f},fir.collider);
        }
    };
    for(const auto p:Session::supplies)route({0,0,p.z},p);
    for(const auto p:Session::repairSites)route({0,0,p.z},p);
    route({0,0,Session::fuse.z},Session::fuse);
    route({0,0,Session::generator.z},Session::generator);
    route({0,0,Session::rangerLog.z},Session::rangerLog);
    for(const auto p:Session::relaySites)route({0,0,p.z},p);
    route({0,0,-10},{0,0,96});
    Require(result.routes==13&&result.samples==4673&&result.overlaps==0,
        "ForestWood 完整身體道路掃描改變");
    return result;
}

void InvalidSelectorContract(const HeightField3D& terrain) {
    const auto invalid=static_cast<FirLayoutSelector>(255);
    bool layoutRejected=false,worldRejected=false;
    try{(void)CreateMatureFirLayout(terrain,invalid);}catch(const std::invalid_argument&){layoutRejected=true;}
    try{(void)WorldSolids(false,invalid);}catch(const std::invalid_argument&){worldRejected=true;}
    Require(layoutRejected&&worldRejected,"未知冷杉 selector 未 fail closed");
}
}

int main() {try {
    const auto terrain=CreateTerrain();
    LegacyContract(terrain);
    const auto metrics=ForestWoodContract(terrain);
    InvalidSelectorContract(terrain);
    std::cout<<"MISTWATCH_FIR_LAYOUT_PASS checks="<<checks
        <<" legacy=100 forest=100 patches=24 fingerprint=8236695966185867318"
        <<" routes="<<metrics.routes<<" route_samples="<<metrics.samples
        <<" route_overlaps="<<metrics.overlaps<<" narrowed_legacy_band="<<metrics.narrowedBand
        <<" closest_pair="<<metrics.closest<<" closest_inter_patch="<<metrics.closestInterPatch<<'\n';
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';
    return 1;
}}
