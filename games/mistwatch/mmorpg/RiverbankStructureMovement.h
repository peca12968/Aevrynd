#pragma once

#include "RiverbankEncounter.h"
#include "../riverbank/include/MistwatchRiverbankBounds.h"
#include "../riverbank/include/MistwatchStaticStructures.h"
#include <Aevrynd/Geometry3D.h>

namespace Mistwatch::RiverbankStructureMovement {
inline const std::vector<Aevrynd::BoxCollider3D>& Solids() {
    static const auto solids = [] {
        auto boxes = WorkshopSolids();
        boxes.push_back(FuseCaseSolid());
        const auto hut = RangerHutSolids();
        boxes.insert(boxes.end(), hut.begin(), hut.end());
        const auto shelter = RepairShelterSolids();
        boxes.insert(boxes.end(), shelter.begin(), shelter.end());
        return boxes;
    }();
    return solids;
}

inline bool HasLine(ArenaPosition from, ArenaPosition to) {
    constexpr float exchangeHeight = 1.05f;
    const Aevrynd::Vec3 start{from.x, GroundElevation(from.x,from.z) + exchangeHeight, from.z};
    const Aevrynd::Vec3 end{to.x, GroundElevation(to.x,to.z) + exchangeHeight, to.z};
    return std::ranges::none_of(Solids(), [&](const auto& solid) {
        return Aevrynd::SegmentIntersectsAabb3D(start,end,solid.center,solid.extents);
    });
}

inline constexpr Aevrynd::Vec3 PlayerBody{RiverbankBounds::BodyClearance,.9f,RiverbankBounds::BodyClearance};
inline bool CanStandInShelter(ArenaPosition feet) {
    static const auto roof = RepairShelterSolids().front();
    const float ground = GroundElevation(feet.x, feet.z);
    // 可以貼著屋簷站立。掃掠保留的水平間隙避免已站立角色自動蹲下。
    return std::abs(feet.x - roof.center.x) >= roof.extents.x + PlayerBody.x ||
        std::abs(feet.z - roof.center.z) >= roof.extents.z + PlayerBody.z ||
        ground + 2.f * PlayerBody.y < roof.center.y - roof.extents.y ||
        ground > roof.center.y + roof.extents.y;
}
inline ArenaPosition ClipStep(ArenaPosition from, ArenaPosition delta, Aevrynd::Vec3 body = PlayerBody) {
    if (!std::isfinite(from.x) || !std::isfinite(from.z) || !std::isfinite(delta.x) || !std::isfinite(delta.z) ||
        (delta.x == 0.f && delta.z == 0.f)) return {};
    constexpr float skin = .002f;
    const Aevrynd::Vec3 start{from.x, GroundElevation(from.x,from.z) + body.y, from.z};
    const auto x = from.x + delta.x, z = from.z + delta.z;
    const Aevrynd::Vec3 end{x, GroundElevation(x,z) + body.y, z};
    auto fraction = 1.f;
    for (const auto& solid : Solids()) {
        const auto volume = solid.extents + body + Aevrynd::Vec3{skin,0,skin};
        if (!Aevrynd::SegmentIntersectsAabb3D(start, end, solid.center, volume)) continue;
        const auto gap = [&](Aevrynd::Vec3 position) {
            const auto offset = position - solid.center;
            return (std::max)({std::abs(offset.x) - volume.x, std::abs(offset.y) - volume.y, std::abs(offset.z) - volume.z});
        };
        const auto initial = gap(start);
        if (initial <= 0.f) {
            // 允許意外載入在牆內的角色向外脫離。
            const auto finalGap = gap(end);
            if (initial < 0.f ? finalGap > initial : finalGap >= 0.f) continue;
            return {};
        }
        if (!Aevrynd::SegmentIntersectsAabb3D(start, start + (end-start)*fraction, solid.center, volume)) continue;
        float safe{}, blocked = fraction;
        for (unsigned iteration = 0; iteration < 12; ++iteration) {
            const auto center = (safe + blocked) * .5f;
            if (Aevrynd::SegmentIntersectsAabb3D(start, start + (end-start)*center, solid.center, volume)) blocked = center;
            else safe = center;
        }
        fraction = safe;
    }
    return {delta.x * fraction, delta.z * fraction};
}

inline ArenaPosition ResolveStep(ArenaPosition from, ArenaPosition request, Aevrynd::Vec3 body = PlayerBody) {
    const auto direct = ClipStep(from, request, body);
    if (direct.x == request.x && direct.z == request.z) return direct;
    // 伺服器快照之間維持一段直線，因此只選可獨立驗證的側滑方向。
    const auto x = ClipStep(from, {request.x, 0.f}, body);
    const auto z = ClipStep(from, {0.f, request.z}, body);
    const auto progress = [&](ArenaPosition point) { return point.x * request.x + point.z * request.z; };
    const auto slide = progress(x) > progress(z) ? x : z;
    return progress(slide) > progress(direct) ? slide : direct;
}
}
