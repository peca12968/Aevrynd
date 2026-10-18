#pragma once
#include "MistwatchShrubCanopy.h"
#include <algorithm>

namespace Mistwatch {
namespace ShrubGapDensityDetail {
inline constexpr const char* Profile="woodland-gap-density-v2";
inline constexpr unsigned MaximumAttempts=24000;
inline constexpr unsigned Columns=40,Rows=184,Bands=5,Sides=2;
inline constexpr unsigned CandidateCount=Columns*Rows*Sides;

inline std::uint32_t Hash(std::uint32_t value) {
    value^=value>>16;value*=0x7feb352du;
    value^=value>>15;value*=0x846ca68bu;
    return value^(value>>16);
}
inline float Unit(std::uint32_t id,std::uint32_t channel) {
    return float(Hash(id^channel)>>8)/16777216.f;
}
}

// 只旁記通過全部靜態安全與原前綴間距的候選，不參與選點決策。
struct ShrubGapCandidate {ShrubPlant plant;std::uint32_t id{};bool accepted{};};
struct ShrubGapTrace {std::vector<ShrubGapCandidate> candidates;};

// 舊64株完整保留；僅在尚餘搜尋額度內以固定分層候選補足缺口。
inline ShrubCanopyResult CreateShrubGapDensity(const Aevrynd::Mesh3D& leaves,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> solids,
    const ForestUnderstory& understory,const ForestCoverResult& grass,
    const FernColonyResult& colonies,const std::array<FernColonyAsset,4>& fernAssets,unsigned target=64,
    std::span<const Aevrynd::BoxCollider3D> canopyWood={},bool fullSolidVolumes=false,ShrubGapTrace* trace=nullptr) {
    using namespace Aevrynd;
    namespace D=ShrubGapDensityDetail;
    if(trace&&!trace->candidates.empty())throw std::invalid_argument("SHRUB_GAP_TRACE_NOT_EMPTY");
    // 非法配額也交由原入口驗證；不可先截成64而吞掉原本應拒絕的輸入。
    const unsigned prefixTarget=target>128?target:(std::min)(target,64u);
    auto result=CreateShrubCanopy(leaves,terrain,solids,understory,grass,colonies,fernAssets,
        prefixTarget,canopyWood,fullSolidVolumes);
    if(target<=64)return result;
    const auto prefixCount=result.plants.size();
    result.reachedBudget=false;
    const ForestCoverSettings footprint{};
    constexpr unsigned maximumBandRows=(D::Rows+D::Bands-1)/D::Bands;
    // 每輪依序探查左右兩側的五個縱向帶，避免把有限接受名額全用在單一端點。
    for(unsigned ordinal=0;ordinal<D::Columns*maximumBandRows&&
        result.attempted<D::MaximumAttempts&&result.plants.size()<target;++ordinal) {
        for(unsigned lane=0;lane<D::Sides*D::Bands&&
            result.attempted<D::MaximumAttempts&&result.plants.size()<target;++lane) {
            const unsigned side=lane%D::Sides,band=lane/D::Sides;
            const unsigned firstRow=band*D::Rows/D::Bands;
            const unsigned bandRows=(band+1)*D::Rows/D::Bands-firstRow;
            if(ordinal>=D::Columns*bandRows)continue;
            // 5與各帶的36或37列互質；17與40欄互質，故每格恰好提出一次。
            const unsigned row=firstRow+((ordinal/D::Columns)*5)%bandRows;
            const unsigned column=((ordinal%D::Columns)*17+row*13)%D::Columns;
            const std::uint32_t id=side*(D::Rows*D::Columns)+row*D::Columns+column;
            const float jitterX=.1f+.8f*D::Unit(id,0x58b81327u);
            const float jitterZ=.1f+.8f*D::Unit(id,0x5a7d239bu);
            const float x=(side?1.f:-1.f)*(6.f+22.f*(float(column)+jitterX)/float(D::Columns));
            const float z=-8.f+92.f*(float(row)+jitterZ)/float(D::Rows);
            const unsigned variant=D::Hash(id^0x56415232u)%4;
            const float scale=.85f+.25f*D::Unit(id,0x53434132u);
            const float yaw=6.2831853f*D::Unit(id,0x59415732u);
            ++result.attempted;
            const Vec3 root{x,terrain.SampleHeight(x,z),z};
            const auto& author=result.variants[variant];
            const float radius=author.radius*scale;
            if(!ForestCoverClearance(root,radius,solids)){++result.rejectedClearance;continue;}
            const CanopyVolume::PlantVolume volume{root,
                CanopyVolume::ConservativeLocalVolume(author.radius,author.minimumY,author.maximumY),scale,{}};
            if(fullSolidVolumes&&!solids.empty()&&!CanopyVolume::ClearsCanopyWood(volume,solids)) {
                ++result.rejectedClearance;continue;
            }
            if(!canopyWood.empty()&&!CanopyVolume::ClearsCanopyWood(volume,canopyWood)) {
                ++result.rejectedClearance;continue;
            }
            if(!ForestCoverFootprint(terrain,root,radius,footprint)){++result.rejectedTerrain;continue;}
            bool occupied=false;
            // 與原生成器相同的作者半徑、完整體積及草蕨間距；不搬動任何既有族群。
            for(std::size_t i=0;i<understory.ferns.size();++i) {
                const auto& f=understory.ferns[i];
                occupied|=std::hypot(x-f.position.x,z-f.position.z)<radius+fernAssets[i%4].radius*f.scale.x+.03f;
            }
            for(const auto& f:colonies.plants)
                occupied|=std::hypot(x-f.transform.position.x,z-f.transform.position.z)<radius+f.radius+.03f;
            for(const auto& g:grass.plants)
                occupied|=std::hypot(x-g.transform.position.x,z-g.transform.position.z)<radius+g.radius+.03f;
            for(std::size_t i=0;i<prefixCount;++i) {
                const auto& p=result.plants[i];
                occupied|=std::hypot(x-p.pose.position.x,z-p.pose.position.z)<radius+p.radius+.3f;
            }
            const bool staticSafe=!occupied;
            for(std::size_t i=prefixCount;i<result.plants.size();++i) {
                const auto& p=result.plants[i];
                occupied|=std::hypot(x-p.pose.position.x,z-p.pose.position.z)<radius+p.radius+.3f;
            }
            if(trace&&staticSafe)trace->candidates.push_back({{Transform3D{root,{0,yaw,0},{scale,scale,scale}},variant,radius},id,!occupied});
            if(occupied){++result.rejectedVegetation;continue;}
            const Transform3D pose{root,{0,yaw,0},{scale,scale,scale}};
            result.plants.push_back({pose,variant,radius});
            for(float value:{x,root.y,z,yaw,scale,radius}) {
                result.fingerprint^=std::bit_cast<std::uint32_t>(value);
                result.fingerprint*=1099511628211ull;
            }
            result.fingerprint^=variant;result.fingerprint*=1099511628211ull;
        }
    }
    result.reachedBudget=result.plants.size()==target;
    return result;
}
}
