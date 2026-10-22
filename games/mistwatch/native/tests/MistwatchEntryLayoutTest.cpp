#include "MistwatchEntryLayout.h"
#include "MistwatchSession.h"
#include <bit>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b) {
    return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
        std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&
        std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);
}
}
int main() {try {
    using namespace Mistwatch;
    static_assert(std::is_same_v<decltype(Session::supplies),const std::array<Aevrynd::Vec3,3>>);
    static_assert(std::is_same_v<decltype(Session::repairSites),const std::array<Aevrynd::Vec3,3>>);
    static_assert(OriginalEntryLayout.Site(EntrySiteId::Tools).x==-8);
    // 獨立抄錄修改前正式站點，不把作者輸出自填成預期結果。
    constexpr std::array<Aevrynd::Vec3,6> expected{{{4,0,6},{-5,0,14},{8,0,22},{-8,0,18},{8,0,-8},{-8,0,26}}};
    const auto& layout=ResolveEntryLayout(EntryLayoutId::Original);
    if(&layout!=&OriginalEntryLayout)return 1;
    for(unsigned i=0;i<expected.size();++i) {
        const auto& site=layout.Site(static_cast<EntrySiteId>(i));
        if(!Same(site,expected[i])||!Same(site,i<3?Session::supplies[i]:Session::repairSites[i-3]))return 2;
    }
    unsigned negatives{};
    for(std::uint32_t id:{0u,3u,4u,(std::numeric_limits<std::uint32_t>::max)()}) {
        try{(void)ResolveEntryLayout(static_cast<EntryLayoutId>(id));return 3;}
        catch(const std::invalid_argument&){++negatives;}
    }
    for(std::uint32_t id:{6u,7u,(std::numeric_limits<std::uint32_t>::max)()}) {
        try{(void)layout.Site(static_cast<EntrySiteId>(id));return 4;}
        catch(const std::invalid_argument&){++negatives;}
    }
    Session game;
    for(unsigned i=0;i<3;++i){game.feet=layout.Site(static_cast<EntrySiteId>(i));if(!game.Collect(i)||game.Collect(i))return 5;}
    if(game.CurrentStage()!=Stage::RepairMaterials)return 6;
    game.feet=layout.Site(EntrySiteId::Tools);
    if(game.CollectRepairMaterial(0,false)||!game.CollectRepairMaterial(0,true))return 7;
    for(unsigned i=1;i<3;++i){game.feet=layout.Site(static_cast<EntrySiteId>(i+3));if(!game.CollectRepairMaterial(i,false))return 8;}
    game.feet=Session::bridge;if(!game.RepairBridge())return 9;
    Session restored;const auto encoded=game.Serialize();
    if(!encoded.starts_with("MISTWATCH 7 ")||!restored.Restore(encoded)||restored.Serialize()!=encoded)return 10;
    const auto unchanged=restored.Serialize();
    if(restored.Restore("MISTWATCH 8 CAMP_V2")||restored.Serialize()!=unchanged)return 11;
    std::cout<<"ENTRY_LAYOUT_ORIGINAL_PASS sites=6 unknown_identity_negatives="<<negatives
        <<" public_array_types_preserved=1 pickup_tools_bridge_save=passed new_layout_enabled=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"ENTRY_LAYOUT_FIRST_DIVERGENCE "<<error.what()<<'\n';return 12;}}
