#pragma once
#include <Aevrynd/Math.h>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch {
// 入口站點的穩定語意不隨位置或顯示順序改變；正式Game仍只啟用原關卡。
enum class EntryLayoutId : std::uint32_t { Original=1, WestSupplyPilot=2 };
enum class EntrySiteId : std::uint32_t { Supply0,Supply1,Supply2,Tools,Timber1,Timber2 };
struct EntryLayout {
    EntryLayoutId id;
    std::array<Aevrynd::Vec3,3> supplies;
    std::array<Aevrynd::Vec3,3> repairSites;

    constexpr const Aevrynd::Vec3& Site(EntrySiteId site) const {
        const auto index=static_cast<std::uint32_t>(site);
        if(index<3)return supplies[index];
        if(index<6)return repairSites[index-3];
        throw std::invalid_argument("Unknown entry site identity");
    }
};
// 唯一作者資料。既有公開Session陣列仍以相同型別、值與bit順序委派此處。
inline constexpr EntryLayout OriginalEntryLayout{
    EntryLayoutId::Original,
    {{{4,0,6},{-5,0,14},{8,0,22}}},
    {{{-8,0,18},{8,0,-8},{-8,0,26}}}
};
// 單一補給移至西側林緣，保留其他五站點。僅專用CPU候選目標可選，不是完整世界資格。
inline constexpr EntryLayout WestSupplyPilotLayout{
    EntryLayoutId::WestSupplyPilot,
    {{{-7,0,6},{-5,0,14},{8,0,22}}},
    OriginalEntryLayout.repairSites
};
inline constexpr const EntryLayout& ResolveEntryLayout(EntryLayoutId id) {
    if(id==EntryLayoutId::Original)return OriginalEntryLayout;
    if(id==EntryLayoutId::WestSupplyPilot)return WestSupplyPilotLayout;
    // 新layout須有明確作者、完整世界與存檔身份；不能以任意enum默認原場景。
    throw std::invalid_argument("Unknown entry layout identity");
}
// 只表示六個站點與語意的內容身份，不是完整地形/碰撞/資產世界的資格憑證。
inline constexpr std::uint64_t EntryLayoutFingerprint(const EntryLayout& layout) {
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){
        for(unsigned byte=0;byte<4;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}
    };
    word(static_cast<std::uint32_t>(layout.id));
    for(unsigned id=0;id<6;++id){
        word(id);const auto p=layout.Site(static_cast<EntrySiteId>(id));
        for(float value:{p.x,p.y,p.z})word(std::bit_cast<std::uint32_t>(value));
    }
    return hash;
}
#if defined(AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT)
#if AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT != 1
#error "Entry layout pilot must be explicitly 1"
#endif
inline constexpr const EntryLayout& ActiveEntryLayout=WestSupplyPilotLayout;
#else
inline constexpr const EntryLayout& ActiveEntryLayout=OriginalEntryLayout;
#endif
inline constexpr bool OriginalEntryLayoutActive=ActiveEntryLayout.id==EntryLayoutId::Original;
// 固定原地形的一補給候選版號，未授予完整世界資格；改地形/作者必須另建epoch。
inline constexpr std::uint32_t ActiveEntryWorldEpoch=OriginalEntryLayoutActive?0u:0x45505731u;
inline constexpr Aevrynd::Vec3 SupplyApproach(unsigned index) {
    if(index>=ActiveEntryLayout.supplies.size())throw std::invalid_argument("Unknown supply approach");
    const auto p=ActiveEntryLayout.supplies[index];
    // 原完整森林rock26位於(-5.4724,5.8295)且不能刪除；候選從北側繞過，不改原站點路線。
    const float north=!OriginalEntryLayoutActive&&index==0?1.f:0.f;
    return p+Aevrynd::Vec3{p.x<0?2.f:-2.f,0,north};
}
}
