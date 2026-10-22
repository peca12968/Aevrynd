#pragma once
#include "MistwatchCanopyVolumeClearance.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <utility>
#include <vector>

namespace Mistwatch::GroupedCanopyVolume {
using Box=Aevrynd::BoxCollider3D;
using Group=std::span<const Box>;
inline constexpr std::size_t MaximumGroups=12,MaximumGroupParts=10000,MaximumTotalParts=120000;
struct Expected {
    std::size_t groupCount{},totalParts{};
    std::uint64_t fingerprint{};
    bool operator==(const Expected&)const=default;
};
namespace Detail {
[[noreturn]] inline void Fail(const char* reason){
    throw std::invalid_argument(std::string("GROUPED_CANOPY_VOLUME_")+reason);
}
struct Hash {
    std::uint64_t value{14695981039346656037ull};
    void Word(std::uint32_t word)noexcept{for(unsigned i=0;i<4;++i){value^=(word>>(8*i))&255u;value*=1099511628211ull;}}
    void Wide(std::uint64_t word)noexcept{Word(static_cast<std::uint32_t>(word));Word(static_cast<std::uint32_t>(word>>32));}
};
}
// 僅衡量輸入；呼叫端必須另用固定版本的獨立證據提供 Expected，不能將這份結果當成資格證明。
[[nodiscard]] inline Expected Measure(std::span<const Group> groups){
    if(groups.empty()||groups.size()>MaximumGroups)Detail::Fail("GROUP_COUNT");
    std::size_t total{};
    // 先檢查整份有界形狀，再逐組完整呼叫舊驗證器；此階段沒有植株或提早碰撞返回。
    for(const auto group:groups){
        if(group.empty()||group.size()>MaximumGroupParts)Detail::Fail("PART_COUNT");
        if(group.size()>MaximumTotalParts-total)Detail::Fail("TOTAL_COUNT");
        total+=group.size();
    }
    for(const auto group:groups)if(!CanopyVolume::ValidWoodParts(group))Detail::Fail("INVALID_BOX");
    Detail::Hash hash;
    hash.Word(0x47574356u);hash.Word(1);hash.Wide(groups.size());
    for(std::size_t index=0;index<groups.size();++index){
        hash.Word(0x47525042u);hash.Wide(index);hash.Wide(groups[index].size());
        for(const auto& box:groups[index])for(float v:{box.center.x,box.center.y,box.center.z,
            box.extents.x,box.extents.y,box.extents.z})hash.Word(std::bit_cast<std::uint32_t>(v));
    }
    hash.Word(0x454e4447u);hash.Wide(total);
    return {groups.size(),total,hash.value};
}
// 自有副本切斷呼叫端可變 span 的壽命／更動風險。建構成功才允許執行碰撞查詢。
// 未提供可省略的 Expected 或「>=某數量」捷徑；不同作者版本由呼叫端明確固定。
class ValidatedWood final {
public:
    explicit ValidatedWood(std::span<const Group> groups,const Expected& expected){
        const auto measured=Measure(groups);
        if(expected.groupCount!=measured.groupCount)Detail::Fail("EXPECTED_GROUP_COUNT");
        if(expected.totalParts!=measured.totalParts)Detail::Fail("EXPECTED_TOTAL_COUNT");
        if(expected.fingerprint!=measured.fingerprint)Detail::Fail("EXPECTED_FINGERPRINT");
        groups_.reserve(groups.size());for(const auto group:groups)groups_.emplace_back(group.begin(),group.end());
        identity_=measured;
    }
    ValidatedWood(const ValidatedWood&)=default;
    ValidatedWood& operator=(const ValidatedWood& other){
        // 複製配置失敗時仍保留原身分與原木件，不產生半份證據。
        if(this!=&other){ValidatedWood copy(other);*this=std::move(copy);}return *this;
    }
    // 移動後來源明確失效；不能將空的舊容器誤當成「整個世界都無碰撞」。
    ValidatedWood(ValidatedWood&& other)noexcept
        :identity_(std::exchange(other.identity_,Expected{})),groups_(std::move(other.groups_)){}
    ValidatedWood& operator=(ValidatedWood&& other)noexcept{
        if(this!=&other){identity_=std::exchange(other.identity_,Expected{});groups_=std::move(other.groups_);}return *this;
    }
    [[nodiscard]] const Expected& Identity()const noexcept{return identity_;}
    [[nodiscard]] std::span<const std::vector<Box>> Groups()const noexcept{return groups_;}
    [[nodiscard]] bool Clears(const CanopyVolume::PlantVolume& plant,float touchMargin=.03f)const noexcept{
        if(identity_.groupCount==0||groups_.size()!=identity_.groupCount)return false;
        // 即使未碰到任何木件，植物／margin 仍須符合原介面完整條件。
        if(!CanopyVolume::ClearsCanopyWood(plant,{},touchMargin))return false;
        for(const auto& group:groups_)if(!CanopyVolume::ClearsCanopyWood(plant,group,touchMargin))return false;
        return true;
    }
    template<class Pose>[[nodiscard]] bool Clears(const Pose& pose,CanopyVolume::PlantLocalVolume local,float touchMargin=.03f)const noexcept{
        if(!CanopyVolume::ClearsCanopyWood(pose,local,{},touchMargin))return false;
        return Clears(CanopyVolume::PlantVolume{pose.position,local,pose.scale.x,pose.rotation},touchMargin);
    }
private:
    Expected identity_{};
    std::vector<std::vector<Box>> groups_;
};
}
