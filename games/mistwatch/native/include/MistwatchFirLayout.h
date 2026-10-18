#pragma once
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/HeightField3D.h>
#include "MistwatchRiverProfile.h"
#include "MistwatchSession.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace Mistwatch {
using namespace Aevrynd;

enum class FirLayoutSelector : std::uint8_t { Legacy, ForestWood };

struct MatureFirRecord {
    Vec3 root{};
    Transform3D render{};
    BoxCollider3D collider{};
    unsigned patch{};
};

struct MatureFirLayout {
    std::vector<MatureFirRecord> firs;
    std::vector<Vec3> roots;
    std::vector<Vec3> patchCentres;
    std::uint64_t fingerprint{14695981039346656037ull};
    unsigned centreAttempts{}, plantAttempts{}, rejectedClearance{}, rejectedSpacing{};
    bool reachedBudget{};
};

inline BoxCollider3D MatureFirCollider(Vec3 root,float scale) {
    if(!Mistwatch::Session::Finite(root)||!std::isfinite(scale)||scale<=0)
        throw std::invalid_argument("Invalid mature fir collider input");
    const float ratio=scale/.65f;
    const float radius=.25f*ratio,halfHeight=2.f*ratio;
    return {root+Vec3{0,halfHeight,0},{radius,halfHeight,radius}};
}

inline MatureFirLayout CreateLegacyMatureFirLayout(std::span<const Vec3> roots) {
    MatureFirLayout result;
    result.roots.assign(roots.begin(),roots.end());
    result.firs.reserve(result.roots.size());
    for(const auto root:result.roots)result.firs.push_back({root,{root,{0,root.z,0},{.65f,.65f,.65f}},
        {root+Vec3{0,2,0},{.25f,2,.25f}},unsigned(result.firs.size())});
    result.reachedBudget=result.firs.size()==100;
    return result;
}

inline bool ClearsMissionRiverAndBranches(Vec3 root,float radius=0) {
    if(!Mistwatch::Session::Finite(root)||!std::isfinite(radius)||radius<0)return false;
    if(Mistwatch::RiverDistance(root.x,root.z)<Mistwatch::RiverOuterWidth(root.x)+2.f+radius)return false;
    for(const auto p:Mistwatch::Session::supplies)
        if(std::hypot(root.x-p.x,root.z-p.z)<3.f+radius)return false;
    for(const auto p:Mistwatch::Session::repairSites) {
        if(std::hypot(root.x-p.x,root.z-p.z)<4.f+radius)return false;
        if(std::abs(root.z-p.z)<1.5f+radius&&root.x>(std::min)(0.f,p.x)-1.f-radius&&
            root.x<(std::max)(0.f,p.x)+1.f+radius)return false;
    }
    for(const auto p:{Mistwatch::Session::fuse,Mistwatch::Session::generator,Mistwatch::Session::rangerLog})
        if(std::hypot(root.x-p.x,root.z-p.z)<5.f+radius)return false;
    for(const auto p:Mistwatch::Session::relaySites) {
        if(std::hypot(root.x-p.x,root.z-p.z)<4.f+radius)return false;
        if(std::abs(root.z-p.z)<1.8f+radius&&root.x>(std::min)(0.f,p.x)-1.f-radius&&
            root.x<(std::max)(0.f,p.x)+1.f+radius)return false;
    }
    return true;
}

inline bool ClearsLegacyWorld(Vec3 root,float radius=0) {
    return std::abs(root.x)>=4.f+radius&&ClearsMissionRiverAndBranches(root,radius);
}

// 舊 abs(x)<4 是直線美術空帶；wood-only 候選改守正式彎曲道路的真半寬、樹幹半徑與15cm餘裕。
// 這只是一個待 native 可玩/鏡頭驗證的幾何候選，不宣稱已證明遊玩安全。
inline bool ClearsForestWoodWorld(Vec3 root,float radius) {
    if(!ClearsMissionRiverAndBranches(root,radius))return false;
    const float centre=.35f*std::sin(root.z*.19f);
    const float halfWidth=2.8f+.3f*std::sin(root.z*.37f);
    return std::abs(root.x-centre)>=halfWidth+radius+.15f;
}

namespace Detail {
struct Random {
    std::uint32_t state;
    float Next(){state=state*1664525u+1013904223u;return float(state>>8)/16777216.f;}
};
inline void Mix(std::uint64_t& hash,std::uint32_t word) {
    for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}
}
inline void Mix(std::uint64_t& hash,float value){Mix(hash,std::bit_cast<std::uint32_t>(value));}
}

inline MatureFirLayout CreateForestWoodMatureFirLayout(const HeightField3D& terrain,
    unsigned target=100,std::uint32_t seed=0x57464952u) {
    if(target!=100||terrain.SampleCount()==0)throw std::invalid_argument("Prototype requires the real bounded 100-fir terrain");
    MatureFirLayout result;
    Detail::Random centreRandom{seed^0x43454e54u};
    constexpr unsigned TargetPatches=24;
    constexpr float StandRadius=2.45f;
    constexpr float ClearanceReserve=3.5f; // 涵蓋整群範圍內彎曲道路中心與寬度的變化。
    const auto tryCentre=[&](Vec3 root) {
        ++result.centreAttempts;
        root.y=terrain.SampleHeight(root.x,root.z);
        if(!terrain.Contains(root.x-ClearanceReserve,root.z-ClearanceReserve)||
            !terrain.Contains(root.x+ClearanceReserve,root.z+ClearanceReserve)||
            !ClearsForestWoodWorld(root,ClearanceReserve)){++result.rejectedClearance;return false;}
        for(const auto old:result.patchCentres)
            if(std::hypot(root.x-old.x,root.z-old.z)<10.f){++result.rejectedSpacing;return false;}
        result.patchCentres.push_back(root);return true;
    };
    // 四群近路配置讓 wood-only 收窄候選可被實際量測。
    // 群落中心仍在彎曲道路 envelope 外；每群朝內的一棵可以進入
    // 僅被舊 abs(x)<4 美術空帶排除的空間。
    for(unsigned attempt=0;attempt<64&&result.patchCentres.size()<4;++attempt) {
        const float z=-5.f+1.42f*float(attempt);
        const float centre=.35f*std::sin(z*.19f);
        const float halfWidth=2.8f+.3f*std::sin(z*.37f);
        const float side=(attempt&1u)?-1.f:1.f;
        tryCentre({centre+side*(halfWidth+ClearanceReserve+.18f),0,z});
    }
    for(unsigned attempt=0;attempt<24000&&result.patchCentres.size()<TargetPatches;++attempt) {
        Vec3 root{-36.f+72.f*centreRandom.Next(),0,-10.f+96.f*centreRandom.Next()};
        // 先保留完整群落 footprint，植栽便可確定生成，而非依賴重複隨機抽樣碰巧塞入。
        tryCentre(root);
    }
    if(result.patchCentres.size()!=TargetPatches)throw std::runtime_error("Prototype patch-centre budget incomplete");

    Detail::Random plantRandom{seed^0x504c414eu};
    result.firs.reserve(target);result.roots.reserve(target);
    constexpr float Pi=3.14159265358979323846f;
    for(unsigned patch=0;patch<TargetPatches;++patch) {
        const unsigned count=patch<4?5u:4u;
        const auto centre=result.patchCentres[patch];
        const float routeCentre=.35f*std::sin(centre.z*.19f);
        const float phase=patch<4?(centre.x>routeCentre?Pi:0.f):plantRandom.Next()*2.f*Pi;
        const float standRadius=(StandRadius-.26f)+.22f*plantRandom.Next();
        for(unsigned member=0;member<count;++member) {
            ++result.plantAttempts;
            const float angleJitter=.04f*(plantRandom.Next()-.5f);
            const float radiusJitter=.08f*(plantRandom.Next()-.5f);
            const float angle=phase+2.f*Pi*float(member)/float(count)+angleJitter;
            const float memberRadius=standRadius+radiusJitter;
            Vec3 root{centre.x+std::cos(angle)*memberRadius,0,centre.z+std::sin(angle)*memberRadius};
            const float scale=.55f+.27f*plantRandom.Next();
            const float yaw=plantRandom.Next()*2.f*Pi;
            const auto collider=MatureFirCollider(root,scale);
            const float radius=collider.extents.x;
            if(!terrain.Contains(root.x-radius,root.z-radius)||!terrain.Contains(root.x+radius,root.z+radius))
                throw std::runtime_error("Reserved patch escaped the terrain");
            root.y=terrain.SampleHeight(root.x,root.z);
            const auto groundedCollider=MatureFirCollider(root,scale);
            if(!ClearsForestWoodWorld(root,radius))
                throw std::runtime_error("Reserved patch violated world clearance");
            bool crowded=false;
            for(const auto& old:result.firs) {
                const float distanceToOld=std::hypot(root.x-old.root.x,root.z-old.root.z);
                const float minimum=old.patch==patch?2.35f:4.8f;
                crowded|=distanceToOld<minimum;
            }
            if(crowded)throw std::runtime_error("Reserved patches overlap");
            const Transform3D render{root,{0,yaw,0},{scale,scale,scale}};
            result.roots.push_back(root);
            result.firs.push_back({root,render,groundedCollider,patch});
        }
    }
    result.reachedBudget=result.firs.size()==target;
    for(const auto& fir:result.firs) {
        for(float value:{fir.root.x,fir.root.y,fir.root.z,fir.render.rotation.y,fir.render.scale.x,
            fir.collider.center.x,fir.collider.center.y,fir.collider.center.z,fir.collider.extents.x,
            fir.collider.extents.y,fir.collider.extents.z})Detail::Mix(result.fingerprint,value);
        Detail::Mix(result.fingerprint,fir.patch);
    }
    return result;
}

inline MatureFirLayout SelectMatureFirLayout(const HeightField3D& terrain,std::span<const Vec3> legacyRoots,
    FirLayoutSelector selector) {
    switch(selector) {
    case FirLayoutSelector::Legacy:return CreateLegacyMatureFirLayout(legacyRoots);
    case FirLayoutSelector::ForestWood:return CreateForestWoodMatureFirLayout(terrain);
    }
    throw std::invalid_argument("Invalid fir layout selector");
}
}
