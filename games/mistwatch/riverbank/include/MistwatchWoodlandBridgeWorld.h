#pragma once
#include "MistwatchGroupedCanopyVolume.h"
#include <array>
#include <bit>

namespace Mistwatch::WoodlandBridgeWorld {
using Box=Aevrynd::BoxCollider3D;
inline constexpr std::size_t BeforeBodyCount=229,AfterBodyCount=230,BridgeIndex=33,WoodCount=12863;
// m2652：由原Q角色快照及run9完整來源box words獨立推導，不從新native畫面回填。
inline constexpr std::uint64_t BeforeBodyFingerprint=14223257952743745931ull,AfterBodyFingerprint=17309027362143902251ull;
inline constexpr GroupedCanopyVolume::Expected WoodIdentity{12,WoodCount,7162537194293152961ull};
namespace Detail {
[[noreturn]]inline void Fail(const char* reason){throw std::invalid_argument(std::string("WOODLAND_BRIDGE_WORLD_")+reason);}
inline auto Words(const Box& b){return std::array<std::uint32_t,6>{std::bit_cast<std::uint32_t>(b.center.x),
    std::bit_cast<std::uint32_t>(b.center.y),std::bit_cast<std::uint32_t>(b.center.z),std::bit_cast<std::uint32_t>(b.extents.x),
    std::bit_cast<std::uint32_t>(b.extents.y),std::bit_cast<std::uint32_t>(b.extents.z)};}
}
// 只量測有界body資料；ValidatedWorld仍須精確固定seal/兩橋差分/完整十二木件。
inline std::uint64_t BodyFingerprint(std::span<const Box> boxes,bool repaired){
    if(!CanopyVolume::ValidWoodParts(boxes))Detail::Fail("BODY_INVALID");
    GroupedCanopyVolume::Detail::Hash h;h.Word(0x57423242u);h.Word(1);h.Word(repaired?1u:0u);h.Wide(boxes.size());
    for(const auto& b:boxes)for(auto w:Detail::Words(b))h.Word(w);return h.value;
}
class ValidatedWorld final {
public:
    ValidatedWorld(std::span<const Box> before,std::span<const Box> after,const GroupedCanopyVolume::ValidatedWood& wood):wood_(wood){
        if(before.size()!=BeforeBodyCount||after.size()!=AfterBodyCount)Detail::Fail("BODY_COUNT");
        if(wood_.Identity()!=WoodIdentity)Detail::Fail("FIXED_WOOD_IDENTITY");
        if(BodyFingerprint(before,false)!=BeforeBodyFingerprint||BodyFingerprint(after,true)!=AfterBodyFingerprint)Detail::Fail("BODY_IDENTITY");
        constexpr std::array<std::uint32_t,6> bridge{{0u,3192704205u,1108344832u,1073741824u,1045220557u,1082130432u}};
        if(Detail::Words(after[BridgeIndex])!=bridge)Detail::Fail("BRIDGE_WORDS");
        for(std::size_t i=0;i<before.size();++i)if(Detail::Words(before[i])!=Detail::Words(after[i+(i>=BridgeIndex?1u:0u)]))Detail::Fail("BRIDGE_ORDER_DELTA");
        bodies_[0].assign(before.begin(),before.end());bodies_[1].assign(after.begin(),after.end());
    }
    ValidatedWorld(const ValidatedWorld&)=default;
    ValidatedWorld(ValidatedWorld&&)=default;
    ValidatedWorld& operator=(const ValidatedWorld& other){if(this!=&other){ValidatedWorld copy(other);*this=std::move(copy);}return *this;}
    ValidatedWorld& operator=(ValidatedWorld&& other)noexcept{if(this!=&other){bodies_=std::move(other.bodies_);wood_=std::move(other.wood_);}return *this;}
    void RequireValid()const {if(!Valid())Detail::Fail("INVALID_STATE");}
    [[nodiscard]]std::span<const Box> Bodies(bool repaired)const {RequireValid();return bodies_[repaired?1:0];}
    [[nodiscard]]const GroupedCanopyVolume::ValidatedWood& Wood()const {RequireValid();return wood_;}
    [[nodiscard]]bool Clears(const CanopyVolume::PlantVolume& plant,bool repaired,float margin=.03f)const noexcept{
        return Valid()&&CanopyVolume::ClearsCanopyWood(plant,bodies_[repaired?1:0],margin)&&wood_.Clears(plant,margin);
    }
    template<class Pose>[[nodiscard]]bool Clears(const Pose& pose,CanopyVolume::PlantLocalVolume local,bool repaired,float margin=.03f)const noexcept{
        return Valid()&&CanopyVolume::ClearsCanopyWood(pose,local,bodies_[repaired?1:0],margin)&&wood_.Clears(pose,local,margin);
    }
    [[nodiscard]]std::vector<Box> Flatten(bool repaired)const{
        RequireValid();auto out=bodies_[repaired?1:0];out.reserve(out.size()+WoodCount);
        for(const auto& group:wood_.Groups())out.insert(out.end(),group.begin(),group.end());return out;
    }
private:
    [[nodiscard]]bool Valid()const noexcept{return bodies_[0].size()==BeforeBodyCount&&bodies_[1].size()==AfterBodyCount&&wood_.Identity()==WoodIdentity;}
    std::array<std::vector<Box>,2> bodies_;
    GroupedCanopyVolume::ValidatedWood wood_;
};
}
