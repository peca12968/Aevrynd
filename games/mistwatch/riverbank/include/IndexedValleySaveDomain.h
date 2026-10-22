#pragma once
#include "MistwatchValleySaveDomain.h"
#include <Aevrynd/StaticAabbIndex3D.h>
#include <Aevrynd/Camera.h>
#include <memory>

namespace Mistwatch {
// 完整世界與每個消費者的活動清單分開；舊一萬筆ValleySaveDomain契約不變。
class IndexedValleySaveDomain final {
    Aevrynd::HeightField3D terrain_;
    std::array<std::vector<Aevrynd::BoxCollider3D>,2> worlds_;
    std::array<std::unique_ptr<Aevrynd::StaticAabbIndex3D>,2> indexes_;
    std::vector<std::vector<std::byte>> roof_;
    std::vector<Aevrynd::BoxCollider3D> camera_;
    std::uint64_t identity_{};
    double lowY_{},highY_{};
    #include "IndexedQueryRegions.inl"
public:
    IndexedValleySaveDomain(const ValleySaveDomain& qualified,const Aevrynd::HeightField3D& field,
        std::span<const Aevrynd::BoxCollider3D> oldBefore,std::span<const Aevrynd::BoxCollider3D> oldAfter,
        std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> roof,std::span<const Aevrynd::BoxCollider3D> camera):IndexedValleySaveDomain(qualified,field,field,oldBefore,oldAfter,before,after,roof,camera){}
    IndexedValleySaveDomain(const ValleySaveDomain& qualified,const Aevrynd::HeightField3D& legacy,const Aevrynd::HeightField3D& field,
        std::span<const Aevrynd::BoxCollider3D> oldBefore,std::span<const Aevrynd::BoxCollider3D> oldAfter,
        std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> roof,std::span<const Aevrynd::BoxCollider3D> camera):terrain_(field){
        if(!qualified.MatchesTerrain(legacy)||!qualified.MatchesWorld(false,oldBefore)||!qualified.MatchesWorld(true,oldAfter)||
            !qualified.MatchesAttachments(roof,camera))throw std::invalid_argument("INDEXED_VALLEY_QUALIFIED_BASE_MISMATCH");
        identity_=qualified.Identity();
        const auto add=[&](std::uint64_t word){for(unsigned i=0;i<8;++i){identity_^=(word>>(i*8))&255u;identity_*=1099511628211ull;}};
        if(!WestCliffRelief::SameExact(legacy,field)){add(0x5249564552420001ull);add(WestCliffRelief::Fingerprint(field));}
        add(0x49564c5900000001ull);
        for(unsigned state=0;state<2;++state){
            const auto input=state?after:before;
            if(input.empty()||input.size()>Aevrynd::StaticAabbIndex3D::MaximumBounds)throw std::invalid_argument("INDEXED_VALLEY_REGISTRATION_CAP");
            std::vector<Aevrynd::ClosedAabb3D> bounds;bounds.reserve(input.size());add(state);add(input.size());
            for(const auto& b:input){
                if(!Session::Finite(b.center)||!Session::Finite(b.extents)||b.extents.x<=0||b.extents.y<=0||b.extents.z<=0)
                    throw std::invalid_argument("INDEXED_VALLEY_INVALID_SOURCE_BOX");
                for(float f:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})add(std::bit_cast<std::uint32_t>(f));
                Aevrynd::ClosedAabb3D a{{double(b.center.x)-b.extents.x,double(b.center.y)-b.extents.y,double(b.center.z)-b.extents.z},
                    {double(b.center.x)+b.extents.x,double(b.center.y)+b.extents.y,double(b.center.z)+b.extents.z}};
                lowY_=(std::min)(lowY_,a.minimum[1]);highY_=(std::max)(highY_,a.maximum[1]);bounds.push_back(a);
            }
            worlds_[state].assign(input.begin(),input.end());indexes_[state]=std::make_unique<Aevrynd::StaticAabbIndex3D>(bounds);
            if(!indexes_[state]->Valid()||indexes_[state]->Size()!=input.size())throw std::invalid_argument("INDEXED_VALLEY_INVALID_SOURCE_INDEX");
        }
        if(!identity_||identity_==qualified.Identity())throw std::invalid_argument("INDEXED_VALLEY_IDENTITY_NOT_DISTINCT");
        for(const auto* hull:roof){roof_.emplace_back(sizeof(*hull));std::memcpy(roof_.back().data(),hull,sizeof(*hull));}
        camera_.assign(camera.begin(),camera.end());
    }
    std::uint64_t Identity()const noexcept{return identity_;}
    bool MatchesTerrain(const Aevrynd::HeightField3D& field)const{return WestCliffRelief::SameExact(field,terrain_);}
    bool MatchesAttachments(std::span<const Aevrynd::ConvexAabbSweep3D* const> roof,std::span<const Aevrynd::BoxCollider3D> camera)const noexcept {
        if(roof.size()!=roof_.size()||camera.size()!=camera_.size())return false;
        for(std::size_t i=0;i<roof.size();++i)if(!roof[i]||std::memcmp(roof[i],roof_[i].data(),sizeof(*roof[i]))!=0)return false;
        for(std::size_t i=0;i<camera.size();++i)if(!FallenLogDetail::SameBox(camera[i],camera_[i]))return false;
        return true;
    }
    std::span<const Aevrynd::BoxCollider3D> Registered(bool repaired)const noexcept{return worlds_[unsigned(repaired)];}
    #include "IndexedQueryPublic.inl"
};
}
