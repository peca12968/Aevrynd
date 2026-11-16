#pragma once

#include "RiverbankTerritory.h"
#include <string_view>

namespace Mistwatch {
enum class RiverbankTerritoryPhase { Unclaimed, Held, Capturing, Contested };

struct RiverbankTerritoryHud {
    RiverbankTerritoryPhase phase{};
    float fraction{};
    unsigned percent{};
    bool showProgress{};
};

inline RiverbankTerritoryHud ReadRiverbankTerritoryHud(const TerritoryState& state) {
    const auto phase = state.contested ? RiverbankTerritoryPhase::Contested :
        state.challenger ? RiverbankTerritoryPhase::Capturing :
        state.owner ? RiverbankTerritoryPhase::Held : RiverbankTerritoryPhase::Unclaimed;
    const auto ticks = state.challenger ? state.progress : 0u;
    return {phase, float(ticks) / RiverbankTerritory::HoldTicks, ticks * 100u / RiverbankTerritory::HoldTicks,
        phase == RiverbankTerritoryPhase::Capturing || phase == RiverbankTerritoryPhase::Contested};
}

inline std::string RiverbankTerritoryDisplayName(std::string_view name) {
    // 領地面板留出狀態與百分比的空間，保留完整 UTF-8 字元。
    unsigned characters = 0;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if ((static_cast<unsigned char>(name[i]) & 0xc0u) == 0x80u) continue;
        if (++characters == 18) return std::string(name.substr(0, i)) + "…";
    }
    return std::string(name);
}
}
