#pragma once

#include "RiverbankStructureMovement.h"
#include "RiverbankCrossing.h"
#include <array>
#include <optional>
#include <algorithm>
#include <cmath>

namespace Mistwatch {

enum class RiverbankStrikeExposure { Outside, Covered, Exposed };

inline RiverbankStrikeExposure StrikeExposureAt(bool field, ArenaPosition feet, ArenaPosition boss,
                                                ArenaPosition center, bool checksCover,
                                                ArenaPosition anchor = RiverbankEncounter::NorthBankCenter) {
    using Hunt = RiverbankEncounter;
    const auto radius = field ? Hunt::FieldStrikeRadius : Hunt::StrikeRadius;
    const bool inSite = field ? Hunt::InPasture(feet) : Hunt::InArena(feet, anchor);
    if (!inSite || !(std::hypot(feet.x - center.x, feet.z - center.z) <= radius))
        return RiverbankStrikeExposure::Outside;
    if (checksCover && !RiverbankStructureMovement::HasLine(boss, feet))
        return RiverbankStrikeExposure::Covered;
    return RiverbankStrikeExposure::Exposed;
}

inline std::optional<ArenaPosition> RiverbankStrikeExit(const RiverbankBossStatus& enemy, bool field,
                                                        ArenaPosition feet, bool checksCover) {
    using Hunt = RiverbankEncounter;
    if (enemy.phase != EncounterPhase::Windup || !enemy.phaseTicks ||
        StrikeExposureAt(field, feet, enemy.position, enemy.strikeCenter, checksCover) != RiverbankStrikeExposure::Exposed)
        return {};
    constexpr float d = .70710678f;
    constexpr std::array<ArenaPosition, 8> headings{{{0,1},{d,d},{1,0},{d,-d},
                                                      {0,-1},{-d,-d},{-1,0},{-d,d}}};
    constexpr float clearanceFromRing = .23f;
    const float radius = (field ? Hunt::FieldStrikeRadius : Hunt::StrikeRadius) + clearanceFromRing;
    const float dx = feet.x - enemy.strikeCenter.x, dz = feet.z - enemy.strikeCenter.z;
    const float offsetSquare = dx * dx + dz * dz;
    const float ox = feet.x - enemy.position.x, oz = feet.z - enemy.position.z;
    // 圓形外包線保留野豬和玩家完整碰撞寬度。若已擦到身體，只建議往外走。
    constexpr float body = Hunt::BoarBodyRadius + 1.41421356f * Hunt::PlayerContactHalfWidth;
    std::optional<ArenaPosition> best;
    float bestDistance = radius * 2.f;
    for (const auto heading : headings) {
        const float along = dx * heading.x + dz * heading.z;
        const float distance = std::sqrt(along * along + radius * radius - offsetSquare) - along;
        if (distance >= bestDistance) continue;
        const ArenaPosition step{heading.x * distance, heading.z * distance};
        const auto fromBoar = ox * heading.x + oz * heading.z;
        const float nearest = std::clamp(-fromBoar, 0.f, distance);
        const float xx = ox + heading.x * nearest, zz = oz + heading.z * nearest;
        const float endX = ox + step.x, endZ = oz + step.z;
        if (xx * xx + zz * zz < body * body &&
            (fromBoar <= 0.f || endX * endX + endZ * endZ < body * body)) continue;
        const auto clipped = RiverbankStructureMovement::ClipStep(feet, step);
        if (clipped.x != step.x || clipped.z != step.z) continue;
        const auto bank = RiverbankCrossing::LimitTerrainSegment(feet, step);
        if (bank.x != step.x || bank.z != step.z) continue;
        const auto ground = RiverbankCrossing::ClipStep(feet, step);
        if (ground.x != step.x || ground.z != step.z) continue;
        best = ArenaPosition{feet.x + step.x, feet.z + step.z};
        bestDistance = distance;
    }
    return best;
}

struct RiverbankStrikeCue {
    unsigned ticks{}, duration{};
    RiverbankStrikeExposure exposure{RiverbankStrikeExposure::Outside};
    bool withinCounterRange{}, clearCounterPath{};

    float Fraction() const {
        return duration ? static_cast<float>(ticks) / static_cast<float>(duration) : 0.f;
    }

    unsigned DisplayTenths() const {
        constexpr auto rate = RiverbankEncounter::TicksPerSecond;
        return (ticks * 10 + rate - 1) / rate;
    }
};

inline RiverbankStrikeCue DescribeRiverbankStrike(const RiverbankBossStatus& enemy, bool field, ArenaPosition feet,
                                                 bool checksCover, float attackRange) {
    using Hunt = RiverbankEncounter;
    const bool inSite = field ? Hunt::InPasture(feet) : Hunt::InArena(feet);
    if (!inSite || !enemy.phaseTicks) return {};
    if (enemy.phase == EncounterPhase::Windup) {
        const auto duration = field ? Hunt::FieldWindupTicks : Hunt::WindupTicks;
        return {(std::min)(duration, enemy.phaseTicks), duration,
                StrikeExposureAt(field, feet, enemy.position, enemy.strikeCenter, checksCover), false, false};
    }
    if (enemy.phase == EncounterPhase::Recovery) {
        const bool withinRange = std::hypot(feet.x - enemy.position.x, feet.z - enemy.position.z) <= attackRange;
        return {(std::min)(Hunt::RecoveryTicks, enemy.phaseTicks), Hunt::RecoveryTicks, RiverbankStrikeExposure::Outside,
                withinRange, withinRange && (!checksCover || RiverbankStructureMovement::HasLine(feet, enemy.position))};
    }
    return {};
}

}
