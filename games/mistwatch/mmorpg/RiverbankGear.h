#pragma once

#include "RiverbankClass.h"

namespace Mistwatch {
enum class GearAction : std::uint8_t { View, Equip, Unequip };
enum class GearResult : std::uint8_t { Ok, NotEarned, Rejected };

struct RiverbankGearState {
    bool earned{}, equipped{};
};

inline constexpr std::string_view BossGearName(RiverbankClass playerClass) {
    switch (playerClass) {
    case RiverbankClass::Guardian: return "北岸護腕";
    case RiverbankClass::Ranger: return "巡林弓弦";
    case RiverbankClass::Priest: return "河岸護符";
    default: return {};
    }
}

struct RiverbankBossGearStats {
    static constexpr unsigned GuardDamageTaken = 12;
    static constexpr unsigned VolleyDamage = 32;
    static constexpr unsigned HealAmount = 46;
};
}
