#include "MistwatchGroupedCanopyVolume.h"
#include <Aevrynd/Mesh3D.h>
#include <iostream>
#include <limits>
#include <utility>

namespace G=::Mistwatch::GroupedCanopyVolume;
namespace V=::Mistwatch::CanopyVolume;
namespace {
unsigned checks{},negatives{};
void Need(bool okay,const char* reason){++checks;if(!okay)throw std::runtime_error(reason);}
template<class Action>void Reject(Action&& action,const char* expected){
    bool rejected=false;try{std::forward<Action>(action)();}catch(const std::invalid_argument& e){
        Need(std::string(e.what())==std::string("GROUPED_CANOPY_VOLUME_")+expected,"WRONG_REJECTION");rejected=true;}
    Need(rejected,"INVALID_INPUT_ACCEPTED");++negatives;
}
std::vector<G::Group> Views(const std::vector<std::vector<G::Box>>& data){
    std::vector<G::Group> out;for(const auto& group:data)out.emplace_back(group);return out;
}
// 獨立以明確 little-endian 位元串流計算，不呼叫 production Measure 或其 Hash。
G::Expected Oracle(std::span<const G::Group> groups){
    std::vector<unsigned char> bytes;
    const auto word=[&](std::uint64_t value,unsigned count){for(unsigned i=0;i<count;++i)bytes.push_back(static_cast<unsigned char>((value>>(i*8))&255));};
    word(0x47574356u,4);word(1,4);word(groups.size(),8);std::size_t total{};
    for(std::size_t i=0;i<groups.size();++i){word(0x47525042u,4);word(i,8);word(groups[i].size(),8);total+=groups[i].size();
        for(const auto& b:groups[i]){const std::array<float,6> values{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z};
            for(float f:values)word(std::bit_cast<std::uint32_t>(f),4);}}
    word(0x454e4447u,4);word(total,8);std::uint64_t hash=14695981039346656037ull;
    for(auto byte:bytes){hash^=byte;hash*=1099511628211ull;}
    return {groups.size(),total,hash};
}
}
int main(){try{
    const G::Box left{{-10,1,0},{.5f,1,.5f}},right{{10,1,0},{.5f,1,.5f}},back{{0,1,10},{.5f,1,.5f}};
    std::vector<std::vector<G::Box>> source{{left},{right,back}};auto groups=Views(source);const auto expected=Oracle(groups);
    Need(G::Measure(groups)==expected,"INDEPENDENT_FINGERPRINT");const G::ValidatedWood wood(groups,expected);
    Need(wood.Identity()==expected&&wood.Groups().size()==2&&wood.Groups()[1].size()==2,"ORDER_BOUNDARIES_STORED");
    const V::PlantVolume clear{{0,0,0},{.25f,0,2},1,{0,0,0}};
    Need(wood.Clears(clear),"LEGAL_MULTI_GROUP_CLEAR");auto last=clear;last.root={0,0,10};
    Need(!wood.Clears(last),"LAST_GROUP_COLLISION");auto first=clear;first.root={-10,0,0};Need(!wood.Clears(first),"FIRST_GROUP_COLLISION");
    // 比對兩組舊介面的邏輯交集，覆蓋相觸、margin、Y分離、yaw與尺度。
    for(float x:{-10.8f,-10.5f,-10.28f,-10.f,0.f,9.5f,10.f,10.8f})for(float y:{-3.f,0.f,3.f})
    for(float scale:{.5f,1.f,2.f})for(float margin:{0.f,.03f,.2f}){
        auto p=clear;p.root={x,y,0};p.uniformScale=scale;p.rotation.y=.7f;
        const bool reference=V::ClearsCanopyWood(p,groups[0],margin)&&V::ClearsCanopyWood(p,groups[1],margin);
        Need(wood.Clears(p,margin)==reference,"LEGACY_PREDICATE_EQUIVALENCE");}
    Aevrynd::Transform3D pose;pose.position={0,0,0};pose.scale={1,1,1};Need(wood.Clears(pose,clear.local),"POSE_OVERLOAD");
    pose.scale.y=2;Need(!wood.Clears(pose,clear.local),"NONUNIFORM_SCALE");++negatives;
    auto copy=wood;Need(copy.Identity()==expected&&copy.Clears(clear),"COPY_RETAINS_COMPLETE_GROUPS");
    auto moved=std::move(copy);Need(moved.Identity()==expected&&moved.Clears(clear)&&!copy.Clears(clear)&&copy.Identity().groupCount==0,"MOVE_SOURCE_INVALIDATED");
    auto assigned=wood;assigned=std::move(moved);
    Need(assigned.Identity()==expected&&assigned.Clears(clear)&&!moved.Clears(clear),"MOVE_ASSIGN_SOURCE_INVALIDATED");
    auto* same=&assigned;assigned=std::move(*same);Need(assigned.Clears(clear),"SELF_MOVE_PRESERVES_VALIDITY");
    auto copiedFromInvalid=copy;Need(!copiedFromInvalid.Clears(clear),"COPY_INVALID_REMAINS_INVALID");negatives+=3;
    auto invalid=clear;invalid.rotation.x=.01f;Need(!wood.Clears(invalid),"PITCH_REJECTED");++negatives;
    invalid=clear;invalid.local.radius=0;Need(!wood.Clears(invalid),"EMPTY_RADIUS_REJECTED");++negatives;
    Need(!wood.Clears(clear,-.001f)&&!wood.Clears(clear,std::numeric_limits<float>::quiet_NaN()),"INVALID_MARGIN");negatives+=2;
    invalid=clear;invalid.uniformScale=std::numeric_limits<float>::infinity();Need(!wood.Clears(invalid),"INFINITE_SCALE");++negatives;
    // 前組已碰撞也必須先驗完整輸入；最後組非法不能被碰撞 false 掩蓋。
    auto broken=source;broken.back().back().extents.y=0;auto brokenGroups=Views(broken);
    Reject([&]{const G::ValidatedWood v(brokenGroups,expected);(void)v.Clears(first);},"INVALID_BOX");
    broken=source;broken.back().back().center.z=std::numeric_limits<float>::quiet_NaN();brokenGroups=Views(broken);
    Reject([&]{(void)G::ValidatedWood(brokenGroups,expected);},"INVALID_BOX");
    broken=source;broken.back().back().extents.x=-1;brokenGroups=Views(broken);
    Reject([&]{(void)G::Measure(brokenGroups);},"INVALID_BOX");
    broken=source;broken.back().back().center.x=std::numeric_limits<float>::max();brokenGroups=Views(broken);
    Reject([&]{(void)G::Measure(brokenGroups);},"INVALID_BOX");
    broken=source;broken.back().back().extents.y=std::numeric_limits<float>::infinity();brokenGroups=Views(broken);
    Reject([&]{(void)G::Measure(brokenGroups);},"INVALID_BOX");
    broken=source;broken.back().clear();brokenGroups=Views(broken);Reject([&]{(void)G::Measure(brokenGroups);},"PART_COUNT");
    std::vector<G::Group> empty;Reject([&]{(void)G::Measure(empty);},"GROUP_COUNT");
    auto wrong=expected;++wrong.groupCount;Reject([&]{(void)G::ValidatedWood(groups,wrong);},"EXPECTED_GROUP_COUNT");
    wrong=expected;++wrong.totalParts;Reject([&]{(void)G::ValidatedWood(groups,wrong);},"EXPECTED_TOTAL_COUNT");
    wrong=expected;wrong.fingerprint^=1;Reject([&]{(void)G::ValidatedWood(groups,wrong);},"EXPECTED_FINGERPRINT");
    auto reordered=source;std::swap(reordered[0],reordered[1]);auto reorderedGroups=Views(reordered);
    Need(Oracle(reorderedGroups).fingerprint!=expected.fingerprint,"ORDER_HASH_DISTINCT");
    Reject([&]{(void)G::ValidatedWood(reorderedGroups,expected);},"EXPECTED_FINGERPRINT");
    std::vector<std::vector<G::Box>> rebounded{{left,right},{back}};auto boundaryGroups=Views(rebounded);
    Need(Oracle(boundaryGroups).fingerprint!=expected.fingerprint,"BOUNDARY_HASH_DISTINCT");
    Reject([&]{(void)G::ValidatedWood(boundaryGroups,expected);},"EXPECTED_FINGERPRINT");
    auto within=source;std::swap(within[1][0],within[1][1]);auto withinGroups=Views(within);
    Reject([&]{(void)G::ValidatedWood(withinGroups,expected);},"EXPECTED_FINGERPRINT");
    auto changed=source;changed[1][1].center.x=std::nextafter(changed[1][1].center.x,1.f);auto changedGroups=Views(changed);
    Reject([&]{(void)G::ValidatedWood(changedGroups,expected);},"EXPECTED_FINGERPRINT");
    // 外部容器改動不能污染已通過驗證的自有副本。
    source[0][0].center={0,1,0};source[1].clear();Need(wood.Identity()==expected&&wood.Clears(clear),"OWNED_IMMUTABLE_SNAPSHOT");
    std::vector<std::vector<G::Box>> large(G::MaximumGroups,std::vector<G::Box>(G::MaximumGroupParts,right));auto largeGroups=Views(large);
    const auto maximum=Oracle(largeGroups);Need(maximum.totalParts==120000&&G::Measure(largeGroups)==maximum,"EXACT_MAXIMUM");
    const G::ValidatedWood maximumWood(largeGroups,maximum);Need(maximumWood.Clears(clear),"ALL_MAXIMUM_GROUPS_CLEAR");
    large.back().back().center={0,1,0};largeGroups=Views(large);const G::ValidatedWood lastAtLimit(largeGroups,Oracle(largeGroups));
    Need(!lastAtLimit.Clears(clear),"LAST_OF_120000_COLLISION");
    large.back().back().extents.z=0;largeGroups=Views(large);
    Reject([&]{(void)G::ValidatedWood(largeGroups,maximum);},"INVALID_BOX");
    large.back().back()=right;large.back().push_back(right);largeGroups=Views(large);
    Reject([&]{(void)G::Measure(largeGroups);},"PART_COUNT");
    large.back().pop_back();large.push_back({right});largeGroups=Views(large);
    Reject([&]{(void)G::Measure(largeGroups);},"GROUP_COUNT");
    // 原 helper 仍拒絕單組10001；新介面不能以總容量取代此門。
    std::vector<G::Box> oversized(10001,right);Need(!V::ValidWoodParts(oversized)&&!V::ClearsCanopyWood(clear,oversized),"OLD_10000_GATE_UNCHANGED");
    std::cout<<"GROUPED_CANOPY_VOLUME_CPU_PASS checks="<<checks<<" negative_count="<<negatives<<" max_groups=12 max_group_parts=10000 max_total_parts=120000 fixture_fingerprint="
        <<expected.fingerprint<<" maximum_fingerprint="<<maximum.fingerprint<<" world_qualified=false native_passed=false\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
