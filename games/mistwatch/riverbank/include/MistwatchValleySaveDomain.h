#pragma once
#include "MistwatchValleyTerrain.h"
#include "MistwatchJointRockBank.h"
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <cstddef>
#include <cstring>

namespace Mistwatch {
// 存檔域綁定已重建的實際地形與兩個碰撞狀態；不是美術、全場景接觸或發布合格證。
class ValleySaveDomain {
    Aevrynd::HeightField3D terrain_;
    std::vector<Aevrynd::BoxCollider3D> before_,after_;
    std::uint64_t identity_{};
    std::vector<std::vector<std::byte>> roofHulls_;
    std::vector<Aevrynd::BoxCollider3D> roofCamera_;
    ValleySaveDomain(Aevrynd::HeightField3D field,std::vector<Aevrynd::BoxCollider3D> before,
        std::vector<Aevrynd::BoxCollider3D> after):terrain_(std::move(field)),before_(std::move(before)),after_(std::move(after)){
        identity_=WestCliffRelief::Fingerprint(terrain_);
        const auto add=[&](std::uint64_t v){for(unsigned i=0;i<8;++i){identity_^=(v>>(i*8))&255u;identity_*=1099511628211ull;}};
        add(0x564c593700000001ull);
        for(const auto* list:{&before_,&after_}){add(list->size());for(const auto& b:*list)
            for(const float v:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})add(std::bit_cast<std::uint32_t>(v));}
        if(!identity_)throw std::invalid_argument("VALLEY_SAVE_IDENTITY_ZERO");
    }
public:
    ValleySaveDomain(ValleySaveDomain&&)=default;
    ValleySaveDomain& operator=(ValleySaveDomain&&)=default;
    static ValleySaveDomain Create(const Aevrynd::HeightField3D& actual,std::span<const Aevrynd::BoxCollider3D> mission,
        std::span<const Aevrynd::BoxCollider3D> supports,std::span<const ValleyTerrain::SupportSurface> surfaces,
        std::span<const Aevrynd::BoxCollider3D> before,std::span<const Aevrynd::BoxCollider3D> after,bool terrace=false,bool jointBank=false){
        auto expected=ValleyTerrain::Build(CreateTerrain(),mission,supports,surfaces,terrace);
        if(jointBank)JointRockBank::Apply(expected,supports);
        if(!WestCliffRelief::SameExact(actual,expected.terrain))throw std::invalid_argument("VALLEY_SAVE_TERRAIN_IDENTITY");
        for(const auto list:{before,after}){
            if(list.empty()||list.size()>10000)throw std::invalid_argument("VALLEY_SAVE_COLLISION_SCOPE");
            for(const auto& b:list)if(!Session::Finite(b.center)||!Session::Finite(b.extents)||b.extents.x<=0||b.extents.y<=0||b.extents.z<=0)
                throw std::invalid_argument("VALLEY_SAVE_COLLISION_INVALID");
        }
        return ValleySaveDomain(std::move(expected.terrain),{before.begin(),before.end()},{after.begin(),after.end()});
    }
    std::uint64_t Identity()const noexcept{return identity_;}
    // 呼叫端先以正式屋頂建構器核對完整來源、75凸包與954相機盒。
    // 執行期快照只用於核對本程序內已準備凸包未變；含padding的位元組絕不進跨程序ID。
    void BindWorkshopRoof(std::uint64_t qualifiedMesh,
        std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,
        std::span<const Aevrynd::BoxCollider3D> camera){
        if(!roofHulls_.empty()||qualifiedMesh!=358593194902208623ull||hulls.size()!=75||camera.size()!=954)
            throw std::invalid_argument("VALLEY_SAVE_ROOF_SCOPE");
        std::vector<std::vector<std::byte>> snapshots; snapshots.reserve(hulls.size());
        auto next=identity_;
        const auto add=[&](std::uint64_t word){for(unsigned byte=0;byte<8;++byte){next^=(word>>(byte*8))&255u;next*=1099511628211ull;}};
        add(0x524f4f4600000001ull);add(qualifiedMesh);add(hulls.size());add(camera.size());
        for(const auto* hull:hulls){
            if(!hull||!hull->AxisCount())throw std::invalid_argument("VALLEY_SAVE_ROOF_HULL");
            snapshots.emplace_back(sizeof(*hull));std::memcpy(snapshots.back().data(),hull,sizeof(*hull));
        }
        for(const auto& b:camera){
            if(!Session::Finite(b.center)||!Session::Finite(b.extents)||b.extents.x<=0||b.extents.y<=0||b.extents.z<=0)
                throw std::invalid_argument("VALLEY_SAVE_ROOF_CAMERA");
            for(const float v:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})add(std::bit_cast<std::uint32_t>(v));
        }
        if(!next)throw std::invalid_argument("VALLEY_SAVE_ROOF_ZERO_ID");
        std::vector<Aevrynd::BoxCollider3D> boxes(camera.begin(),camera.end());
        roofHulls_=std::move(snapshots);roofCamera_=std::move(boxes);identity_=next;
    }
    bool MatchesAttachments(std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,
        std::span<const Aevrynd::BoxCollider3D> camera)const noexcept{
        if(hulls.size()!=roofHulls_.size()||camera.size()!=roofCamera_.size())return false;
        for(std::size_t i=0;i<hulls.size();++i)
            if(!hulls[i]||std::memcmp(roofHulls_[i].data(),hulls[i],sizeof(*hulls[i]))!=0)return false;
        for(std::size_t i=0;i<camera.size();++i)if(!FallenLogDetail::SameBox(camera[i],roofCamera_[i]))return false;
        return true;
    }
    bool MatchesTerrain(const Aevrynd::HeightField3D& field)const{return WestCliffRelief::SameExact(field,terrain_);}
    bool MatchesWorld(bool repaired,std::span<const Aevrynd::BoxCollider3D> boxes)const{
        const auto& expected=repaired?after_:before_;if(boxes.size()!=expected.size())return false;
        for(std::size_t i=0;i<boxes.size();++i)if(!FallenLogDetail::SameBox(boxes[i],expected[i]))return false;
        return true;
    }
};
}
