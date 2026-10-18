#pragma once
#include "MistwatchCoreHerbClusters.h"
#include <span>

namespace Mistwatch::UnderstoryRetention {
using namespace Aevrynd;
enum class Family : unsigned { ForestGrass,RoadsideBroadleaf,CoreHerb };
inline constexpr std::array<unsigned,3> Counts{3600,96,192};
inline constexpr unsigned Total=3888;
inline constexpr std::array<std::uint64_t,5> GrassSources{
    13676700745932842533ull,6708166984977766172ull,9741578749886758716ull,
    15330444761625695554ull,14455864278497895329ull};
inline constexpr std::array<std::uint64_t,2> NettleSources{5775871692697253043ull,3304028173848676471ull};
struct Row {
    Family family{};unsigned id{},variant{};
    Vec3 anchor{};Transform3D pose{};float footprintRadius{};bool suppressed{};
    std::uint64_t sourceFingerprint{},actualFingerprint{},handle{};
};
// 這是完整初始化作者資料，不是當幀opaque或GPU內容讀回；suppressed也保留以免後續選位漏算。
struct Summary {std::array<unsigned,3> counts{},suppressed{};std::array<std::uint64_t,3> fingerprints{};};
inline unsigned Slot(Family family){const auto slot=static_cast<unsigned>(family);
    if(slot>=3)throw std::invalid_argument("UNDERSTORY_RETENTION_FAMILY");return slot;
}
inline std::string_view Name(Family family){switch(family){
    case Family::ForestGrass:return "forest-grass";
    case Family::RoadsideBroadleaf:return "roadside-broadleaf";
    case Family::CoreHerb:return "core-herb";
    }throw std::invalid_argument("UNDERSTORY_RETENTION_FAMILY");
}
inline Summary Validate(std::span<const Row> rows){
    const auto need=[](bool ok,const char* why){if(!ok)throw std::invalid_argument(why);};
    need(rows.size()==Total,"UNDERSTORY_RETENTION_TOTAL");Summary out;
    out.fingerprints.fill(14695981039346656037ull);unsigned cursor{};
    for(unsigned family=0;family<3;++family)for(unsigned id=0;id<Counts[family];++id){const auto& r=rows[cursor++];
        const auto slot=Slot(r.family);need(slot==family&&r.id==id,"UNDERSTORY_RETENTION_ORDER");
        need(Session::Finite(r.anchor)&&Session::Finite(r.pose.position)&&Session::Finite(r.pose.rotation)&&Session::Finite(r.pose.scale)&&
            r.pose.rotation.x==0&&r.pose.rotation.z==0&&r.pose.scale.x>0&&r.pose.scale.x==r.pose.scale.y&&r.pose.scale.x==r.pose.scale.z&&
            std::isfinite(r.footprintRadius)&&r.footprintRadius>CoreHerbClusters::WindRadius&&r.footprintRadius<=4,
            "UNDERSTORY_RETENTION_POSE");
        need(r.anchor.x==r.pose.position.x&&r.anchor.z==r.pose.position.z,"UNDERSTORY_RETENTION_ANCHOR");
        need(r.handle&&r.actualFingerprint&&r.sourceFingerprint,"UNDERSTORY_RETENTION_RESOURCE");
        if(family==0){need(r.variant<5,"UNDERSTORY_RETENTION_VARIANT");
            need(r.sourceFingerprint==GrassSources[r.variant]&&r.actualFingerprint==r.sourceFingerprint,"UNDERSTORY_RETENTION_SOURCE");
            const float s=ForestCoverScale(r.anchor);
            need(r.pose.position.x==r.anchor.x&&r.pose.position.y==r.anchor.y&&r.pose.position.z==r.anchor.z&&
                r.pose.scale.x==s&&r.pose.rotation.y==r.anchor.x+r.anchor.z,"UNDERSTORY_RETENTION_GRASS_RENDER_POSE");
        }else{need(r.variant==2||r.variant==3,"UNDERSTORY_RETENTION_VARIANT");
            need(r.sourceFingerprint==NettleSources[r.variant-2],"UNDERSTORY_RETENTION_SOURCE");
            if(family==1)need(r.actualFingerprint==r.sourceFingerprint,"UNDERSTORY_RETENTION_SOURCE");
            else need(!r.suppressed,"UNDERSTORY_RETENTION_CORE_SUPPRESSION");
        }
        ++out.counts[slot];out.suppressed[slot]+=r.suppressed;
        auto& h=out.fingerprints[slot];namespace D=CoreHerbClusters::Detail;
        D::Word(h,slot);D::Word(h,r.id);D::Word(h,r.variant);
        for(float f:{r.anchor.x,r.anchor.y,r.anchor.z,r.pose.position.x,r.pose.position.y,r.pose.position.z,
                     r.pose.rotation.x,r.pose.rotation.y,r.pose.rotation.z,r.pose.scale.x,r.pose.scale.y,r.pose.scale.z,r.footprintRadius})D::Float(h,f);
        D::Word(h,r.suppressed?1:0);D::Wide(h,r.sourceFingerprint);D::Wide(h,r.actualFingerprint);
        // handle不納入內容封印：同內容的不同實機執行可以取得不同的allocation handle。
    }
    need(out.counts==Counts,"UNDERSTORY_RETENTION_COUNTS");return out;
}
}
