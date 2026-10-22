#pragma once
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchMiddleLayer.h"

namespace Mistwatch {
// 只使用目前作者留下的核心闊葉／中層樹；不從相機或種子另生落葉區。
struct DeciduousHabitatRoot {
    unsigned role{},sourceId{};
    Aevrynd::Transform3D pose;
    float radius{};
};
struct DeciduousHabitatPlan {
    std::vector<DeciduousHabitatRoot> roots;
    std::uint64_t fingerprint{};
};
namespace DeciduousHabitatDetail {
inline void Need(bool value,const char* gate){
    if(!value)throw std::invalid_argument(std::string("DECIDUOUS_HABITAT_FIRST_DIVERGENCE ")+gate);
}
inline std::uint64_t Identity(std::span<const DeciduousHabitatRoot> roots){
    Need(roots.size()==35,"actual-root-census");
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned i=0;i<35;++i){const auto& root=roots[i];const auto& p=root.pose;
        const unsigned role=i<12?0u:1u,ordinal=i<12?i:i-12;
        const unsigned source=role==0?ordinal:(ordinal<16?ordinal:ordinal+1);
        Need(root.role==role&&root.sourceId==source,"actual-source-role-order");
        Need(Session::Finite(p.position)&&Session::Finite(p.rotation)&&Session::Finite(p.scale)&&
            p.rotation.x==0&&p.rotation.z==0&&p.scale.x==p.scale.y&&p.scale.x==p.scale.z&&
            p.scale.x>=(role==0?.95f:.65f)&&p.scale.x<=(role==0?1.2f:.95f)&&
            std::abs(p.position.x)<=50.f&&p.position.z>=-20.f&&p.position.z<=100.f,
            "actual-root-pose-domain");
        Need(root.radius==BroadleafBounds().extents.z*p.scale.x,"authored-leaf-radius");
        ForestHabitatFloorDetail::Mix(hash,role);ForestHabitatFloorDetail::Mix(hash,source);
        for(float value:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,
            p.scale.x,p.scale.y,p.scale.z,root.radius})
            ForestHabitatFloorDetail::Mix(hash,std::bit_cast<std::uint32_t>(value));
    }
    return hash;
}
inline void Validate(const DeciduousHabitatPlan& plan){
    Need(plan.fingerprint!=0&&Identity(plan.roots)==plan.fingerprint,"root-plan-identity");
}
}
inline DeciduousHabitatPlan CreateDeciduousHabitatPlan(std::span<const BroadleafTree> broadleaf,
    std::span<const BroadleafTree> middle,std::span<const unsigned> middleSourceIds){
    DeciduousHabitatDetail::Need(broadleaf.size()==12&&middle.size()==23&&middleSourceIds.size()==23,
        "actual-root-census");
    DeciduousHabitatPlan result;result.roots.reserve(35);
    for(unsigned role=0;role<2;++role){const auto cohort=role==0?broadleaf:middle;
        for(unsigned i=0;i<cohort.size();++i){const auto& pose=cohort[i].transform;
            result.roots.push_back({role,role==0?i:middleSourceIds[i],pose,BroadleafBounds().extents.z*pose.scale.x});}}
    result.fingerprint=DeciduousHabitatDetail::Identity(result.roots);return result;
}
// 輸入是已編碼原 habitat 的真網格；事前核對全部資料，失敗不留下半套材質。
inline ForestHabitatFloorReceipt ApplyDeciduousHabitatFloor(Aevrynd::Mesh3D& mesh,
    const Aevrynd::HeightField3D& terrain,const DeciduousHabitatPlan& plan,bool landscape=false){
    DeciduousHabitatDetail::Validate(plan);
    DeciduousHabitatDetail::Need(mesh.Valid(),"actual-mesh-validity");
    const auto grid=terrain.Settings();
    for(unsigned z=0;z<grid.depth;++z)for(unsigned x=0;x<grid.width;++x)
        DeciduousHabitatDetail::Need(std::isfinite(terrain.Height(x,z)),"actual-terrain-finite");
    for(const auto& root:plan.roots){const auto p=root.pose.position;
        DeciduousHabitatDetail::Need(terrain.Contains(p.x,p.z)&&
            p.y==terrain.SampleHeight(p.x,p.z)-.045f,"actual-root-ground-source");}
    for(const auto& v:mesh.vertices)DeciduousHabitatDetail::Need(Session::Finite({v.x,v.y,v.z})&&
        std::abs(v.x)<=1000.f&&std::abs(v.z)<=1000.f&&std::isfinite(v.u)&&std::isfinite(v.v)&&
        v.u>=0&&v.v>=0&&v.u<=1&&v.v<=1&&v.u+v.v<=1.000001f,"actual-mesh-layer-domain");
    ForestHabitatFloorReceipt receipt;
    for(auto& v:mesh.vertices){
        // 地形法線沿用原銀行沉積語意；只有土層權重會改動。
        const auto normal=landscape?LandscapeNormal(terrain,v.x,v.z):terrain.SampleNormal(v.x,v.z);
        const float before=v.u;
        const float bank=ForestWoodBankSedimentWeight(v.x,v.z,normal,RiverSedimentWeight(v.x,v.z));
        float soil=before;
        if(!((v.z>32.f&&v.z<40.f)||std::abs(v.x)>=50.f||v.z<=-20.f||v.z>=100.f)){
            const float edge=ForestFloorSmooth((std::min)({50.f-std::abs(v.x),v.z+20.f,100.f-v.z})/5.f);
            for(const auto& root:plan.roots){const auto p=root.pose.position;
                const float distance=std::hypot(v.x-p.x,v.z-p.z);
                const float local=.85f*ForestFloorSmooth(1.f-(distance-.30f)/(root.radius-.30f));
                soil=(std::max)(soil,local*edge*(1.f-bank)*(1.f-v.v));}}
        v.u=soil;++receipt.vertices;
        receipt.changed+=std::bit_cast<std::uint32_t>(before)!=std::bit_cast<std::uint32_t>(soil);
        receipt.bridgePreserved+=v.z>32.f&&v.z<40.f;
        receipt.soilDominant+=soil>.5f;receipt.mossDominant+=soil+v.v<.5f;
        receipt.soilSum+=soil;receipt.rockSum+=v.v;
        for(float value:{v.x,v.y,v.z,v.u,v.v})ForestHabitatFloorDetail::Mix(receipt.fingerprint,std::bit_cast<std::uint32_t>(value));
        ForestHabitatFloorDetail::Mix(receipt.fingerprint,v.color);ForestHabitatFloorDetail::Mix(receipt.fingerprint,v.normal);
    }
    for(const auto index:mesh.indices)ForestHabitatFloorDetail::Mix(receipt.fingerprint,index);
    return receipt;
}
}
