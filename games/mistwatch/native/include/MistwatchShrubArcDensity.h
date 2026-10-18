#pragma once
#include "MistwatchShrubCanopy.h"
#include "MistwatchGroupedCanopyVolume.h"
#include "MistwatchShrubComposition.h"
#include <Aevrynd/FeasibleCircleArcs2D.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>

namespace Mistwatch {
namespace ShrubArcDensityDetail {
inline constexpr const char* Profile="woodland-feasible-arc-v3";
inline constexpr unsigned MaximumAttempts=24000,Columns=40,Rows=184,Bands=5,Sides=2;
inline constexpr unsigned CandidateCount=Columns*Rows*Sides;
inline std::uint32_t Hash(std::uint32_t value){value^=value>>16;value*=0x7feb352du;value^=value>>15;
    value*=0x846ca68bu;return value^(value>>16);}
inline float Unit(std::uint32_t id,std::uint32_t channel){return float(Hash(id^channel)>>8)/16777216.f;}
}
// 獨立圓弧候選與 trace；與原 gap-v2 API 並存，不取代舊演算法。
struct ShrubArcCandidate {ShrubPlant plant;std::uint32_t id{};bool accepted{};};
struct ShrubArcTrace {std::vector<ShrubArcCandidate> candidates;};
struct ShrubArcDiagnostics {
    unsigned prefixAttempts{},generated{},evaluated{},consideredAnchors{},domainSkipped{},computedAnchors{},truncatedAnchors{};
    std::size_t maximumNeighbors{};
    std::uint64_t operations{},neighborChecks{},geometryOperations{},proposalFingerprint{14695981039346656037ull};
};
inline thread_local ShrubArcDiagnostics LastShrubArcDiagnostics{};
namespace ShrubArcDetail {
namespace A=Aevrynd::FeasibleCircleArcs2D;
inline constexpr double Margin=.005;
struct Proposal {A::Candidate root;std::uint32_t anchorId{};unsigned variant{},lane{};float scale{},yaw{},radius{};};
inline void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline void Mix(std::uint64_t& h,std::uint64_t value){for(unsigned i=0;i<8;++i){h^=(value>>(i*8))&255u;h*=1099511628211ull;}}
struct Budget {
    ShrubArcDiagnostics& diagnostics;
    std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    void Time()const{Need(std::chrono::steady_clock::now()-start<std::chrono::minutes(10),"SHRUB_ARC_TIME_LIMIT");}
    void Op(){Need(diagnostics.operations<400000000,"SHRUB_ARC_OPERATION_LIMIT");if(!(++diagnostics.operations&255u))Time();}
    void Pair(){Need(diagnostics.neighborChecks<5000000,"SHRUB_ARC_PAIR_LIMIT");++diagnostics.neighborChecks;Op();}
};
inline bool InDomain(float x,float z){return std::isfinite(x)&&std::isfinite(z)&&std::abs(x)>=6&&std::abs(x)<28&&z>=-8&&z<84;}
inline bool CircleMayReachDomain(const A::Disc& a,double r){
    const double minX=std::nextafter(static_cast<float>(a.x-r),-std::numeric_limits<float>::infinity());
    const double maxX=std::nextafter(static_cast<float>(a.x+r),std::numeric_limits<float>::infinity());
    const double minZ=std::nextafter(static_cast<float>(a.z-r),-std::numeric_limits<float>::infinity());
    const double maxZ=std::nextafter(static_cast<float>(a.z+r),std::numeric_limits<float>::infinity());
    return maxZ>=-8&&minZ<84&&((maxX>=6&&minX<28)||(maxX>-28&&minX<=-6));
}
// 完整資料逐 bit 封印；不以雜湊或容器位址取代來源相等性。
using ShrubCompositionDetail::Same;
inline bool Same(const Aevrynd::BoxCollider3D& a,const Aevrynd::BoxCollider3D& b){
    return Same(a.center,b.center)&&Same(a.extents,b.extents);
}
inline bool Same(const ForestCoverPlant& a,const ForestCoverPlant& b){
    return Same(a.transform,b.transform)&&a.variant==b.variant&&a.patch==b.patch&&Same(a.radius,b.radius);
}
inline bool Same(const ForestCoverPatch& a,const ForestCoverPatch& b){
    return Same(a.centre,b.centre)&&Same(a.radius,b.radius)&&Same(a.aspect,b.aspect)&&Same(a.yaw,b.yaw)&&
        a.requested==b.requested&&a.accepted==b.accepted;
}
inline bool Same(const FernColonyPlant& a,const FernColonyPlant& b){
    return Same(a.transform,b.transform)&&a.variant==b.variant&&a.colony==b.colony&&
        a.grassPatch==b.grassPatch&&Same(a.radius,b.radius);
}
inline bool Same(const FernColony& a,const FernColony& b){
    return Same(a.centre,b.centre)&&Same(a.radius,b.radius)&&a.grassPatch==b.grassPatch&&
        a.requested==b.requested&&a.accepted==b.accepted;
}
inline bool Same(const FernColonyAsset& a,const FernColonyAsset& b){
    return Same(a.radius,b.radius)&&Same(a.height,b.height)&&Same(a.minimumY,b.minimumY)&&
        a.vertices==b.vertices&&a.triangles==b.triangles;
}
template<class T>inline bool SameRange(std::span<const T> a,std::span<const T> b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(!Same(a[i],b[i]))return false;
    return true;
}
inline bool Same(const ForestCoverResult& a,const ForestCoverResult& b){
    return a.rejectedClearance==b.rejectedClearance&&a.rejectedTerrain==b.rejectedTerrain&&
        a.rejectedSpacing==b.rejectedSpacing&&a.reachedBudget==b.reachedBudget&&
        SameRange<ForestCoverPlant>(a.plants,b.plants)&&SameRange<ForestCoverPatch>(a.patches,b.patches);
}
inline bool Same(const FernColonyResult& a,const FernColonyResult& b){
    return a.rejectedClearance==b.rejectedClearance&&a.rejectedTerrain==b.rejectedTerrain&&
        a.rejectedExistingFern==b.rejectedExistingFern&&a.rejectedGrass==b.rejectedGrass&&
        a.rejectedSpacing==b.rejectedSpacing&&a.reachedBudget==b.reachedBudget&&
        SameRange<FernColonyPlant>(a.plants,b.plants)&&SameRange<FernColony>(a.colonies,b.colonies);
}
inline bool Same(const Aevrynd::HeightField3D& a,const Aevrynd::HeightField3D& b){
    const auto& x=a.Settings();const auto& y=b.Settings();
    if(x.width!=y.width||x.depth!=y.depth||!Same(x.origin,y.origin)||!Same(x.cellSize,y.cellSize)||
        a.SampleCount()!=b.SampleCount()||!Same(a.MinimumHeight(),b.MinimumHeight())||!Same(a.MaximumHeight(),b.MaximumHeight()))return false;
    for(unsigned z=0;z<x.depth;++z)for(unsigned i=0;i<x.width;++i)if(!Same(a.Height(i,z),b.Height(i,z)))return false;
    return true;
}
inline bool Same(const GroupedCanopyVolume::ValidatedWood* a,const GroupedCanopyVolume::ValidatedWood* b){
    if(!a||!b)return a==b;
    if(a->Identity()!=b->Identity()||a->Groups().size()!=b->Groups().size())return false;
    for(std::size_t i=0;i<a->Groups().size();++i)
        if(!SameRange<Aevrynd::BoxCollider3D>(a->Groups()[i],b->Groups()[i]))return false;
    return true;
}
}
// 唯一工廠直接執行原 64 株演算法，絕不從 128 候選反推原前綴。
// 自有不可變來源快照與原結果；預設及移出後的 token 不能授權重用。
class ValidatedShrubArcPrefix final {
    struct State {
        Aevrynd::Mesh3D leaves;
        Aevrynd::HeightField3D terrain;
        std::vector<Aevrynd::BoxCollider3D> solids,wood;
        ForestUnderstory understory;
        ForestCoverResult grass;
        FernColonyResult colonies;
        std::array<FernColonyAsset,4> fernAssets;
        bool fullSolidVolumes{};
        std::shared_ptr<const GroupedCanopyVolume::ValidatedWood> groupedWood;
        ShrubCanopyResult original;
        State(const Aevrynd::Mesh3D& l,const Aevrynd::HeightField3D& t,std::span<const Aevrynd::BoxCollider3D> s,
            const ForestUnderstory& u,const ForestCoverResult& g,const FernColonyResult& c,
            const std::array<FernColonyAsset,4>& f,std::span<const Aevrynd::BoxCollider3D> w,bool full,
            const GroupedCanopyVolume::ValidatedWood* grouped,ShrubCanopyResult result)
            :leaves(l),terrain(t),solids(s.begin(),s.end()),wood(w.begin(),w.end()),understory(u),grass(g),
            colonies(c),fernAssets(f),fullSolidVolumes(full),
            groupedWood(grouped?std::make_shared<const GroupedCanopyVolume::ValidatedWood>(*grouped):nullptr),
            original(std::move(result)){}
    };
    std::shared_ptr<const State> state_;
public:
    ValidatedShrubArcPrefix()=default;
    [[nodiscard]] static ValidatedShrubArcPrefix CreateOriginal64(const Aevrynd::Mesh3D& leaves,
        const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
        const ForestUnderstory& understory,const ForestCoverResult& grass,const FernColonyResult& colonies,
        const std::array<FernColonyAsset,4>& fernAssets,std::span<const Aevrynd::BoxCollider3D> canopyWood={},
        bool fullSolidVolumes=false,const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr){
        auto original=CreateShrubCanopy(leaves,terrain,solids,understory,grass,colonies,fernAssets,
            64,canopyWood,fullSolidVolumes,nullptr,groupedWood);
        if(!original.reachedBudget||original.plants.size()!=64||original.attempted!=64+original.rejectedClearance+
            original.rejectedTerrain+original.rejectedVegetation||
            ShrubCompositionDetail::PlacementFingerprint(original)!=original.fingerprint)
            throw std::invalid_argument("SHRUB_ARC_PREFIX_ORIGINAL_64_REQUIRED");
        ValidatedShrubArcPrefix token;
        token.state_=std::make_shared<const State>(leaves,terrain,solids,understory,grass,colonies,fernAssets,
            canopyWood,fullSolidVolumes,groupedWood,std::move(original));
        return token;
    }
    [[nodiscard]] const ShrubCanopyResult& Original()const{
        if(!state_)throw std::invalid_argument("SHRUB_ARC_PREFIX_TOKEN_REQUIRED");
        return state_->original;
    }
    void Validate(const ShrubCanopyResult& original64,const Aevrynd::Mesh3D& leaves,
        const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
        const ForestUnderstory& understory,const ForestCoverResult& grass,const FernColonyResult& colonies,
        const std::array<FernColonyAsset,4>& fernAssets,std::span<const Aevrynd::BoxCollider3D> canopyWood={},
        bool fullSolidVolumes=false,const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr)const{
        const auto& original=Original();const auto& s=*state_;
        if(!ShrubCompositionDetail::Same(original,original64))
            throw std::invalid_argument("SHRUB_ARC_PREFIX_BITS_CHANGED");
        if(s.fullSolidVolumes!=fullSolidVolumes||!ShrubArcDetail::Same(s.leaves,leaves)||
            !ShrubArcDetail::Same(s.terrain,terrain)||!ShrubArcDetail::SameRange<Aevrynd::BoxCollider3D>(s.solids,solids)||
            !ShrubArcDetail::SameRange<Aevrynd::BoxCollider3D>(s.wood,canopyWood)||
            !ShrubArcDetail::SameRange<Aevrynd::Transform3D>(s.understory.ferns,understory.ferns)||
            !ShrubArcDetail::SameRange<Aevrynd::Transform3D>(s.understory.saplings,understory.saplings)||
            !ShrubArcDetail::Same(s.grass,grass)||!ShrubArcDetail::Same(s.colonies,colonies)||
            !ShrubArcDetail::SameRange<FernColonyAsset>(s.fernAssets,fernAssets)||
            !ShrubArcDetail::Same(s.groupedWood.get(),groupedWood))
            throw std::invalid_argument("SHRUB_ARC_PREFIX_INPUT_CHANGED");
    }
};
namespace ShrubArcDetail {
inline ShrubCanopyResult Continue(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::BoxCollider3D> solids,
    const ForestUnderstory& understory,const ForestCoverResult& grass,const FernColonyResult& colonies,
    const std::array<FernColonyAsset,4>& fernAssets,unsigned target,ShrubCanopyResult result,
    ShrubArcDiagnostics& diagnostics,std::span<const Aevrynd::BoxCollider3D> canopyWood,
    bool fullSolidVolumes,ShrubArcTrace* trace,const GroupedCanopyVolume::ValidatedWood* groupedWood){
    namespace D=ShrubArcDensityDetail;namespace C=ShrubArcDetail;namespace A=Aevrynd::FeasibleCircleArcs2D;
    diagnostics.prefixAttempts=result.attempted;
    if(target<=64)return result;
    const auto prefixCount=result.plants.size();result.reachedBudget=false;
    C::Budget budget{diagnostics};
    C::Need(result.attempted<=D::MaximumAttempts,"SHRUB_ARC_PREFIX_BUDGET");
    std::vector<A::Disc> discs;discs.reserve(understory.ferns.size()+colonies.plants.size()+grass.plants.size()+prefixCount);
    const auto add=[&](Aevrynd::Vec3 p,double weight){discs.push_back({p.x,p.z,weight,static_cast<std::uint32_t>(discs.size())});};
    for(std::size_t j=0;j<understory.ferns.size();++j){const auto& f=understory.ferns[j];
        add(f.position,double(fernAssets[j%4].radius*f.scale.x)+.03);}
    for(const auto& f:colonies.plants)add(f.transform.position,double(f.radius)+.03);
    for(const auto& g:grass.plants)add(g.transform.position,double(g.radius)+.03);
    for(const auto& p:result.plants)add(p.pose.position,double(p.radius)+.3);
    double maximumWeight=0;
    for(const auto& d:discs){budget.Op();A::Validate(d);maximumWeight=(std::max)(maximumWeight,d.radius);}
    std::sort(discs.begin(),discs.end(),[&](const A::Disc& a,const A::Disc& b){budget.Op();
        if(a.x!=b.x)return a.x<b.x;if(a.z!=b.z)return a.z<b.z;
        if(a.radius!=b.radius)return a.radius<b.radius;return a.id<b.id;});
    std::array<std::vector<std::size_t>,10> lanes;
    for(std::size_t i=0;i<discs.size();++i){budget.Op();const auto& a=discs[i];
        const unsigned band=static_cast<unsigned>((std::min)(4.0,(std::max)(0.0,std::floor((a.z+8)/18.4))));
        lanes[band*2+(a.x>=0?1u:0u)].push_back(i);}
    std::vector<C::Proposal> proposals;proposals.reserve(D::MaximumAttempts-diagnostics.prefixAttempts);
    for(std::size_t rank=0;proposals.size()+diagnostics.prefixAttempts<D::MaximumAttempts;++rank){bool any=false;
        for(unsigned lane=0;lane<10&&proposals.size()+diagnostics.prefixAttempts<D::MaximumAttempts;++lane){
            if(rank>=lanes[lane].size())continue;any=true;budget.Op();++diagnostics.consideredAnchors;
            const auto& anchor=discs[lanes[lane][rank]];
            const unsigned variant=D::Hash(anchor.id^0x56415232u)%4;
            const float scale=.85f+.25f*D::Unit(anchor.id,0x53434132u),yaw=6.2831853f*D::Unit(anchor.id,0x59415732u);
            const float radius=result.variants[variant].radius*scale;
            const double anchorRadius=anchor.radius+double(radius)+C::Margin;
            if(!C::CircleMayReachDomain(anchor,anchorRadius)){++diagnostics.domainSkipped;continue;}
            const double range=std::nextafter(anchorRadius+maximumWeight+double(radius)+C::Margin,std::numeric_limits<double>::infinity());
            const auto first=std::lower_bound(discs.begin(),discs.end(),anchor.x-range,[&](const A::Disc& d,double x){budget.Op();return d.x<x;});
            std::vector<A::Disc> neighbors;unsigned foundSelf=0;
            for(auto it=first;it!=discs.end();++it){budget.Op();if(it->x>anchor.x+range)break;budget.Pair();
                if(it->id==anchor.id){++foundSelf;continue;}
                const double reach=std::nextafter(anchorRadius+it->radius+double(radius)+C::Margin,std::numeric_limits<double>::infinity());
                if(std::hypot(it->x-anchor.x,it->z-anchor.z)<=reach)neighbors.push_back(*it);}
            C::Need(foundSelf==1,"SHRUB_ARC_EXACT_SELF_ID");
            diagnostics.maximumNeighbors=(std::max)(diagnostics.maximumNeighbors,neighbors.size());
            C::Need(neighbors.size()<=A::MaximumBlockers,"SHRUB_ARC_NEIGHBOR_LIMIT_NO_TRUNCATION");
            const unsigned remaining=D::MaximumAttempts-diagnostics.prefixAttempts-static_cast<unsigned>(proposals.size());
            const auto arcs=A::Compute(anchor,neighbors,{double(radius),C::Margin,remaining});
            ++diagnostics.computedAnchors;diagnostics.truncatedAnchors+=arcs.truncated?1u:0u;
            diagnostics.geometryOperations+=arcs.operations;for(unsigned op=0;op<arcs.operations;++op)budget.Op();
            C::Need(arcs.discChecks==neighbors.size()&&arcs.proposals==arcs.candidates.size()&&arcs.proposals<=remaining,"SHRUB_ARC_GEOMETRY_ACCOUNTING");
            for(const auto& point:arcs.candidates){budget.Op();proposals.push_back({point,anchor.id,variant,lane,scale,yaw,radius});
                C::Mix(diagnostics.proposalFingerprint,anchor.id);C::Mix(diagnostics.proposalFingerprint,variant);C::Mix(diagnostics.proposalFingerprint,point.arcIndex);
                C::Mix(diagnostics.proposalFingerprint,static_cast<unsigned>(point.kind));
                for(const double v:{point.x,point.z,point.angle})C::Mix(diagnostics.proposalFingerprint,std::bit_cast<std::uint64_t>(v));
                for(const float v:{scale,yaw,radius})C::Mix(diagnostics.proposalFingerprint,std::bit_cast<std::uint32_t>(v));}
        }
        if(!any)break;
    }
    diagnostics.generated=static_cast<unsigned>(proposals.size());
    C::Need(diagnostics.prefixAttempts+diagnostics.generated<=D::MaximumAttempts,"SHRUB_ARC_GENERATED_BUDGET");
    for(const auto& p:proposals){if(result.plants.size()==target)break;budget.Time();++result.attempted;++diagnostics.evaluated;
        const float x=static_cast<float>(p.root.x),z=static_cast<float>(p.root.z);
        if(!C::InDomain(x,z)){++result.rejectedClearance;continue;}
        const Aevrynd::Vec3 root{x,terrain.SampleHeight(x,z),z};const auto& author=result.variants[p.variant];
        C::Need(p.radius==author.radius*p.scale,"SHRUB_ARC_AUTHOR_RADIUS");
        if(!ForestCoverClearance(root,p.radius,solids)){++result.rejectedClearance;continue;}
        const CanopyVolume::PlantVolume volume{root,CanopyVolume::ConservativeLocalVolume(author.radius,author.minimumY,author.maximumY),p.scale,{}};
        if(fullSolidVolumes&&!solids.empty()&&!CanopyVolume::ClearsCanopyWood(volume,solids)){++result.rejectedClearance;continue;}
        if(!canopyWood.empty()&&!CanopyVolume::ClearsCanopyWood(volume,canopyWood)){++result.rejectedClearance;continue;}
        if(groupedWood&&!groupedWood->Clears(volume)){++result.rejectedClearance;continue;}
        if(!ForestCoverFootprint(terrain,root,p.radius,{})){++result.rejectedTerrain;continue;}
        bool occupied=false;
        for(std::size_t j=0;j<understory.ferns.size();++j){budget.Op();const auto& f=understory.ferns[j];
            occupied|=std::hypot(x-f.position.x,z-f.position.z)<p.radius+fernAssets[j%4].radius*f.scale.x+.03f;}
        for(const auto& f:colonies.plants){budget.Op();occupied|=std::hypot(x-f.transform.position.x,z-f.transform.position.z)<p.radius+f.radius+.03f;}
        for(const auto& g:grass.plants){budget.Op();occupied|=std::hypot(x-g.transform.position.x,z-g.transform.position.z)<p.radius+g.radius+.03f;}
        for(std::size_t j=0;j<prefixCount;++j){budget.Op();const auto& s=result.plants[j];
            occupied|=std::hypot(x-s.pose.position.x,z-s.pose.position.z)<p.radius+s.radius+.3f;}
        const bool staticSafe=!occupied;
        for(std::size_t j=prefixCount;j<result.plants.size();++j){budget.Op();const auto& s=result.plants[j];
            occupied|=std::hypot(x-s.pose.position.x,z-s.pose.position.z)<p.radius+s.radius+.3f;}
        const ShrubPlant plant{{root,{0,p.yaw,0},{p.scale,p.scale,p.scale}},p.variant,p.radius};
        if(trace&&staticSafe)trace->candidates.push_back({plant,diagnostics.evaluated-1,!occupied});
        if(occupied){++result.rejectedVegetation;continue;}
        result.plants.push_back(plant);
        for(const float value:{x,root.y,z,p.yaw,p.scale,p.radius}){result.fingerprint^=std::bit_cast<std::uint32_t>(value);result.fingerprint*=1099511628211ull;}
        result.fingerprint^=p.variant;result.fingerprint*=1099511628211ull;
    }
    result.reachedBudget=result.plants.size()==target;
    C::Need(result.attempted==diagnostics.prefixAttempts+diagnostics.evaluated&&diagnostics.evaluated<=diagnostics.generated&&
        result.attempted==result.plants.size()+result.rejectedClearance+result.rejectedTerrain+result.rejectedVegetation,"SHRUB_ARC_EVALUATED_ACCOUNTING");
    budget.Time();
    // 圓弧生成診斷；正式資格、原生畫面與發布狀態由呼叫端獨立驗證。
    std::cout<<"SHRUB_ARC_DIAGNOSTICS actual_algorithm="<<D::Profile<<" target="<<target<<" actual="<<result.plants.size()
        <<" generated="<<diagnostics.generated<<" evaluated="<<diagnostics.evaluated<<" prefix_attempts="<<diagnostics.prefixAttempts
        <<" generated_with_prefix="<<diagnostics.generated+diagnostics.prefixAttempts<<" evaluated_with_prefix="<<result.attempted
        <<" generated_unevaluated="<<diagnostics.generated-diagnostics.evaluated<<" proposal_fingerprint="<<diagnostics.proposalFingerprint
        <<" placement_fingerprint="<<result.fingerprint<<" neighbor_checks="<<diagnostics.neighborChecks
        <<" maximum_neighbors="<<diagnostics.maximumNeighbors<<" geometry_operations="<<diagnostics.geometryOperations
        <<" operations="<<diagnostics.operations<<'\n'<<std::flush;
    return result;
}
}
inline ShrubCanopyResult CreateShrubArcDensity(const Aevrynd::Mesh3D& leaves,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
    const ForestUnderstory& understory,const ForestCoverResult& grass,const FernColonyResult& colonies,
    const std::array<FernColonyAsset,4>& fernAssets,unsigned target=64,
    std::span<const Aevrynd::BoxCollider3D> canopyWood={},bool fullSolidVolumes=false,ShrubArcTrace* trace=nullptr,
    const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr){
    if(trace&&!trace->candidates.empty())throw std::invalid_argument("SHRUB_GAP_TRACE_NOT_EMPTY");
    LastShrubArcDiagnostics={};
    // 舊入口保持原驗證順序；非法 129、NaN 與尺寸不得由新分支截斷。
    const unsigned prefixTarget=target>128?target:(std::min)(target,64u);
    auto result=CreateShrubCanopy(leaves,terrain,solids,understory,grass,colonies,fernAssets,
        prefixTarget,canopyWood,fullSolidVolumes,nullptr,groupedWood);
    return ShrubArcDetail::Continue(terrain,solids,understory,grass,colonies,fernAssets,target,std::move(result),
        LastShrubArcDiagnostics,canopyWood,fullSolidVolumes,trace,groupedWood);
}
inline ShrubCanopyResult CreateShrubArcDensityFromValidatedPrefix(const ValidatedShrubArcPrefix& token,
    const ShrubCanopyResult& original64,const Aevrynd::Mesh3D& leaves,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
    const ForestUnderstory& understory,const ForestCoverResult& grass,const FernColonyResult& colonies,
    const std::array<FernColonyAsset,4>& fernAssets,unsigned target=128,
    std::span<const Aevrynd::BoxCollider3D> canopyWood={},bool fullSolidVolumes=false,ShrubArcTrace* trace=nullptr,
    const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr){
    if(trace&&!trace->candidates.empty())throw std::invalid_argument("SHRUB_GAP_TRACE_NOT_EMPTY");
    LastShrubArcDiagnostics={};
    token.Validate(original64,leaves,terrain,solids,understory,grass,colonies,fernAssets,
        canopyWood,fullSolidVolumes,groupedWood);
    if(target<64||target>128)throw std::invalid_argument("SHRUB_ARC_PREFIX_TARGET");
    return ShrubArcDetail::Continue(terrain,solids,understory,grass,colonies,fernAssets,target,original64,
        LastShrubArcDiagnostics,canopyWood,fullSolidVolumes,trace,groupedWood);
}
}
