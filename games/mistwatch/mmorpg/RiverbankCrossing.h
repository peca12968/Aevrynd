#pragma once

#include "RiverbankEncounter.h"
#include "../riverbank/include/MistwatchBridgeDeck.h"
#include "../riverbank/include/MistwatchRiverProfile.h"
#include "../riverbank/include/MistwatchRiverbankBounds.h"

namespace Mistwatch::RiverbankCrossing {
inline constexpr float BridgeCenterLimit = RiverbankBridgeDeck.extents.x - .45f;

inline ArenaPosition TerrainRequest(ArenaPosition from, ArenaPosition delta) {
    using namespace RiverbankBounds;
    if (!std::isfinite(from.x) || !std::isfinite(from.z) || !std::isfinite(delta.x) || !std::isfinite(delta.z) ||
        from.x < WalkMinX || from.x > WalkMaxX || from.z < WalkMinZ || from.z > WalkMaxZ) return {};
    // 保留沿地圖邊緣的分量。
    const auto clip = [](float start, float step, float minimum, float maximum) {
        const float end = start + step;
        return (end < minimum || end > maximum) ? std::clamp(end, minimum, maximum) - start : step;
    };
    return {clip(from.x, delta.x, WalkMinX, WalkMaxX), clip(from.z, delta.z, WalkMinZ, WalkMaxZ)};
}

inline ArenaPosition LimitTerrainSegment(ArenaPosition from, ArenaPosition delta) {
    if (!std::isfinite(delta.x) || !std::isfinite(delta.z)) return {};
    const auto within = TerrainRequest(from, delta);
    const auto ratio = [](float accepted, float original) {
        return original == 0.f ? 1.f : std::clamp(accepted / original, 0.f, 1.f);
    };
    // 怪物碰撞可能改變位移方向。只縮短同一段，維持已驗證的側滑路徑。
    const float fraction = (std::min)(ratio(within.x, delta.x), ratio(within.z, delta.z));
    return {delta.x * fraction, delta.z * fraction};
}

inline bool Walkable(ArenaPosition point) {
    if (RiverDistance(point.x, point.z) >= RiverOuterWidth(point.x)) return true;
    const auto& deck = RiverbankBridgeDeck;
    return std::abs(point.x - deck.center.x) <= BridgeCenterLimit &&
        std::abs(point.z - deck.center.z) <= deck.extents.z;
}

inline ArenaPosition ClipStep(ArenaPosition from, ArenaPosition delta) {
    if (!std::isfinite(from.x) || !std::isfinite(from.z) || !Walkable(from)) return {};
    const auto length = std::hypot(delta.x, delta.z);
    if (!std::isfinite(length) || length == 0.f) return {};
    const auto at = [&](float fraction) {
        return ArenaPosition{from.x + delta.x * fraction, from.z + delta.z * fraction};
    };
    // 衝刺與斜向移動都檢查整段位移，避免跨過彎曲的河岸邊界。
    const unsigned count = static_cast<unsigned>(std::clamp(std::ceil(length / .025f), 1.f, 128.f));
    for (unsigned sample = 1; sample <= count; ++sample) {
        float upper = float(sample) / float(count);
        if (Walkable(at(upper))) continue;
        float lower = float(sample - 1) / float(count);
        for (unsigned split = 0; split < 8; ++split) {
            const float middle = (lower + upper) * .5f;
            if (Walkable(at(middle))) lower = middle;
            else upper = middle;
        }
        return {delta.x * lower, delta.z * lower};
    }
    return delta;
}

inline ArenaPosition ResolveStep(ArenaPosition from, ArenaPosition requested) {
    const auto direct = ClipStep(from, requested);
    if (direct.x == requested.x && direct.z == requested.z) return direct;
    // 比較兩條可直接同步給觀察者的側滑路徑。
    const auto alongX = ClipStep(from, {requested.x, 0.f});
    const auto alongZ = ClipStep(from, {0.f, requested.z});
    const auto progress = [&](ArenaPosition step) { return requested.x * step.x + requested.z * step.z; };
    const auto slide = progress(alongX) > progress(alongZ) ? alongX : alongZ;
    return progress(slide) > progress(direct) ? slide : direct;
}
}
