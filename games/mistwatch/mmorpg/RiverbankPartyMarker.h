#pragma once

#include "RiverbankPartyWire.h"
#include <Aevrynd/Camera.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace Mistwatch {
struct RiverbankPartyMarker {
    Aevrynd::Vec2 center{};
    float distance{};
    std::uint32_t actor{};
    RiverbankClass playerClass{};
    float healthFraction{};
    bool downed{}, showHealth{};
};

inline std::optional<RiverbankPartyMarker> MakeRiverbankPartyMarker(
    const RiverbankPartyWire::Member& member, std::uint32_t entityFlags,
    const Aevrynd::ProjectedPoint& point, float distance) {
    if (!point.visible || !std::isfinite(distance) || distance < 0.f || distance > 12.f ||
        !(point.position.x > 70.f && point.position.x < 1210.f &&
          point.position.y > 245.f && point.position.y < 552.f) ||
        !IsRiverbankClass(member.playerClass))
        return {};
    const bool downed = (entityFlags & RiverbankBattleWire::DownedEntityFlag) != 0;
    const auto fullHealth = CombatStatsFor(member.playerClass).health;
    // 世界快照用於倒地提示。若名單尚未收到復起後的生命值，暫時只顯示職業。
    const float fraction = float((std::min)(member.health, fullHealth)) / float(fullHealth);
    return RiverbankPartyMarker{point.position, distance, member.actor, member.playerClass,
                                fraction, downed, !downed && member.health != 0};
}

inline std::size_t ArrangeRiverbankPartyMarkers(std::span<RiverbankPartyMarker> markers) {
    std::ranges::sort(markers, [](const auto& a, const auto& b) {
        if (a.downed != b.downed) return a.downed;
        if (a.distance != b.distance) return a.distance < b.distance;
        return a.actor < b.actor;
    });
    std::size_t kept = 0;
    for (const auto marker : markers) {
        const auto collides = [&](const auto& placed) {
            return std::abs(marker.center.x - placed.center.x) < 96.f &&
                std::abs(marker.center.y - placed.center.y) < 32.f;
        };
        if (std::ranges::none_of(markers.first(kept), collides)) markers[kept++] = marker;
    }
    return kept;
}
}
