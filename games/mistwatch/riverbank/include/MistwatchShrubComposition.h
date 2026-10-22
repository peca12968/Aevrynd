#pragma once
#include "MistwatchShrubCanopy.h"
#include <memory>

namespace Mistwatch {
namespace ShrubCompositionDetail {
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
inline bool Same(const Aevrynd::Transform3D& a,const Aevrynd::Transform3D& b){
    return Same(a.position,b.position)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale);
}
inline bool Same(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b){
    if(a.vertices.size()!=b.vertices.size()||a.indices!=b.indices)return false;
    for(std::size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        if(!Same(x.x,y.x)||!Same(x.y,y.y)||!Same(x.z,y.z)||!Same(x.u,y.u)||!Same(x.v,y.v)||
            x.normal!=y.normal||x.color!=y.color)return false;}
    return true;
}
inline bool Same(const ShrubMesh& a,const ShrubMesh& b){
    if(!Same(a.stems,b.stems)||!Same(a.leaves,b.leaves)||!Same(a.radius,b.radius)||!Same(a.height,b.height)||
        !Same(a.minimumY,b.minimumY)||!Same(a.maximumY,b.maximumY)||a.leafCount!=b.leafCount||
        a.stemCount!=b.stemCount||a.attachments.size()!=b.attachments.size())return false;
    for(std::size_t i=0;i<a.attachments.size();++i){const auto& x=a.attachments[i];const auto& y=b.attachments[i];
        if(x.vertex!=y.vertex||!Same(x.centre,y.centre)||!Same(x.branchRadius,y.branchRadius))return false;}
    return true;
}
inline bool Same(const ShrubPlant& a,const ShrubPlant& b){return Same(a.pose,b.pose)&&a.variant==b.variant&&Same(a.radius,b.radius);}
inline bool Same(const ShrubCanopyResult& a,const ShrubCanopyResult& b){
    if(a.reachedBudget!=b.reachedBudget||a.fingerprint!=b.fingerprint||a.attempted!=b.attempted||
        a.rejectedClearance!=b.rejectedClearance||a.rejectedTerrain!=b.rejectedTerrain||
        a.rejectedVegetation!=b.rejectedVegetation||a.plants.size()!=b.plants.size())return false;
    for(unsigned i=0;i<4;++i)if(!Same(a.variants[i],b.variants[i]))return false;
    for(std::size_t i=0;i<a.plants.size();++i)if(!Same(a.plants[i],b.plants[i]))return false;
    return true;
}
inline std::uint64_t PlacementFingerprint(const ShrubCanopyResult& result){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& p:result.plants){
        if(!Session::Finite(p.pose.position)||!Session::Finite(p.pose.rotation)||!Session::Finite(p.pose.scale)||
            !std::isfinite(p.radius)||p.radius<=0||p.variant>=4)
            throw std::invalid_argument("SHRUB_COMPOSITION_NONFINITE_OR_INVALID_PLANT");
        for(float value:{p.pose.position.x,p.pose.position.y,p.pose.position.z,p.pose.rotation.y,p.pose.scale.x,p.radius}){
            hash^=std::bit_cast<std::uint32_t>(value);hash*=1099511628211ull;}
        hash^=p.variant;hash*=1099511628211ull;
    }
    return hash;
}
}

// 只有使用真實 64 株基準核對的工廠可授權 128；私有不可變快照防止事後替換任何欄位。
// 完整逐值 bit 比較而非僅靠雜湊，亦不把特定修橋狀態的數值寫死。
class ShrubCompositionContract {
public:
    ShrubCompositionContract()=default;
    static ShrubCompositionContract PreservedPrefix128(const ShrubCanopyResult& original64,const ShrubCanopyResult& candidate){
        if(!original64.reachedBudget||!candidate.reachedBudget||original64.plants.size()!=64||candidate.plants.size()!=128)
            throw std::invalid_argument("SHRUB_COMPOSITION_EXPECTED_64_128");
        for(unsigned i=0;i<4;++i)if(!ShrubCompositionDetail::Same(original64.variants[i],candidate.variants[i]))
            throw std::invalid_argument("SHRUB_COMPOSITION_VARIANT_BITS_CHANGED");
        for(unsigned i=0;i<64;++i)if(!ShrubCompositionDetail::Same(original64.plants[i],candidate.plants[i]))
            throw std::invalid_argument("SHRUB_COMPOSITION_PREFIX_BITS_CHANGED");
        if(ShrubCompositionDetail::PlacementFingerprint(original64)!=original64.fingerprint||
            ShrubCompositionDetail::PlacementFingerprint(candidate)!=candidate.fingerprint)
            throw std::invalid_argument("SHRUB_COMPOSITION_PLACEMENT_FINGERPRINT_MISMATCH");
        ShrubCompositionContract contract;
        contract.candidate_=std::make_shared<const ShrubCanopyResult>(candidate);
        return contract;
    }
    unsigned ExpectedCount()const noexcept{return candidate_?128u:64u;}
    void Validate(const ShrubCanopyResult& result)const{
        if(!result.reachedBudget||result.plants.size()!=ExpectedCount())
            throw std::invalid_argument("ROADSIDE_COMPLETE_EXISTING_VEGETATION_REQUIRED");
        if(candidate_&&!ShrubCompositionDetail::Same(*candidate_,result))
            throw std::invalid_argument("SHRUB_COMPOSITION_CANDIDATE_CHANGED");
    }
private:
    std::shared_ptr<const ShrubCanopyResult> candidate_;
};
}
