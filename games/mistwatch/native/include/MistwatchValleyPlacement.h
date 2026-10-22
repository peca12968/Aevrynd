#pragma once
#include "MistwatchForestCover.h"
#include <string_view>
#include <ostream>
#include <string>
namespace Mistwatch::ValleyPlacement {
struct Plant {
    std::string_view group;std::size_t id{};Aevrynd::Transform3D* pose{};
    CanopyVolume::PlantLocalVolume local{};float* radius{};bool proceduralGrassPose{},originalFernCenterPolicy{};
};
struct Receipt {unsigned inspected{},inheritedFootprintFailures{},beforeFailures{},relocated{},proposals{},afterFailures{},centerPolicyInspected{},unqualifiedOriginalFernFootprints{};};
inline Receipt Repair(std::span<Plant> plants,const Aevrynd::HeightField3D& original,
    const Aevrynd::HeightField3D& terrain,std::span<const Aevrynd::BoxCollider3D> vegetation,
    std::span<const Aevrynd::BoxCollider3D> complete,std::ostream& log){
    Receipt r;ForestCoverSettings settings{};CanopyVolume::IndexedVolumeBounds volumes(complete);
    // 原420蕨類的作者契約僅有根中心坡度；草、混合蕨與灌木才使用完整腳印契約。
    // 不把前者冒稱完整接觸合格，也不將新增的草契約當作原始蕨類已通過的門。
    const auto fits=[&](const Plant& p,const Aevrynd::HeightField3D& field,Aevrynd::Vec3 root,float radius){
        return p.originalFernCenterPolicy?
            field.Contains(root.x,root.z)&&field.SampleNormal(root.x,root.z).y>=.88f&&
                std::abs(root.y-field.SampleHeight(root.x,root.z))<=1.e-5f:
            ForestCoverFootprint(field,root,radius,settings);
    };
    log.precision(9);log<<"group\tid\tbefore_x\tbefore_y\tbefore_z\tafter_x\tafter_y\tafter_z\tradius\tscale\tproposal\n";
    for(auto& p:plants){
        if(!p.pose||!p.radius||!CanopyVolume::Detail::ValidLocal(p.local)||!Session::Finite(p.pose->position)||
            !std::isfinite(*p.radius)||*p.radius<=0||p.pose->scale.x<=0||p.pose->scale.x!=p.pose->scale.y||p.pose->scale.x!=p.pose->scale.z)
            throw std::invalid_argument("VALLEY_PLANT_INPUT");
        ++r.inspected;auto oldRoot=p.pose->position;oldRoot.y=original.SampleHeight(oldRoot.x,oldRoot.z);
        r.inheritedFootprintFailures+=!ForestCoverFootprint(original,oldRoot,*p.radius,settings);
        r.centerPolicyInspected+=p.originalFernCenterPolicy;
        if(fits(p,terrain,p.pose->position,*p.radius))continue;
        ++r.beforeFailures;const auto before=*p.pose;bool found=false;
        // 同一有界最近鄰配置器，不做美術參數掃描；保留數量、來源ID和實際變體。
        for(unsigned attempt=0;attempt<192;++attempt){
            ++r.proposals;const unsigned ring=attempt/16+1,sector=attempt%16;
            const float distance=.25f*float(ring),angle=6.2831853f*float(sector)/16.f;
            auto pose=before;pose.position.x+=distance*std::cos(angle);pose.position.z+=distance*std::sin(angle);
            if(!terrain.Contains(pose.position.x,pose.position.z))continue;
            pose.position.y=terrain.SampleHeight(pose.position.x,pose.position.z);
            if(p.proceduralGrassPose){const float scale=ForestCoverScale(pose.position);
                pose.scale={scale,scale,scale};pose.rotation={0,pose.position.x+pose.position.z,0};}
            const float radius=p.local.radius*pose.scale.x;
            if(!fits(p,terrain,pose.position,radius)||
                !ForestCoverClearance(pose.position,radius,vegetation)||
                !volumes.Clears({pose.position,p.local,pose.scale.x,pose.rotation}))continue;
            bool crowded=false;
            for(const auto& other:plants)if(other.group==p.group&&other.pose!=p.pose&&
                std::hypot(pose.position.x-other.pose->position.x,pose.position.z-other.pose->position.z)<
                    .65f*(radius+*other.radius)){crowded=true;break;}
            if(crowded)continue;
            *p.pose=pose;*p.radius=radius;++r.relocated;found=true;
            log<<p.group<<'\t'<<p.id<<'\t'<<before.position.x<<'\t'<<before.position.y<<'\t'<<before.position.z
                <<'\t'<<pose.position.x<<'\t'<<pose.position.y<<'\t'<<pose.position.z<<'\t'<<radius<<'\t'<<pose.scale.x<<'\t'<<attempt<<'\n';
            break;
        }
        if(!found){++r.afterFailures;throw std::runtime_error("VALLEY_PLANT_NO_VALID_LOCAL_PLACEMENT:"+
            std::string(p.group)+":"+std::to_string(p.id)+":before="+std::to_string(r.beforeFailures)+":relocated="+std::to_string(r.relocated));}
    }
    for(const auto& p:plants){
        if(!fits(p,terrain,p.pose->position,*p.radius))++r.afterFailures;
        if(p.originalFernCenterPolicy&&!ForestCoverFootprint(terrain,p.pose->position,*p.radius,settings))++r.unqualifiedOriginalFernFootprints;
    }
    if(r.afterFailures)throw std::runtime_error("VALLEY_FINAL_FOOTPRINT_FAILURE");
    return r;
}
}
